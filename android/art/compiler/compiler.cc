/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "compiler.h"

#include <android-base/logging.h>

#include "base/macros.h"
#include "base/utils.h"
#include "dex/code_item_accessors-inl.h"
#include "driver/compiler_driver.h"
#include "optimizing/optimizing_compiler.h"

namespace art {

Compiler* Compiler::Create(CompilerDriver* driver, Compiler::Kind kind) {
  switch (kind) {
    case kQuick:
      // TODO: Remove Quick in options.
    case kOptimizing:
      return CreateOptimizingCompiler(driver);

    default:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }
}

bool Compiler::IsPathologicalCase(const DexFile::CodeItem& code_item,
                                  uint32_t method_idx,
                                  const DexFile& dex_file) {
  /*
   * Skip compilation for pathologically large methods - either by instruction count or num vregs.
   * Dalvik uses 16-bit uints for instruction and register counts.  We'll limit to a quarter
   * of that, which also guarantees we cannot overflow our 16-bit internal Quick SSA name space.
   */
  CodeItemDataAccessor accessor(dex_file, &code_item);
  if (accessor.InsnsSizeInCodeUnits() >= UINT16_MAX / 4) {
    LOG(INFO) << "Method exceeds compiler instruction limit: "
              << accessor.InsnsSizeInCodeUnits()
              << " in " << dex_file.PrettyMethod(method_idx);
    return true;
  }
  if (accessor.RegistersSize() >= UINT16_MAX / 4) {
    LOG(INFO) << "Method exceeds compiler virtual register limit: "
              << accessor.RegistersSize() << " in " << dex_file.PrettyMethod(method_idx);
    return true;
  }
  return false;
}

}  // namespace art
