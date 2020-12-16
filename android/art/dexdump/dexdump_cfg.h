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

#ifndef ART_DEXDUMP_DEXDUMP_CFG_H_
#define ART_DEXDUMP_DEXDUMP_CFG_H_

#include <inttypes.h>
#include <ostream>

namespace art {

class DexFile;

void DumpMethodCFG(const DexFile* dex_file, uint32_t dex_method_idx, std::ostream& os);

}  // namespace art

#endif  // ART_DEXDUMP_DEXDUMP_CFG_H_
