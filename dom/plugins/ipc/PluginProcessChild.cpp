/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/plugins/PluginProcessChild.h"

#include "prlink.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "nsDebugImpl.h"

#ifdef XP_WIN
bool ShouldProtectPluginCurrentDirectory(char16ptr_t pluginFilePath);
#endif

using mozilla::ipc::IOThreadChild;

#ifdef OS_WIN
#include "nsSetDllDirectory.h"
#include <algorithm>
#endif

namespace mozilla {
namespace plugins {


bool
PluginProcessChild::Init()
{
    nsDebugImpl::SetMultiprocessMode("NPAPI");

    // Certain plugins, such as flash, steal the unhandled exception filter
    // thus we never get crash reports when they fault. This call fixes it.
    message_loop()->set_exception_restoration(true);

    std::string pluginFilename;

#if defined(OS_POSIX)
    // NB: need to be very careful in ensuring that the first arg
    // (after the binary name) here is indeed the plugin module path.
    // Keep in sync with dom/plugins/PluginModuleParent.
    std::vector<std::string> values = CommandLine::ForCurrentProcess()->argv();
    MOZ_ASSERT(values.size() >= 2, "not enough args");

    pluginFilename = UnmungePluginDsoPath(values[1]);

#elif defined(OS_WIN)
    std::vector<std::wstring> values =
        CommandLine::ForCurrentProcess()->GetLooseValues();
    MOZ_ASSERT(values.size() >= 1, "not enough loose args");

    if (ShouldProtectPluginCurrentDirectory(values[0].c_str())) {
        SanitizeEnvironmentVariables();
        SetDllDirectory(L"");
    }

    pluginFilename = WideToUTF8(values[0]);

#else
#  error Sorry
#endif

    if (NS_FAILED(nsRegion::InitStatic())) {
      NS_ERROR("Could not initialize nsRegion");
      return false;
    }

    bool retval = mPlugin.InitForChrome(pluginFilename, ParentPid(),
                                        IOThreadChild::message_loop(),
                                        IOThreadChild::channel());
    return retval;
}

void
PluginProcessChild::CleanUp()
{
    nsRegion::ShutdownStatic();
}

} // namespace plugins
} // namespace mozilla
