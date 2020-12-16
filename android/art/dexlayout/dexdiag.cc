/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <memory>
#include <vector>

#include "android-base/stringprintf.h"

#include "base/logging.h"  // For InitLogging.
#include "base/mutex.h"
#include "base/stringpiece.h"

#include "dexlayout.h"
#include "dex/dex_file.h"
#include "dex_ir.h"
#include "dex_ir_builder.h"
#ifdef ART_TARGET_ANDROID
#include "pagemap/pagemap.h"
#endif
#include "runtime.h"
#include "vdex_file.h"

namespace art {

using android::base::StringPrintf;

static bool g_verbose = false;

// The width needed to print a file page offset (32-bit).
static constexpr int kPageCountWidth =
    static_cast<int>(std::numeric_limits<uint32_t>::digits10);
// Display the sections.
static constexpr char kSectionHeader[] = "Section name";

struct DexSectionInfo {
 public:
  std::string name;
  char letter;
};

static const std::map<uint16_t, DexSectionInfo> kDexSectionInfoMap = {
  { DexFile::kDexTypeHeaderItem, { "Header", 'H' } },
  { DexFile::kDexTypeStringIdItem, { "StringId", 'S' } },
  { DexFile::kDexTypeTypeIdItem, { "TypeId", 'T' } },
  { DexFile::kDexTypeProtoIdItem, { "ProtoId", 'P' } },
  { DexFile::kDexTypeFieldIdItem, { "FieldId", 'F' } },
  { DexFile::kDexTypeMethodIdItem, { "MethodId", 'M' } },
  { DexFile::kDexTypeClassDefItem, { "ClassDef", 'C' } },
  { DexFile::kDexTypeCallSiteIdItem, { "CallSiteId", 'z' } },
  { DexFile::kDexTypeMethodHandleItem, { "MethodHandle", 'Z' } },
  { DexFile::kDexTypeMapList, { "TypeMap", 'L' } },
  { DexFile::kDexTypeTypeList, { "TypeList", 't' } },
  { DexFile::kDexTypeAnnotationSetRefList, { "AnnotationSetReferenceItem", '1' } },
  { DexFile::kDexTypeAnnotationSetItem, { "AnnotationSetItem", '2' } },
  { DexFile::kDexTypeClassDataItem, { "ClassData", 'c' } },
  { DexFile::kDexTypeCodeItem, { "CodeItem", 'X' } },
  { DexFile::kDexTypeStringDataItem, { "StringData", 's' } },
  { DexFile::kDexTypeDebugInfoItem, { "DebugInfo", 'D' } },
  { DexFile::kDexTypeAnnotationItem, { "AnnotationItem", '3' } },
  { DexFile::kDexTypeEncodedArrayItem, { "EncodedArrayItem", 'E' } },
  { DexFile::kDexTypeAnnotationsDirectoryItem, { "AnnotationsDirectoryItem", '4' } }
};

class PageCount {
 public:
  PageCount() {
    for (auto it = kDexSectionInfoMap.begin(); it != kDexSectionInfoMap.end(); ++it) {
      map_[it->first] = 0;
    }
  }
  void Increment(uint16_t type) {
    map_[type]++;
  }
  size_t Get(uint16_t type) const {
    return map_.at(type);
  }
 private:
  std::map<uint16_t, size_t> map_;
  DISALLOW_COPY_AND_ASSIGN(PageCount);
};

class Printer {
 public:
  Printer() : section_header_width_(ComputeHeaderWidth()) {
  }

  void PrintHeader() const {
    std::cout << StringPrintf("%-*s %*s %*s %% of   %% of",
                              section_header_width_,
                              kSectionHeader,
                              kPageCountWidth,
                              "resident",
                              kPageCountWidth,
                              "total"
                              )
              << std::endl;
    std::cout << StringPrintf("%-*s %*s %*s sect.  total",
                              section_header_width_,
                              "",
                              kPageCountWidth,
                              "pages",
                              kPageCountWidth,
                              "pages")
              << std::endl;
  }

  void PrintOne(const char* name,
                size_t resident,
                size_t mapped,
                double percent_of_section,
                double percent_of_total) const {
    // 6.2 is sufficient to print 0-100% with two decimal places of accuracy.
    std::cout << StringPrintf("%-*s %*zd %*zd %6.2f %6.2f",
                              section_header_width_,
                              name,
                              kPageCountWidth,
                              resident,
                              kPageCountWidth,
                              mapped,
                              percent_of_section,
                              percent_of_total)
              << std::endl;
  }

  void PrintSkipLine() const { std::cout << std::endl; }

