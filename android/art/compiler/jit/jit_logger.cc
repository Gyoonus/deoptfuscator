/*
 * Copyright 2016 The Android Open Source Project
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

#include "jit_logger.h"

#include "arch/instruction_set.h"
#include "art_method-inl.h"
#include "base/time_utils.h"
#include "base/unix_file/fd_file.h"
#include "driver/compiler_driver.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "oat_file-inl.h"

namespace art {
namespace jit {

#ifdef ART_TARGET_ANDROID
static const char* kLogPrefix = "/data/misc/trace";
#else
static const char* kLogPrefix = "/tmp";
#endif

// File format of perf-PID.map:
// +---------------------+
// |ADDR SIZE symbolname1|
// |ADDR SIZE symbolname2|
// |...                  |
// +---------------------+
void JitLogger::OpenPerfMapLog() {
  std::string pid_str = std::to_string(getpid());
  std::string perf_filename = std::string(kLogPrefix) + "/perf-" + pid_str + ".map";
  perf_file_.reset(OS::CreateEmptyFileWriteOnly(perf_filename.c_str()));
  if (perf_file_ == nullptr) {
    LOG(ERROR) << "Could not create perf file at " << perf_filename <<
      " Are you on a user build? Perf only works on userdebug/eng builds";
  }
}

void JitLogger::WritePerfMapLog(const void* ptr, size_t code_size, ArtMethod* method) {
  if (perf_file_ != nullptr) {
    std::string method_name = method->PrettyMethod();

    std::ostringstream stream;
    stream << std::hex
           << reinterpret_cast<uintptr_t>(ptr)
           << " "
           << code_size
           << " "
           << method_name
           << std::endl;
    std::string str = stream.str();
    bool res = perf_file_->WriteFully(str.c_str(), str.size());
    if (!res) {
      LOG(WARNING) << "Failed to write jitted method info in log: write failure.";
    }
  } else {
    LOG(WARNING) << "Failed to write jitted method info in log: log file doesn't exist.";
  }
}

void JitLogger::ClosePerfMapLog() {
  if (perf_file_ != nullptr) {
    UNUSED(perf_file_->Flush());
    UNUSED(perf_file_->Close());
  }
}

//  File format of jit-PID.jump:
//
//  +--------------------------------+
//  |  PerfJitHeader                 |
//  +--------------------------------+
//  |  PerfJitCodeLoad {             | .
//  |    struct PerfJitBase;         |  .
//  |    uint32_t process_id_;       |   .
//  |    uint32_t thread_id_;        |   .
//  |    uint64_t vma_;              |   .
//  |    uint64_t code_address_;     |   .
//  |    uint64_t code_size_;        |   .
//  |    uint64_t code_id_;          |   .
//  |  }                             |   .
//  +-                              -+   .
//  |  method_name'\0'               |   +--> one jitted method
//  +-                              -+   .
//  |  jitted code binary            |   .
//  |  ...                           |   .
//  +--------------------------------+   .
//  |  PerfJitCodeDebugInfo     {    |   .
//  |    struct PerfJitBase;         |   .
//  |    uint64_t address_;          |   .
//  |    uint64_t entry_count_;      |   .
//  |    struct PerfJitDebugEntry;   |  .
//  |  }                             | .
//  +--------------------------------+
//  |  PerfJitCodeLoad               |
//     ...
//
struct PerfJitHeader {
  uint32_t magic_;            // Characters "JiTD"
  uint32_t version_;          // Header version
  uint32_t size_;             // Total size of header
  uint32_t elf_mach_target_;  // Elf mach target
  uint32_t reserved_;         // Reserved, currently not used
  uint32_t process_id_;       // Process ID of the JIT compiler
  uint64_t time_stamp_;       // Timestamp when the header is generated
  uint64_t flags_;            // Currently the flags are only used for choosing clock for timestamp,
                              // we set it to 0 to tell perf that we use CLOCK_MONOTONIC clock.
  static const uint32_t kMagic = 0x4A695444;  // "JiTD"
  static const uint32_t kVersion = 1;
};

// Each record starts with such basic information: event type, total size, and timestamp.
struct PerfJitBase {
  enum PerfJitEvent {
    // A jitted code load event.
    // In ART JIT, it is used to log a new method is jit compiled and committed to jit-code-cache.
    // Note that such kLoad event supports code cache GC in ART JIT.
    // For every kLoad event recorded in jit-PID.dump and every perf sample recorded in perf.data,
    // each event/sample has time stamp. In case code cache GC happens in ART JIT, and a new
    // jitted method is committed to the same address of a previously deleted method,
    // the time stamp information can help profiler to tell whether this sample belongs to the
    // era of the first jitted method, or does it belong to the period of the second jitted method.
    // JitCodeCache doesn't have to record any event on 'code delete'.
    kLoad = 0,

    // A jitted code move event, i,e. a jitted code moved from one address to another address.
    // It helps profiler to map samples to the right symbol even when the code is moved.
    // In ART JIT, this event can help log such behavior:
    // A jitted method is recorded in previous kLoad event, but due to some reason,
    // it is moved to another address in jit-code-cache.
    kMove = 1,

    // Logs debug line/column information.
    kDebugInfo = 2,

    // Logs JIT VM end of life event.
    kClose = 3
  };
  uint32_t event_;       // Must be one of the events defined in PerfJitEvent.
  uint32_t size_;        // Total size of this event record.
                         // For example, for kLoad event, size of the event record is:
                         // sizeof(PerfJitCodeLoad) + method_name.size() + compiled code size.
  uint64_t time_stamp_;  // Timestamp for the event.
};

// Logs a jitted code load event (kLoad).
// In ART JIT, it is used to log a new method is jit compiled and commited to jit-code-cache.
struct PerfJitCodeLoad : PerfJitBase {
  uint32_t process_id_;    // Process ID who performs the jit code load.
                           // In ART JIT, it is the pid of the JIT compiler.
  uint32_t thread_id_;     // Thread ID who performs the jit code load.
                           // In ART JIT, it is the tid of the JIT compiler.
  uint64_t vma_;           // Address of the code section. In ART JIT, because code_address_
                           // uses absolute address, this field is 0.
  uint64_t code_address_;  // Address where is jitted code is loaded.
  uint64_t code_size_;     // Size of the jitted code.
  uint64_t code_id_;       // Unique ID for each jitted code.
};

// This structure is for source line/column mapping.
// Currently this feature is not implemented in ART JIT yet.
struct PerfJitDebugEntry {
  uint64_t address_;      // Code address which maps to the line/column in source.
  uint32_t line_number_;  // Source line number starting at 1.
  uint32_t column_;       // Column discriminator, default 0.
  const char name_[0];    // Followed by null-terminated name or \0xff\0 if same as previous.
};

// Logs debug line information (kDebugInfo).
// This structure is for source line/column mapping.
// Currently this feature is not implemented in ART JIT yet.
struct PerfJitCodeDebugInfo : PerfJitBase {
  uint64_t address_;              // Starting code address which the debug info describes.
  uint64_t entry_count_;          // How many instances of PerfJitDebugEntry.
  PerfJitDebugEntry entries_[0];  // Followed by entry_count_ instances of PerfJitDebugEntry.
};

static uint32_t GetElfMach() {
#if defined(__arm__)
  static const uint32_t kElfMachARM = 0x28;
  return kElfMachARM;
#elif defined(__aarch64__)
  static const uint32_t kElfMachARM64 = 0xB7;
  return kElfMachARM64;
#elif defined(__i386__)
  static const uint32_t kElfMachIA32 = 0x3;
  return kElfMachIA32;
#elif defined(__x86_64__)
  static const uint32_t kElfMachX64 = 0x3E;
  return kElfMachX64;
#else
  UNIMPLEMENTED(WARNING) << "Unsupported architecture in JitLogger";
  return 0;
#endif
}

void JitLogger::OpenMarkerFile() {
  int fd = jit_dump_file_->Fd();
  // The 'perf inject' tool requires that the jit-PID.dump file
  // must have a mmap(PROT_READ|PROT_EXEC) record in perf.data.
  marker_address_ = mmap(nullptr, kPageSize, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0);
  if (marker_address_ == MAP_FAILED) {
    LOG(WARNING) << "Failed to create record in perf.data. JITed code profiling will not work.";
    return;
  }
}

void JitLogger::CloseMarkerFile() {
  if (marker_address_ != nullptr) {
    munmap(marker_address_, kPageSize);
  }
}

void JitLogger::WriteJitDumpDebugInfo() {
  // In the future, we can add java source file line/column mapping here.
}

void JitLogger::WriteJitDumpHeader() {
  PerfJitHeader header;

  std::memset(&header, 0, sizeof(header));
  header.magic_ = PerfJitHeader::kMagic;
  header.version_ = PerfJitHeader::kVersion;
  header.size_ = sizeof(header);
  header.elf_mach_target_ = GetElfMach();
  header.process_id_ = static_cast<uint32_t>(getpid());
  header.time_stamp_ = art::NanoTime();  // CLOCK_MONOTONIC clock is required.
  header.flags_ = 0;

  bool res = jit_dump_file_->WriteFully(reinterpret_cast<const char*>(&header), sizeof(header));
  if (!res) {
    LOG(WARNING) << "Failed to write profiling log. The 'perf inject' tool will not work.";
  }
}

void JitLogger::OpenJitDumpLog() {
  std::string pid_str = std::to_string(getpid());
  std::string jitdump_filename = std::string(kLogPrefix) + "/jit-" + pid_str + ".dump";

  jit_dump_file_.reset(OS::CreateEmptyFile(jitdump_filename.c_str()));
  if (jit_dump_file_ == nullptr) {
    LOG(ERROR) << "Could not create jit dump file at " << jitdump_filename <<
      " Are you on a user build? Perf only works on userdebug/eng builds";
    return;
  }

  OpenMarkerFile();

  // Continue to write jit-PID.dump file even above OpenMarkerFile() fails.
  // Even if that means 'perf inject' tool cannot work, developers can still use other tools
  // to map the samples in perf.data to the information (symbol,address,code) recorded
  // in the jit-PID.dump file, and still proceed the jitted code analysis.
  WriteJitDumpHeader();
}

void JitLogger::WriteJitDumpLog(const void* ptr, size_t code_size, ArtMethod* method) {
  if (jit_dump_file_ != nullptr) {
    std::string method_name = method->PrettyMethod();

    PerfJitCodeLoad jit_code;
    std::memset(&jit_code, 0, sizeof(jit_code));
    jit_code.event_ = PerfJitCodeLoad::kLoad;
    jit_code.size_ = sizeof(jit_code) + method_name.size() + 1 + code_size;
    jit_code.time_stamp_ = art::NanoTime();    // CLOCK_MONOTONIC clock is required.
    jit_code.process_id_ = static_cast<uint32_t>(getpid());
    jit_code.thread_id_ = static_cast<uint32_t>(art::GetTid());
    jit_code.vma_ = 0x0;
    jit_code.code_address_ = reinterpret_cast<uint64_t>(ptr);
    jit_code.code_size_ = code_size;
    jit_code.code_id_ = code_index_++;

    // Write one complete jitted method info, including:
    // - PerfJitCodeLoad structure
    // - Method name
    // - Complete generated code of this method
    //
    // Use UNUSED() here to avoid compiler warnings.
    UNUSED(jit_dump_file_->WriteFully(reinterpret_cast<const char*>(&jit_code), sizeof(jit_code)));
    UNUSED(jit_dump_file_->WriteFully(method_name.c_str(), method_name.size() + 1));
    UNUSED(jit_dump_file_->WriteFully(ptr, code_size));

    WriteJitDumpDebugInfo();
  }
}

void JitLogger::CloseJitDumpLog() {
  if (jit_dump_file_ != nullptr) {
    CloseMarkerFile();
    UNUSED(jit_dump_file_->Flush());
    UNUSED(jit_dump_file_->Close());
  }
}

}  // namespace jit
}  // namespace art
