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

#ifndef ART_COMPILER_OPTIMIZING_OPTIMIZING_COMPILER_H_
#define ART_COMPILER_OPTIMIZING_OPTIMIZING_COMPILER_H_

#include "base/mutex.h"
#include "globals.h"

namespace art {

class ArtMethod;
class Compiler;
class CompilerDriver;
class DexFile;

Compiler* CreateOptimizingCompiler(CompilerDriver* driver);

// Returns whether we are compiling against a "core" image, which
// is an indicative we are running tests. The compiler will use that
// information for checking invariants.
bool IsCompilingWithCoreImage();

bool EncodeArtMethodInInlineInfo(ArtMethod* method);
bool CanEncodeInlinedMethodInStackMap(const DexFile& caller_dex_file, ArtMethod* callee)
      REQUIRES_SHARED(Locks::mutator_lock_);

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_OPTIMIZING_COMPILER_H_
