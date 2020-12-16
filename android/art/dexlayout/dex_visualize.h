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
 *
 * Header file of the dexlayout utility.
 *
 * This is a tool to read dex files into an internal representation,
 * reorganize the representation, and emit dex files with a better
 * file layout.
 */

#ifndef ART_DEXLAYOUT_DEX_VISUALIZE_H_
#define ART_DEXLAYOUT_DEX_VISUALIZE_H_

#include <stddef.h>

namespace art {

class DexFile;
class ProfileCompilationInfo;
namespace dex_ir {
class Header;
}  // namespace dex_ir

void VisualizeDexLayout(dex_ir::Header* header,
                        const DexFile* dex_file,
                        size_t dex_file_index,
                        ProfileCompilationInfo* profile_info);

void ShowDexSectionStatistics(dex_ir::Header* header, size_t dex_file_index);

}  // namespace art

#endif  // ART_DEXLAYOUT_DEX_VISUALIZE_H_
