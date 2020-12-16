/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "hidden_api.h"

#include <fstream>
#include <sstream>

#include "dex/dex_file-inl.h"

namespace art {

std::string HiddenApi::GetApiMethodName(const DexFile& dex_file, uint32_t method_index) {
  std::stringstream ss;
  const DexFile::MethodId& method_id = dex_file.GetMethodId(method_index);
  ss << dex_file.StringByTypeIdx(method_id.class_idx_)
     << "->"
     << dex_file.GetMethodName(method_id)
     << dex_file.GetMethodSignature(method_id).ToString();
  return ss.str();
}

std::string HiddenApi::GetApiFieldName(const DexFile& dex_file, uint32_t field_index) {
  std::stringstream ss;
  const DexFile::FieldId& field_id = dex_file.GetFieldId(field_index);
  ss << dex_file.StringByTypeIdx(field_id.class_idx_)
     << "->"
     << dex_file.GetFieldName(field_id)
     << ":"
     << dex_file.GetFieldTypeDescriptor(field_id);
  return ss.str();
}

void HiddenApi::FillList(const char* filename, std::set<std::string>& entries) {
  if (filename == nullptr) {
    return;
  }
  std::ifstream in(filename);
  std::string str;
  while (std::getline(in, str)) {
    entries.insert(str);
    size_t pos = str.find("->");
    if (pos != std::string::npos) {
      // Add the class name.
      entries.insert(str.substr(0, pos));
      pos = str.find('(');
      if (pos != std::string::npos) {
        // Add the class->method name (so stripping the signature).
        entries.insert(str.substr(0, pos));
      }
      pos = str.find(':');
      if (pos != std::string::npos) {
        // Add the class->field name (so stripping the type).
        entries.insert(str.substr(0, pos));
      }
    }
  }
}

}  // namespace art
