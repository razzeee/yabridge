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

#include "context-menu-target.h"

YaContextMenuTarget::YaContextMenuTarget(ConstructArgs&& args) noexcept
    : arguments_(std::move(args)){FUNKNOWN_CTOR}

      YaContextMenuTarget::~YaContextMenuTarget() noexcept {FUNKNOWN_DTOR}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
      IMPLEMENT_FUNKNOWN_METHODS(YaContextMenuTarget,
                                 Steinberg::Vst::IContextMenuTarget,
                                 Steinberg::Vst::IContextMenuTarget::iid)
#pragma GCC diagnostic pop
