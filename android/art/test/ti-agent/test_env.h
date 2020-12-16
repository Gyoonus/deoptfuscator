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

#ifndef ART_TEST_TI_AGENT_TEST_ENV_H_
#define ART_TEST_TI_AGENT_TEST_ENV_H_

#include "jvmti.h"

namespace art {

extern jvmtiEnv* jvmti_env;

// This is a jvmtiEventCallbacks struct that is used by all common ti-agent code whenever it calls
// SetEventCallbacks. This can be used by single tests to add additional event callbacks without
// being unable to use the rest of the ti-agent support code.
extern jvmtiEventCallbacks current_callbacks;

bool IsJVM();
void SetJVM(bool b);

}  // namespace art

#endif  // ART_TEST_TI_AGENT_TEST_ENV_H_
