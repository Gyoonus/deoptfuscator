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

#include "elf_writer_quick.h"

#include <openssl/sha.h>
#include <unordered_map>
#include <unordered_set>

#include <android-base/logging.h>

#include "base/casts.h"
#include "base/leb128.h"
#include "base/utils.h"
#include "compiled_method.h"
#include "debug/elf_debug_writer.h"
#include "debug/method_debug_info.h"
#include "driver/compiler_options.h"
#include "elf.h"
#include "elf_utils.h"
#include "globals.h"
#include "linker/buffered_output_stream.h"
#include "linker/elf_builder.h"
#include "linker/file_output_stream.h"
#include "thread-current-inl.h"
#include "thread_pool.h"

namespace art {
namespace linker {

// .eh_frame and .debug_frame are almost identical.
// Except for some minor formatting differences, the main difference
// is that .eh_frame is allocated within the running program because
// it is used by C++ exception handling (which we do not use so we
// can choose either).  C++ compilers generally tend to use .eh_frame
// because if they need it sometimes, they might as well always use it.
// Let's use .debug_frame because it is easier to strip or compress.
constexpr dwarf::CFIFormat kCFIFormat = dwarf::DW_DEBUG_FRAME_FORMAT;

class DebugInfoTask : public Task {
 public:
  DebugInfoTask(InstructionSet isa,
                const InstructionSetFeatures* features,
                uint64_t text_section_address,
                size_t text_section_size,
                uint64_t dex_section_address,
                size_t dex_section_size,
                const debug::DebugInfo& debug_info)
      : isa_(isa),
        instruction_set_features_(features),
        text_section_address_(text_section_address),
        text_section_size_(text_section_size),
        dex_section_address_(dex_section_address),
        dex_section_size_(dex_section_size),
        debug_info_(debug_info) {
  }

  void Run(Thread*) {
    result_ = debug::MakeMiniDebugInfo(isa_,
                                       instruction_set_features_,
                                       text_section_address_,
                                       text_section_size_,
                                       dex_section_address_,
                                       dex_section_size_,
                                       debug_info_);
  }

  std::vector<uint8_t>* GetResult() {
    return &result_;
  }

 private:
  InstructionSet isa_;
  const InstructionSetFeatures* instruction_set_features_;
  uint64_t text_section_address_;
  size_t text_section_size_;
  uint64_t dex_section_address_;
  size_t dex_section_size_;
  const debug::DebugInfo& debug_info_;
  std::vector<uint8_t> result_;
};

template <typename ElfTypes>
class ElfWriterQuick FINAL : public ElfWriter {
 public:
  ElfWriterQuick(InstructionSet instruction_set,
                 const InstructionSetFeatures* features,
                 const CompilerOptions* compiler_options,
                 File* elf_file);
  ~ElfWriterQuick();

  void Start() OVERRIDE;
  void PrepareDynamicSection(size_t rodata_size,
                             size_t text_size,
                             size_t bss_size,
                             size_t bss_methods_offset,
                             size_t bss_roots_offset,
                             size_t dex_section_size) OVERRIDE;
  void PrepareDebugInfo(const debug::DebugInfo& debug_info) OVERRIDE;
  OutputStream* StartRoData() OVERRIDE;
  void EndRoData(OutputStream* rodata) OVERRIDE;
  OutputStream* StartText() OVERRIDE;
  void EndText(OutputStream* text) OVERRIDE;
  void WriteDynamicSection() OVERRIDE;
  void WriteDebugInfo(const debug::DebugInfo& debug_info) OVERRIDE;
  bool End() OVERRIDE;

  virtual OutputStream* GetStream() OVERRIDE;

  size_t GetLoadedSize() OVERRIDE;

  static void EncodeOatPatches(const std::vector<uintptr_t>& locations,
                               std::vector<uint8_t>* buffer);

 private:
  const InstructionSetFeatures* instruction_set_features_;
  const CompilerOptions* const compiler_options_;
  File* const elf_file_;
  size_t rodata_size_;
  size_t text_size_;
  size_t bss_size_;
  size_t dex_section_size_;
  std::unique_ptr<BufferedOutputStream> output_stream_;
  std::unique_ptr<ElfBuilder<ElfTypes>> builder_;
  std::unique_ptr<DebugInfoTask> debug_info_task_;
  std::unique_ptr<ThreadPool> debug_info_thread_pool_;

  void ComputeFileBuildId(uint8_t (*build_id)[ElfBuilder<ElfTypes>::kBuildIdLen]);

