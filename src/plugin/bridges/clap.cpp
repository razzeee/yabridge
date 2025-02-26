// yabridge: a Wine plugin bridge
// Copyright (C) 2020-2022 Robbert van der Helm
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "clap.h"

namespace fs = ghc::filesystem;

ClapPluginBridge::ClapPluginBridge(const ghc::filesystem::path& plugin_path)
    : PluginBridge(
          PluginType::clap,
          plugin_path,
          [](asio::io_context& io_context, const PluginInfo& info) {
              return ClapSockets<std::jthread>(
                  io_context,
                  generate_endpoint_base(info.native_library_path_.filename()
                                             .replace_extension("")
                                             .string()),
                  true);
          }),
      logger_(generic_logger_) {
    log_init_message();

    // This will block until all sockets have been connected to by the Wine VST
    // host
    connect_sockets_guarded();

    // Now that communication is set up the Wine host can send callbacks to this
    // bridge class, and we can send control messages to the Wine host. This
    // messaging mechanism is how we relay the CLAP communication protocol. As a
    // first thing, the Wine plugin host will ask us for a copy of the
    // configuration.
    host_callback_handler_ = std::jthread([&]() {
        set_realtime_priority(true);
        pthread_setname_np(pthread_self(), "host-callbacks");

        sockets_.plugin_host_main_thread_callback_.receive_messages(
            std::pair<ClapLogger&, bool>(logger_, false),
            overload{
                [&](const WantsConfiguration& request)
                    -> WantsConfiguration::Response {
                    warn_on_version_mismatch(request.host_version);

                    return config_;
                },
                [&](const clap::host::RequestRestart& request)
                    -> clap::host::RequestRestart::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy
                        .run_on_main_thread([host = plugin_proxy.host_]() {
                            host->request_restart(host);
                        })
                        .wait();

                    return Ack{};
                },
                [&](const clap::host::RequestProcess& request)
                    -> clap::host::RequestProcess::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy
                        .run_on_main_thread([host = plugin_proxy.host_]() {
                            host->request_process(host);
                        })
                        .wait();

                    return Ack{};
                },
                [&](const clap::ext::audio_ports::host::IsRescanFlagSupported&
                        request)
                    -> clap::ext::audio_ports::host::IsRescanFlagSupported::
                        Response {
                            const auto& [plugin_proxy, _] =
                                get_proxy(request.owner_instance_id);

                            return plugin_proxy
                                .run_on_main_thread(
                                    [&, host = plugin_proxy.host_,
                                     audio_ports = plugin_proxy.host_extensions_
                                                       .audio_ports]() {
                                        return audio_ports
                                            ->is_rescan_flag_supported(
                                                host, request.flag);
                                    })
                                .get();
                        },
                [&](const clap::ext::audio_ports::host::Rescan& request)
                    -> clap::ext::audio_ports::host::Rescan::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy
                        .run_on_main_thread(
                            [&, host = plugin_proxy.host_,
                             audio_ports =
                                 plugin_proxy.host_extensions_.audio_ports]() {
                                audio_ports->rescan(host, request.flags);
                            })
                        .wait();

                    return Ack{};
                },
                [&](const clap::ext::gui::host::ResizeHintsChanged& request)
                    -> clap::ext::gui::host::ResizeHintsChanged::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    // This callback is thread-safe
                    plugin_proxy.host_extensions_.gui->resize_hints_changed(
                        plugin_proxy.host_);

                    return Ack{};
                },
                [&](const clap::ext::gui::host::RequestResize& request)
                    -> clap::ext::gui::host::RequestResize::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    // This callback is thread-safe
                    return plugin_proxy.host_extensions_.gui->request_resize(
                        plugin_proxy.host_, request.width, request.height);
                },
                [&](const clap::ext::gui::host::RequestShow& request)
                    -> clap::ext::gui::host::RequestShow::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    // This callback is thread-safe
                    return plugin_proxy.host_extensions_.gui->request_show(
                        plugin_proxy.host_);
                },
                [&](const clap::ext::gui::host::RequestHide& request)
                    -> clap::ext::gui::host::RequestHide::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    // This callback is thread-safe
                    return plugin_proxy.host_extensions_.gui->request_hide(
                        plugin_proxy.host_);
                },
                [&](const clap::ext::gui::host::Closed& request)
                    -> clap::ext::gui::host::Closed::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    // This callback is thread-safe
                    plugin_proxy.host_extensions_.gui->closed(
                        plugin_proxy.host_, request.was_destroyed);

                    return Ack{};
                },
                [&](const clap::ext::latency::host::Changed& request)
                    -> clap::ext::latency::host::Changed::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy
                        .run_on_main_thread(
                            [&, host = plugin_proxy.host_,
                             latency =
                                 plugin_proxy.host_extensions_.latency]() {
                                latency->changed(host);
                            })
                        .wait();

                    return Ack{};
                },
                [&](const clap::ext::note_ports::host::SupportedDialects&
                        request)
                    -> clap::ext::note_ports::host::SupportedDialects::
                        Response {
                            const auto& [plugin_proxy, _] =
                                get_proxy(request.owner_instance_id);

                            return plugin_proxy
                                .run_on_main_thread(
                                    [host = plugin_proxy.host_,
                                     note_ports = plugin_proxy.host_extensions_
                                                      .note_ports]() {
                                        return note_ports->supported_dialects(
                                            host);
                                    })
                                .get();
                        },
                [&](const clap::ext::note_ports::host::Rescan& request)
                    -> clap::ext::note_ports::host::Rescan::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy
                        .run_on_main_thread(
                            [&, host = plugin_proxy.host_,
                             note_ports =
                                 plugin_proxy.host_extensions_.note_ports]() {
                                note_ports->rescan(host, request.flags);
                            })
                        .wait();

                    return Ack{};
                },
                [&](const clap::ext::params::host::Rescan& request)
                    -> clap::ext::params::host::Rescan::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy
                        .run_on_main_thread(
                            [&, host = plugin_proxy.host_,
                             params = plugin_proxy.host_extensions_.params]() {
                                params->rescan(host, request.flags);
                            })
                        .wait();

                    return Ack{};
                },
                [&](const clap::ext::params::host::Clear& request)
                    -> clap::ext::params::host::Clear::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy
                        .run_on_main_thread(
                            [&, host = plugin_proxy.host_,
                             params = plugin_proxy.host_extensions_.params]() {
                                params->clear(host, request.param_id,
                                              request.flags);
                            })
                        .wait();

                    return Ack{};
                },
                [&](const clap::ext::state::host::MarkDirty& request)
                    -> clap::ext::state::host::MarkDirty::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy
                        .run_on_main_thread(
                            [&, host = plugin_proxy.host_,
                             state = plugin_proxy.host_extensions_.state]() {
                                state->mark_dirty(host);
                            })
                        .wait();

                    return Ack{};
                },
                [&](const clap::ext::voice_info::host::Changed& request)
                    -> clap::ext::voice_info::host::Changed::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy
                        .run_on_main_thread(
                            [&, host = plugin_proxy.host_,
                             voice_info =
                                 plugin_proxy.host_extensions_.voice_info]() {
                                voice_info->changed(host);
                            })
                        .wait();

                    return Ack{};
                },
            });
    });
}

