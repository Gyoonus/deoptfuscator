/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "elf_writer.h"

#include "base/unix_file/fd_file.h"
#include "elf_file.h"

namespace art {
namespace linker {

uintptr_t ElfWriter::GetOatDataAddress(ElfFile* elf_file) {
  uintptr_t oatdata_address = elf_file->FindSymbolAddress(SHT_DYNSYM,
                                                           "oatdata",
                                                           false);
  CHECK_NE(0U, oatdata_address);
  return oatdata_address;
}

void ElfWriter::GetOatElfInformation(File* file,
                                     size_t* oat_loaded_size,
                                     size_t* oat_data_offset) {
  std::string error_msg;
  std::unique_ptr<ElfFile> elf_file(ElfFile::Open(file,
                                                  false,
                                                  false,
                                                  /*low_4gb*/false,
                                                  &error_msg));
  CHECK(elf_file.get() != nullptr) << error_msg;

  bool success = elf_file->GetLoadedSize(oat_loaded_size, &error_msg);
  CHECK(success) << error_msg;
  CHECK_NE(0U, *oat_loaded_size);
  *oat_data_offset = GetOatDataAddress(elf_file.get());
  CHECK_NE(0U, *oat_data_offset);
}

bool ElfWriter::Fixup(File* file, uintptr_t oat_data_begin) {
  std::string error_msg;
  std::unique_ptr<ElfFile> elf_file(ElfFile::Open(file, true, false, /*low_4gb*/false, &error_msg));
  CHECK(elf_file.get() != nullptr) << error_msg;

  // Lookup "oatdata" symbol address.
  uintptr_t oatdata_address = ElfWriter::GetOatDataAddress(elf_file.get());
  uintptr_t base_address = oat_data_begin - oatdata_address;

  return elf_file->Fixup(base_address);
}

}  // namespace linker
}  // namespace art
