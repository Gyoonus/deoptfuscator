/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "shadow_frame.h"

#include "art_method-inl.h"

namespace art {

mirror::Object* ShadowFrame::GetThisObject() const {
  ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return nullptr;
  } else if (m->IsNative()) {
    return GetVRegReference(0);
  } else {
    CHECK(m->GetCodeItem() != nullptr) << ArtMethod::PrettyMethod(m);
    CodeItemDataAccessor accessor(m->DexInstructionData());
    uint16_t reg = accessor.RegistersSize() - accessor.InsSize();
    return GetVRegReference(reg);
  }
}

mirror::Object* ShadowFrame::GetThisObject(uint16_t num_ins) const {
  ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return nullptr;
  } else {
    return GetVRegReference(NumberOfVRegs() - num_ins);
  }
}

}  // namespace art