ClapPluginBridge::~ClapPluginBridge() noexcept {
    try {
        // Drop all work make sure all sockets are closed
        plugin_host_->terminate();
        io_context_.stop();
    } catch (const std::system_error&) {
        // It could be that the sockets have already been closed or that the
        // process has already exited (at which point we probably won't be
        // executing this, but maybe if all the stars align)
    }
}

const void* ClapPluginBridge::get_factory(const char* factory_id) {
    assert(factory_id);

    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        // We'll initialize the factory the first time it's requested
        if (!plugin_factory_) {
            // If the plugin does not support this factory type, then we'll also
            // return a null poitner
            const clap::plugin_factory::ListResponse response =
                send_main_thread_message(clap::plugin_factory::List{});
            if (!response.descriptors) {
                return nullptr;
            }

            plugin_factory_ = std::make_unique<clap_plugin_factory_proxy>(
                *this, std::move(*response.descriptors));
        }

        return &plugin_factory_->plugin_factory_vtable;
    } else {
        logger_.log_trace([factory_id]() {
            return "Unknown factory type '" + std::string(factory_id) + "'";
        });
        return nullptr;
    }
}

std::pair<clap_plugin_proxy&, std::shared_lock<std::shared_mutex>>
ClapPluginBridge::get_proxy(size_t instance_id) noexcept {
    std::shared_lock lock(plugin_proxies_mutex_);

    return std::pair<clap_plugin_proxy&, std::shared_lock<std::shared_mutex>>(
        *plugin_proxies_.at(instance_id), std::move(lock));
}