  // Computes the width of the section header column in the table (for fixed formatting).
  static int ComputeHeaderWidth() {
    int header_width = 0;
    for (const auto& pair : kDexSectionInfoMap) {
      const DexSectionInfo& section_info = pair.second;
      header_width = std::max(header_width, static_cast<int>(section_info.name.length()));
    }
    return header_width;
  }

 private:
  const int section_header_width_;
};

static void PrintLetterKey() {
  std::cout << "L pagetype" << std::endl;
  for (const auto& pair : kDexSectionInfoMap) {
    const DexSectionInfo& section_info = pair.second;
    std::cout << section_info.letter << " " << section_info.name.c_str() << std::endl;
  }
  std::cout << "* (Executable page resident)" << std::endl;
  std::cout << ". (Mapped page not resident)" << std::endl;
}

#ifdef ART_TARGET_ANDROID
static char PageTypeChar(uint16_t type) {
  if (kDexSectionInfoMap.find(type) == kDexSectionInfoMap.end()) {
    return '-';
  }
  return kDexSectionInfoMap.find(type)->second.letter;
}

static uint16_t FindSectionTypeForPage(size_t page,
                                       const std::vector<dex_ir::DexFileSection>& sections) {
  for (const auto& section : sections) {
    size_t first_page_of_section = section.offset / kPageSize;
    // Only consider non-empty sections.
    if (section.size == 0) {
      continue;
    }
    // Attribute the page to the highest-offset section that starts before the page.
    if (first_page_of_section <= page) {
      return section.type;
    }
  }
  // If there's no non-zero sized section with an offset below offset we're looking for, it
  // must be the header.
  return DexFile::kDexTypeHeaderItem;
}

static void ProcessPageMap(uint64_t* pagemap,
                           size_t start,
                           size_t end,
                           const std::vector<dex_ir::DexFileSection>& sections,
                           PageCount* page_counts) {
  static constexpr size_t kLineLength = 32;
  for (size_t page = start; page < end; ++page) {
    char type_char = '.';
    if (PM_PAGEMAP_PRESENT(pagemap[page])) {
      const size_t dex_page_offset = page - start;
      uint16_t type = FindSectionTypeForPage(dex_page_offset, sections);
      page_counts->Increment(type);
      type_char = PageTypeChar(type);
    }
    if (g_verbose) {
      std::cout << type_char;
      if ((page - start) % kLineLength == kLineLength - 1) {
        std::cout << std::endl;
      }
    }
  }
  if (g_verbose) {
    if ((end - start) % kLineLength != 0) {
      std::cout << std::endl;
    }
  }
}

static void DisplayDexStatistics(size_t start,
                                 size_t end,
                                 const PageCount& resident_pages,
                                 const std::vector<dex_ir::DexFileSection>& sections,
                                 Printer* printer) {
  // Compute the total possible sizes for sections.
  PageCount mapped_pages;
  DCHECK_GE(end, start);
  size_t total_mapped_pages = end - start;
  if (total_mapped_pages == 0) {
    return;
  }
  for (size_t page = start; page < end; ++page) {
    const size_t dex_page_offset = page - start;
    mapped_pages.Increment(FindSectionTypeForPage(dex_page_offset, sections));
  }
  size_t total_resident_pages = 0;
  printer->PrintHeader();
  for (size_t i = sections.size(); i > 0; --i) {
    const dex_ir::DexFileSection& section = sections[i - 1];
    const uint16_t type = section.type;
    const DexSectionInfo& section_info = kDexSectionInfoMap.find(type)->second;
    size_t pages_resident = resident_pages.Get(type);
    double percent_resident = 0;
    if (mapped_pages.Get(type) > 0) {
      percent_resident = 100.0 * pages_resident / mapped_pages.Get(type);
    }
    printer->PrintOne(section_info.name.c_str(),
                      pages_resident,
                      mapped_pages.Get(type),
                      percent_resident,
                      100.0 * pages_resident / total_mapped_pages);
    total_resident_pages += pages_resident;
  }
  double percent_of_total = 100.0 * total_resident_pages / total_mapped_pages;
  printer->PrintOne("GRAND TOTAL",
                    total_resident_pages,
                    total_mapped_pages,
                    percent_of_total,
                    percent_of_total);
  printer->PrintSkipLine();
}

static void ProcessOneDexMapping(uint64_t* pagemap,
                                 uint64_t map_start,
                                 const DexFile* dex_file,
                                 uint64_t vdex_start,
                                 Printer* printer) {
  uint64_t dex_file_start = reinterpret_cast<uint64_t>(dex_file->Begin());
  size_t dex_file_size = dex_file->Size();
  if (dex_file_start < vdex_start) {
    std::cerr << "Dex file start offset for "
              << dex_file->GetLocation().c_str()
              << " is incorrect: map start "
              << StringPrintf("%" PRIx64 " > dex start %" PRIx64 "\n", map_start, dex_file_start)
              << std::endl;
    return;
  }
  uint64_t start_page = (dex_file_start - vdex_start) / kPageSize;
  uint64_t start_address = start_page * kPageSize;
  uint64_t end_page = RoundUp(start_address + dex_file_size, kPageSize) / kPageSize;
  std::cout << "DEX "
            << dex_file->GetLocation().c_str()
            << StringPrintf(": %" PRIx64 "-%" PRIx64,
                            map_start + start_page * kPageSize,
                            map_start + end_page * kPageSize)
            << std::endl;
  // Build a list of the dex file section types, sorted from highest offset to lowest.
  std::vector<dex_ir::DexFileSection> sections;
  {
    Options options;
    std::unique_ptr<dex_ir::Header> header(dex_ir::DexIrBuilder(*dex_file,
                                                                /*eagerly_assign_offsets*/ true,
                                                                options));
    sections = dex_ir::GetSortedDexFileSections(header.get(),
                                                dex_ir::SortDirection::kSortDescending);
  }
  PageCount section_resident_pages;
  ProcessPageMap(pagemap, start_page, end_page, sections, &section_resident_pages);
  DisplayDexStatistics(start_page, end_page, section_resident_pages, sections, printer);
}

static bool IsVdexFileMapping(const std::string& mapped_name) {
  // Confirm that the map is from a vdex file.
  static const char* suffixes[] = { ".vdex" };
  for (const char* suffix : suffixes) {
    size_t match_loc = mapped_name.find(suffix);
    if (match_loc != std::string::npos && mapped_name.length() == match_loc + strlen(suffix)) {
      return true;
    }
  }
  return false;
}

static bool DisplayMappingIfFromVdexFile(pm_map_t* map, Printer* printer) {
  std::string vdex_name = pm_map_name(map);
  // Extract all the dex files from the vdex file.
  std::string error_msg;
  std::unique_ptr<VdexFile> vdex(VdexFile::Open(vdex_name,
                                                false /*writeable*/,
                                                false /*low_4gb*/,
                                                false /*unquicken */,
                                                &error_msg /*out*/));
  if (vdex == nullptr) {
    std::cerr << "Could not open vdex file "
              << vdex_name
              << ": error "
              << error_msg
              << std::endl;
    return false;
  }

  std::vector<std::unique_ptr<const DexFile>> dex_files;
  if (!vdex->OpenAllDexFiles(&dex_files, &error_msg)) {
    std::cerr << "Dex files could not be opened for "
              << vdex_name
              << ": error "
              << error_msg
              << std::endl;
    return false;
  }
  // Open the page mapping (one uint64_t per page) for the entire vdex mapping.
  uint64_t* pagemap;
  size_t len;
  if (pm_map_pagemap(map, &pagemap, &len) != 0) {
    std::cerr << "Error creating pagemap." << std::endl;
    return false;
  }
  // Process the dex files.
  std::cout << "MAPPING "
            << pm_map_name(map)
            << StringPrintf(": %" PRIx64 "-%" PRIx64, pm_map_start(map), pm_map_end(map))
            << std::endl;
  for (const auto& dex_file : dex_files) {
    ProcessOneDexMapping(pagemap,
                         pm_map_start(map),
                         dex_file.get(),
                         reinterpret_cast<uint64_t>(vdex->Begin()),
                         printer);
  }
  free(pagemap);
  return true;
}

static void ProcessOneOatMapping(uint64_t* pagemap, size_t size, Printer* printer) {
  static constexpr size_t kLineLength = 32;
  size_t resident_page_count = 0;
  for (size_t page = 0; page < size; ++page) {
    char type_char = '.';
    if (PM_PAGEMAP_PRESENT(pagemap[page])) {
      ++resident_page_count;
      type_char = '*';
    }
    if (g_verbose) {
      std::cout << type_char;
      if (page % kLineLength == kLineLength - 1) {
        std::cout << std::endl;
      }
    }
  }
  if (g_verbose) {
    if (size % kLineLength != 0) {
      std::cout << std::endl;
    }
  }
  double percent_of_total = 100.0 * resident_page_count / size;
  printer->PrintHeader();
  printer->PrintOne("EXECUTABLE", resident_page_count, size, percent_of_total, percent_of_total);
  printer->PrintSkipLine();
}

static bool IsOatFileMapping(const std::string& mapped_name) {
  // Confirm that the map is from an oat file.
  static const char* suffixes[] = { ".odex", ".oat" };
  for (const char* suffix : suffixes) {
    size_t match_loc = mapped_name.find(suffix);
    if (match_loc != std::string::npos && mapped_name.length() == match_loc + strlen(suffix)) {
      return true;
    }
  }
  return false;
}

static bool DisplayMappingIfFromOatFile(pm_map_t* map, Printer* printer) {
  // Open the page mapping (one uint64_t per page) for the entire vdex mapping.
  uint64_t* pagemap;
  size_t len;
  if (pm_map_pagemap(map, &pagemap, &len) != 0) {
    std::cerr << "Error creating pagemap." << std::endl;
    return false;
  }
  // Process the dex files.
  std::cout << "MAPPING "
            << pm_map_name(map)
            << StringPrintf(": %" PRIx64 "-%" PRIx64, pm_map_start(map), pm_map_end(map))
            << std::endl;
  ProcessOneOatMapping(pagemap, len, printer);
  free(pagemap);
  return true;
}

static bool FilterByNameContains(const std::string& mapped_file_name,
                                 const std::vector<std::string>& name_filters) {
  // If no filters were set, everything matches.
  if (name_filters.empty()) {
    return true;
  }
  for (const auto& name_contains : name_filters) {
    if (mapped_file_name.find(name_contains) != std::string::npos) {
      return true;
    }
  }
  return false;
}
#endif

static void Usage(const char* cmd) {
  std::cout << "Usage: " << cmd << " [options] pid" << std::endl
            << "    --contains=<string>:  Display sections containing string." << std::endl
            << "    --help:               Shows this message." << std::endl
            << "    --verbose:            Makes displays verbose." << std::endl;
  PrintLetterKey();
}

static int DexDiagMain(int argc, char* argv[]) {
  if (argc < 2) {
    Usage(argv[0]);
    return EXIT_FAILURE;
  }

  std::vector<std::string> name_filters;
  // TODO: add option to track usage by class name, etc.
  for (int i = 1; i < argc - 1; ++i) {
    const StringPiece option(argv[i]);
    if (option == "--help") {
      Usage(argv[0]);
      return EXIT_SUCCESS;
    } else if (option == "--verbose") {
      g_verbose = true;
    } else if (option.starts_with("--contains=")) {
      std::string contains(option.substr(strlen("--contains=")).data());
      name_filters.push_back(contains);
    } else {
      Usage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  // Art specific set up.
  Locks::Init();
  InitLogging(argv, Runtime::Abort);
  MemMap::Init();

#ifdef ART_TARGET_ANDROID
  pid_t pid;
  char* endptr;
  pid = (pid_t)strtol(argv[argc - 1], &endptr, 10);
  if (*endptr != '\0' || kill(pid, 0) != 0) {
    std::cerr << StringPrintf("Invalid PID \"%s\".\n", argv[argc - 1]) << std::endl;
    return EXIT_FAILURE;
  }

  // get libpagemap kernel information.
  pm_kernel_t* ker;
  if (pm_kernel_create(&ker) != 0) {
    std::cerr << "Error creating kernel interface -- does this kernel have pagemap?" << std::endl;
    return EXIT_FAILURE;
  }

  // get libpagemap process information.
  pm_process_t* proc;
  if (pm_process_create(ker, pid, &proc) != 0) {
    std::cerr << "Error creating process interface -- does process "
              << pid
              << " really exist?"
              << std::endl;
    return EXIT_FAILURE;
  }

  // Get the set of mappings by the specified process.
  pm_map_t** maps;
  size_t num_maps;
  if (pm_process_maps(proc, &maps, &num_maps) != 0) {
    std::cerr << "Error listing maps." << std::endl;
    return EXIT_FAILURE;
  }

  bool match_found = false;
  // Process the mappings that are due to vdex or oat files.
  Printer printer;
  for (size_t i = 0; i < num_maps; ++i) {
    std::string mapped_file_name = pm_map_name(maps[i]);
    // Filter by name contains options (if any).
    if (!FilterByNameContains(mapped_file_name, name_filters)) {
      continue;
    }
    if (IsVdexFileMapping(mapped_file_name)) {
      if (!DisplayMappingIfFromVdexFile(maps[i], &printer)) {
        return EXIT_FAILURE;
      }
      match_found = true;
    } else if (IsOatFileMapping(mapped_file_name)) {
      if (!DisplayMappingIfFromOatFile(maps[i], &printer)) {
        return EXIT_FAILURE;
      }
      match_found = true;
    }
  }
  if (!match_found) {
    std::cerr << "No relevant memory maps were found." << std::endl;
    return EXIT_FAILURE;
  }
#endif

  return EXIT_SUCCESS;
}

}  // namespace art

int main(int argc, char* argv[]) {
  return art::DexDiagMain(argc, argv);
}
