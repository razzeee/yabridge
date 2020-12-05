// yabridge: a Wine VST bridge
// Copyright (C) 2020  Robbert van der Helm
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

#include "plugin-factory.h"

YaPluginFactory::YaPluginFactory(){FUNKNOWN_CTOR}

YaPluginFactory::YaPluginFactory(
    Steinberg::IPtr<Steinberg::IPluginFactory> factory) {
    FUNKNOWN_CTOR

    known_iids.insert(factory->iid);
    // `IPluginFactory::getFactoryInfo`
    if (Steinberg::PFactoryInfo info;
        factory->getFactoryInfo(&info) == Steinberg::kResultOk) {
        factory_info = info;
    }
    // `IPluginFactory::countClasses`
    num_classes = factory->countClasses();
    // `IPluginFactory::getClassInfo`
    // TODO: At this point we don't know what this class is and thus we can't
    //       filter unsupported classes, right?
    class_infos_1.resize(num_classes);
    for (int i = 0; i < num_classes; i++) {
        Steinberg::PClassInfo info;
        if (factory->getClassInfo(i, &info) == Steinberg::kResultOk) {
            class_infos_1[i] = info;
        }
    }

    auto factory2 = Steinberg::FUnknownPtr<Steinberg::IPluginFactory2>(factory);
    if (!factory2) {
        return;
    }

    known_iids.insert(factory2->iid);
    // `IpluginFactory2::getClassInfo2`
    for (int i = 0; i < num_classes; i++) {
        Steinberg::PClassInfo2 info;
        if (factory2->getClassInfo2(i, &info) == Steinberg::kResultOk) {
            class_infos_2[i] = info;
        }
    }

    auto factory3 = Steinberg::FUnknownPtr<Steinberg::IPluginFactory3>(factory);
    if (!factory3) {
        return;
    }

    // TODO: Copy data from `IPluginFactory3`
    known_iids.insert(factory3->iid);
}

YaPluginFactory::~YaPluginFactory() {
    FUNKNOWN_DTOR
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
IMPLEMENT_REFCOUNT(YaPluginFactory)
#pragma GCC diagnostic pop

tresult PLUGIN_API YaPluginFactory::queryInterface(Steinberg::FIDString _iid,
                                                   void** obj) {
    QUERY_INTERFACE(_iid, obj, Steinberg::FUnknown::iid,
                    Steinberg::IPluginFactory)
    if (known_iids.contains(Steinberg::IPluginFactory::iid)) {
        QUERY_INTERFACE(_iid, obj, Steinberg::IPluginFactory::iid,
                        Steinberg::IPluginFactory)
    }
    if (known_iids.contains(Steinberg::IPluginFactory2::iid)) {
        QUERY_INTERFACE(_iid, obj, Steinberg::IPluginFactory2::iid,
                        Steinberg::IPluginFactory2)
    }
    if (known_iids.contains(Steinberg::IPluginFactory3::iid)) {
        QUERY_INTERFACE(_iid, obj, Steinberg::IPluginFactory3::iid,
                        Steinberg::IPluginFactory3)
    }

    *obj = nullptr;
    return Steinberg::kNoInterface;
}

tresult PLUGIN_API
YaPluginFactory::getFactoryInfo(Steinberg::PFactoryInfo* info) {
    if (info && factory_info) {
        *info = *factory_info;
        return Steinberg::kResultOk;
    } else {
        return Steinberg::kNotInitialized;
    }
}

int32 PLUGIN_API YaPluginFactory::countClasses() {
    return num_classes;
}

tresult PLUGIN_API YaPluginFactory::getClassInfo(Steinberg::int32 index,
                                                 Steinberg::PClassInfo* info) {
    if (index >= static_cast<int32>(class_infos_1.size())) {
        return Steinberg::kInvalidArgument;
    }

    if (class_infos_1[index]) {
        *info = *class_infos_1[index];
        return Steinberg::kResultOk;
    } else {
        return Steinberg::kResultFalse;
    }
}

tresult PLUGIN_API
YaPluginFactory::getClassInfo2(int32 index, Steinberg::PClassInfo2* info) {
    if (index >= static_cast<int32>(class_infos_1.size())) {
        return Steinberg::kInvalidArgument;
    }

    if (class_infos_2[index]) {
        *info = *class_infos_2[index];
        return Steinberg::kResultOk;
    } else {
        return Steinberg::kResultFalse;
    }
}

tresult PLUGIN_API
YaPluginFactory::getClassInfoUnicode(int32 /*index*/,
                                     Steinberg::PClassInfoW* /*info*/) {
    // TODO: Implement
    return 0;
}