void ClapPluginBridge::register_plugin_proxy(
    std::unique_ptr<clap_plugin_proxy> plugin_proxy) {
    std::unique_lock lock(plugin_proxies_mutex_);

    assert(plugin_proxy);

    const size_t instance_id = plugin_proxy->instance_id();
    plugin_proxies_.emplace(instance_id, std::move(plugin_proxy));

    // For optimization reaons we use dedicated sockets for functions that will
    // be run in the audio processing loop.
    // Every plugin instance gets its own audio thread along with sockets for
    // host->plugin control messages and plugin->host callbacks
    std::promise<void> socket_listening_latch;
    plugin_proxies_.at(instance_id)
        ->audio_thread_handler_ = std::jthread([&, instance_id]() {
        set_realtime_priority(true);

        // XXX: Like with VST2 worker threads, when using plugin groups the
        //      thread names from different plugins will clash. Not a huge
        //      deal probably, since duplicate thread names are still more
        //      useful than no thread names.
        const std::string thread_name = "audio-" + std::to_string(instance_id);
        pthread_setname_np(pthread_self(), thread_name.c_str());

        // Certain CLAP extensions allow audio thread callbacks, so we need
        // a dedicated per-instance socket for that
        sockets_.add_audio_thread_and_listen_callback(
            instance_id, logger_, socket_listening_latch,
            overload{
                [&](const WantsConfiguration&) -> WantsConfiguration::Response {
                    // FIXME: Without starting the variant with
                    //        `WantsConfiguration` you enter template deduction
                    //        hell. I haven't been able to figure out why.
                    return {};
                },
                [&](const clap::ext::log::host::Log& request)
                    -> clap::ext::log::host::Log::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy.host_extensions_.log->log(plugin_proxy.host_,
                                                           request.severity,
                                                           request.msg.c_str());

                    return Ack{};
                },
                [&](const clap::ext::params::host::RequestFlush& request)
                    -> clap::ext::params::host::RequestFlush::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy.host_extensions_.params->request_flush(
                        plugin_proxy.host_);

                    return Ack{};
                },
                [&](const clap::ext::tail::host::Changed& request)
                    -> clap::ext::tail::host::Changed::Response {
                    const auto& [plugin_proxy, _] =
                        get_proxy(request.owner_instance_id);

                    plugin_proxy.host_extensions_.tail->changed(
                        plugin_proxy.host_);

                    return Ack{};
                },
            });
    });

    // Wait for the new socket to be listening on before continuing. Otherwise
    // the native plugin may try to connect to it before our thread is up and
    // running.
    socket_listening_latch.get_future().wait();
}

void ClapPluginBridge::unregister_plugin_proxy(size_t instance_id) {
    std::lock_guard lock(plugin_proxies_mutex_);

    plugin_proxies_.erase(instance_id);
    sockets_.remove_audio_thread(instance_id);
}
