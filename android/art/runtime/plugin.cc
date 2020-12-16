/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "plugin.h"

#include <dlfcn.h>

#include "android-base/stringprintf.h"

namespace art {

using android::base::StringPrintf;

const char* PLUGIN_INITIALIZATION_FUNCTION_NAME = "ArtPlugin_Initialize";
const char* PLUGIN_DEINITIALIZATION_FUNCTION_NAME = "ArtPlugin_Deinitialize";

Plugin::Plugin(const Plugin& other) : library_(other.library_), dlopen_handle_(nullptr) {
  CHECK(!other.IsLoaded()) << "Should not copy loaded plugins.";
}

bool Plugin::Load(/*out*/std::string* error_msg) {
  DCHECK(!IsLoaded());
  void* res = dlopen(library_.c_str(), RTLD_LAZY);
  if (res == nullptr) {
    *error_msg = StringPrintf("dlopen failed: %s", dlerror());
    return false;
  }
  // Get the initializer function
  PluginInitializationFunction init = reinterpret_cast<PluginInitializationFunction>(
      dlsym(res, PLUGIN_INITIALIZATION_FUNCTION_NAME));
  if (init != nullptr) {
    if (!init()) {
      dlclose(res);
      *error_msg = StringPrintf("Initialization of plugin failed");
      return false;
    }
  } else {
    LOG(WARNING) << this << " does not include an initialization function";
  }
  dlopen_handle_ = res;
  return true;
}

bool Plugin::Unload() {
  DCHECK(IsLoaded());
  bool ret = true;
  void* handle = dlopen_handle_;
  PluginDeinitializationFunction deinit = reinterpret_cast<PluginDeinitializationFunction>(
      dlsym(handle, PLUGIN_DEINITIALIZATION_FUNCTION_NAME));
  if (deinit != nullptr) {
    if (!deinit()) {
      LOG(WARNING) << this << " failed deinitialization";
      ret = false;
    }
  } else {
    LOG(WARNING) << this << " does not include a deinitialization function";
  }
  dlopen_handle_ = nullptr;
  // Don't bother to actually dlclose since we are shutting down anyway and there might be small
  // amounts of processing still being done.
  return ret;
}

std::ostream& operator<<(std::ostream &os, const Plugin* m) {
  return os << *m;
}

std::ostream& operator<<(std::ostream &os, Plugin const& m) {
  return os << "Plugin { library=\"" << m.library_ << "\", handle=" << m.dlopen_handle_ << " }";
}

}  // namespace art