  DISALLOW_IMPLICIT_CONSTRUCTORS(ElfWriterQuick);
};

std::unique_ptr<ElfWriter> CreateElfWriterQuick(InstructionSet instruction_set,
                                                const InstructionSetFeatures* features,
                                                const CompilerOptions* compiler_options,
                                                File* elf_file) {
  if (Is64BitInstructionSet(instruction_set)) {
    return std::make_unique<ElfWriterQuick<ElfTypes64>>(instruction_set,
                                                        features,
                                                        compiler_options,
                                                        elf_file);
  } else {
    return std::make_unique<ElfWriterQuick<ElfTypes32>>(instruction_set,
                                                        features,
                                                        compiler_options,
                                                        elf_file);
  }
}

template <typename ElfTypes>
ElfWriterQuick<ElfTypes>::ElfWriterQuick(InstructionSet instruction_set,
                                         const InstructionSetFeatures* features,
                                         const CompilerOptions* compiler_options,
                                         File* elf_file)
    : ElfWriter(),
      instruction_set_features_(features),
      compiler_options_(compiler_options),
      elf_file_(elf_file),
      rodata_size_(0u),
      text_size_(0u),
      bss_size_(0u),
      dex_section_size_(0u),
      output_stream_(
          std::make_unique<BufferedOutputStream>(std::make_unique<FileOutputStream>(elf_file))),
      builder_(new ElfBuilder<ElfTypes>(instruction_set, features, output_stream_.get())) {}

template <typename ElfTypes>
ElfWriterQuick<ElfTypes>::~ElfWriterQuick() {}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::Start() {
  builder_->Start();
  if (compiler_options_->GetGenerateBuildId()) {
    builder_->GetBuildId()->AllocateVirtualMemory(builder_->GetBuildId()->GetSize());
    builder_->WriteBuildIdSection();
  }
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::PrepareDynamicSection(size_t rodata_size,
                                                     size_t text_size,
                                                     size_t bss_size,
                                                     size_t bss_methods_offset,
                                                     size_t bss_roots_offset,
                                                     size_t dex_section_size) {
  DCHECK_EQ(rodata_size_, 0u);
  rodata_size_ = rodata_size;
  DCHECK_EQ(text_size_, 0u);
  text_size_ = text_size;
  DCHECK_EQ(bss_size_, 0u);
  bss_size_ = bss_size;
  DCHECK_EQ(dex_section_size_, 0u);
  dex_section_size_ = dex_section_size;
  builder_->PrepareDynamicSection(elf_file_->GetPath(),
                                  rodata_size_,
                                  text_size_,
                                  bss_size_,
                                  bss_methods_offset,
                                  bss_roots_offset,
                                  dex_section_size);
}

template <typename ElfTypes>
OutputStream* ElfWriterQuick<ElfTypes>::StartRoData() {
  auto* rodata = builder_->GetRoData();
  rodata->Start();
  return rodata;
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::EndRoData(OutputStream* rodata) {
  CHECK_EQ(builder_->GetRoData(), rodata);
  builder_->GetRoData()->End();
}

template <typename ElfTypes>
OutputStream* ElfWriterQuick<ElfTypes>::StartText() {
  auto* text = builder_->GetText();
  text->Start();
  return text;
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::EndText(OutputStream* text) {
  CHECK_EQ(builder_->GetText(), text);
  builder_->GetText()->End();
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::WriteDynamicSection() {
  if (builder_->GetIsa() == InstructionSet::kMips ||
      builder_->GetIsa() == InstructionSet::kMips64) {
    builder_->WriteMIPSabiflagsSection();
  }
  builder_->WriteDynamicSection();
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::PrepareDebugInfo(const debug::DebugInfo& debug_info) {
  if (!debug_info.Empty() && compiler_options_->GetGenerateMiniDebugInfo()) {
    // Prepare the mini-debug-info in background while we do other I/O.
    Thread* self = Thread::Current();
    debug_info_task_ = std::unique_ptr<DebugInfoTask>(
        new DebugInfoTask(builder_->GetIsa(),
                          instruction_set_features_,
                          builder_->GetText()->GetAddress(),
                          text_size_,
                          builder_->GetDex()->Exists() ? builder_->GetDex()->GetAddress() : 0,
                          dex_section_size_,
                          debug_info));
    debug_info_thread_pool_ = std::unique_ptr<ThreadPool>(
        new ThreadPool("Mini-debug-info writer", 1));
    debug_info_thread_pool_->AddTask(self, debug_info_task_.get());
    debug_info_thread_pool_->StartWorkers(self);
  }
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::WriteDebugInfo(const debug::DebugInfo& debug_info) {
  if (!debug_info.Empty()) {
    if (compiler_options_->GetGenerateDebugInfo()) {
      // Generate all the debug information we can.
      debug::WriteDebugInfo(builder_.get(), debug_info, kCFIFormat, true /* write_oat_patches */);
    }
    if (compiler_options_->GetGenerateMiniDebugInfo()) {
      // Wait for the mini-debug-info generation to finish and write it to disk.
      Thread* self = Thread::Current();
      DCHECK(debug_info_thread_pool_ != nullptr);
      debug_info_thread_pool_->Wait(self, true, false);
      builder_->WriteSection(".gnu_debugdata", debug_info_task_->GetResult());
    }
  }
}

template <typename ElfTypes>
bool ElfWriterQuick<ElfTypes>::End() {
  builder_->End();
  if (compiler_options_->GetGenerateBuildId()) {
    uint8_t build_id[ElfBuilder<ElfTypes>::kBuildIdLen];
    ComputeFileBuildId(&build_id);
    builder_->WriteBuildId(build_id);
  }
  return builder_->Good();
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::ComputeFileBuildId(
    uint8_t (*build_id)[ElfBuilder<ElfTypes>::kBuildIdLen]) {
  constexpr int kBufSize = 8192;
  std::vector<char> buffer(kBufSize);
  int64_t offset = 0;
  SHA_CTX ctx;
  SHA1_Init(&ctx);
  while (true) {
    int64_t bytes_read = elf_file_->Read(buffer.data(), kBufSize, offset);
    CHECK_GE(bytes_read, 0);
    if (bytes_read == 0) {
      // End of file.
      break;
    }
    SHA1_Update(&ctx, buffer.data(), bytes_read);
    offset += bytes_read;
  }
  SHA1_Final(*build_id, &ctx);
}

template <typename ElfTypes>
OutputStream* ElfWriterQuick<ElfTypes>::GetStream() {
  return builder_->GetStream();
}

template <typename ElfTypes>
size_t ElfWriterQuick<ElfTypes>::GetLoadedSize() {
  return builder_->GetLoadedSize();
}

// Explicit instantiations
template class ElfWriterQuick<ElfTypes32>;
template class ElfWriterQuick<ElfTypes64>;

}  // namespace linker
}  // namespace art
