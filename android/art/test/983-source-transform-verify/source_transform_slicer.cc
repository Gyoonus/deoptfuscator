/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "source_transform.h"

#pragma clang diagnostic push
// Slicer's headers have code that triggers these warnings. b/65298177
#pragma clang diagnostic ignored "-Wsign-compare"
#include "slicer/reader.h"

#pragma clang diagnostic pop

namespace art {
namespace Test983SourceTransformVerify {

// The hook we are using.
void VerifyClassData(jint class_data_len, const unsigned char* class_data) {
  dex::Reader reader(class_data, class_data_len);
  reader.CreateFullIr();  // This will verify all bytecode.
}

}  // namespace Test983SourceTransformVerify
}  // namespace art
