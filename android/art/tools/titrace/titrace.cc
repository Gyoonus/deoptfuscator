// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "instruction_decoder.h"

#include <android-base/logging.h>
#include <atomic>
#include <jni.h>
#include <jvmti.h>
#include <map>
#include <memory>
#include <mutex>

// We could probably return a JNI_ERR here but lets crash instead if something fails.
#define CHECK_JVMTI_ERROR(jvmti, errnum) \
    CHECK_EQ(JVMTI_ERROR_NONE, (errnum)) << GetJvmtiErrorString((jvmti), (errnum)) << (" ")

namespace titrace {

static const char* GetJvmtiErrorString(jvmtiEnv* jvmti, jvmtiError errnum) {
  char* errnum_str = nullptr;
  jvmti->GetErrorName(errnum, /*out*/ &errnum_str);
  if (errnum_str == nullptr) {
    return "Unknown";
  }

  return errnum_str;
}

// Type-safe wrapper for JVMTI-allocated memory.
// Deallocates with jvmtiEnv::Deallocate.
template <typename T>
struct TiMemory {
  explicit TiMemory(jvmtiEnv* env, T* mem, size_t size) : env_(env), mem_(mem), size_(size) {
  }

  ~TiMemory() {
    if (mem_ != nullptr) {
      env_->Deallocate(static_cast<unsigned char*>(mem_));
    }
    mem_ = nullptr;
  }

  TiMemory(const TiMemory& other) = delete;
  TiMemory(TiMemory&& other) {
    env_ = other.env_;
    mem_ = other.mem_;
    size_ = other.size_;

    if (this != &other) {
      other.env_ = nullptr;
      other.mem_ = nullptr;
      other.size_ = 0u;
    }
  }

  TiMemory& operator=(TiMemory&& other) {
    if (mem_ != other.mem_) {
      TiMemory::~TiMemory();
    }
    new (this) TiMemory(std::move(other));
    return *this;
  }

  T* GetMemory() {
    return mem_;
  }

  size_t Size() {
    return size_ / sizeof(T);
  }

 private:
  jvmtiEnv* env_;
  T* mem_;
  size_t size_;
};

struct MethodBytecode {
  explicit MethodBytecode(jvmtiEnv* env, unsigned char* memory, jint size)
      : bytecode_(env, memory, static_cast<size_t>(size)) {
  }

  TiMemory<uint8_t> bytecode_;
};

struct TraceStatistics {
  static void Initialize(jvmtiEnv* jvmti) {
    TraceStatistics& stats = GetSingleton();

    bool is_ri = true;
    {
      jvmtiError error;
      char* value_ptr;
      error = jvmti->GetSystemProperty("java.vm.name", /*out*/ &value_ptr);
      CHECK_JVMTI_ERROR(jvmti, error) << "Failed to get property 'java.vm.name'";
      CHECK(value_ptr != nullptr) << "Returned property was null for 'java.vm.name'";

      if (strcmp("Dalvik", value_ptr) == 0) {
        is_ri = false;
      }
    }

    InstructionFileFormat format =
        is_ri ? InstructionFileFormat::kClass : InstructionFileFormat::kDex;
    stats.instruction_decoder_.reset(InstructionDecoder::NewInstance(format));

    CHECK_GE(arraysize(stats.instruction_counter_),
             stats.instruction_decoder_->GetMaximumOpcode());
  }

  static TraceStatistics& GetSingleton() {
    static TraceStatistics stats;
    return stats;
  }

  void Log() {
    LOG(INFO) << "================================================";
    LOG(INFO) << "              TI Trace // Summary               ";
    LOG(INFO) << "++++++++++++++++++++++++++++++++++++++++++++++++";
    LOG(INFO) << "  * Single step counter: " << single_step_counter_;
    LOG(INFO) << "+++++++++++    Instructions Count   ++++++++++++";

    size_t total = single_step_counter_;
    for (size_t i = 0; i < arraysize(instruction_counter_); ++i) {
      size_t inst_count = instruction_counter_[i];
      if (inst_count > 0) {
        const char* opcode_name = instruction_decoder_->GetName(i);
        LOG(INFO) << "  * " << opcode_name << "(op:" << i << "), count: " << inst_count
                  << ", % of total: " << (100.0 * inst_count / total);
      }
    }

    LOG(INFO) << "------------------------------------------------";
  }

  void OnSingleStep(jvmtiEnv* jvmti_env, jmethodID method, jlocation location) {
    // Counters do not need a happens-before.
    // Use the weakest memory order simply to avoid tearing.
    single_step_counter_.fetch_add(1u, std::memory_order_relaxed);

    MethodBytecode& bytecode = LookupBytecode(jvmti_env, method);

    // Decode jlocation value that depends on the bytecode format.
    size_t actual_location = instruction_decoder_->LocationToOffset(static_cast<size_t>(location));

    // Decode the exact instruction and increment its counter.
    CHECK_LE(actual_location, bytecode.bytecode_.Size());
    RecordInstruction(bytecode.bytecode_.GetMemory() + actual_location);
  }

 private:
  void RecordInstruction(const uint8_t* instruction) {
    uint8_t opcode = instruction[0];
    // Counters do not need a happens-before.
    // Use the weakest memory order simply to avoid tearing.
    instruction_counter_[opcode].fetch_add(1u, std::memory_order_relaxed);
  }

  MethodBytecode& LookupBytecode(jvmtiEnv* jvmti_env, jmethodID method) {
    jvmtiError error;
    std::lock_guard<std::mutex> lock(bytecode_cache_mutex_);

    auto it = bytecode_cache_.find(method);
    if (it == bytecode_cache_.end()) {
      jint bytecode_count_ptr = 0;
      unsigned char* bytecodes_ptr = nullptr;

      error = jvmti_env->GetBytecodes(method, &bytecode_count_ptr, &bytecodes_ptr);
      CHECK_JVMTI_ERROR(jvmti_env, error) << "Failed to get bytecodes for method " << method;
      CHECK(bytecodes_ptr != nullptr) << "Bytecode ptr was null for method " << method;
      CHECK_GE(bytecode_count_ptr, 0) << "Bytecode size too small for method " << method;

      // std::pair<iterator, bool inserted>
      auto&& pair = bytecode_cache_.insert(
          std::make_pair(method, MethodBytecode(jvmti_env, bytecodes_ptr, bytecode_count_ptr)));
      it = pair.first;
    }

    // Returning the address is safe. if map is resized, the contents will not move.
    return it->second;
  }

  std::unique_ptr<InstructionDecoder> instruction_decoder_;

  std::atomic<size_t> single_step_counter_{0u};
  std::atomic<size_t> instruction_counter_[256]{};

  // Cache the bytecode to avoid calling into JVMTI repeatedly.
  // TODO: invalidate if the bytecode was updated?
  std::map<jmethodID, MethodBytecode> bytecode_cache_;
  // bytecode cache is thread-safe.
  std::mutex bytecode_cache_mutex_;
};

struct EventCallbacks {
  static void SingleStep(jvmtiEnv* jvmti_env,
                         JNIEnv* jni_env ATTRIBUTE_UNUSED,
                         jthread thread ATTRIBUTE_UNUSED,
                         jmethodID method,
                         jlocation location) {
    TraceStatistics& stats = TraceStatistics::GetSingleton();
    stats.OnSingleStep(jvmti_env, method, location);
  }

  // Use "kill -SIGQUIT" to generate a data dump request.
  // Useful when running an android app since it doesn't go through
  // a normal Agent_OnUnload.
  static void DataDumpRequest(jvmtiEnv* jvmti_env ATTRIBUTE_UNUSED) {
    TraceStatistics& stats = TraceStatistics::GetSingleton();
    stats.Log();
  }
};

}  // namespace titrace

// Late attachment (e.g. 'am attach-agent').
JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM *vm, char* options, void* reserved) {
  return Agent_OnLoad(vm, options, reserved);
}

// Early attachment (e.g. 'java -agent[lib|path]:filename.so').
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm,
                                    char* /* options */,
                                    void* /* reserved */) {
  using namespace titrace;  // NOLINT [build/namespaces] [5]

  android::base::InitLogging(/* argv */nullptr);

  jvmtiEnv* jvmti = nullptr;
  {
    jint res = 0;
    res = jvm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_1);

    if (res != JNI_OK || jvmti == nullptr) {
      LOG(FATAL) << "Unable to access JVMTI, error code " << res;
    }
  }

  LOG(INFO) << "Agent_OnLoad: Hello World";

  {
    // Initialize our instruction file-format decoder.
    TraceStatistics::Initialize(jvmti);
  }

  jvmtiError error{};

  // Set capabilities.
  {
    jvmtiCapabilities caps = {};
    caps.can_generate_single_step_events = 1;
    caps.can_get_bytecodes = 1;

    error = jvmti->AddCapabilities(&caps);
    CHECK_JVMTI_ERROR(jvmti, error)
      << "Unable to get necessary JVMTI capabilities";
  }

  // Set callbacks.
  {
    jvmtiEventCallbacks callbacks = {};
    callbacks.SingleStep = &EventCallbacks::SingleStep;
    callbacks.DataDumpRequest = &EventCallbacks::DataDumpRequest;

    error = jvmti->SetEventCallbacks(&callbacks,
                                     static_cast<jint>(sizeof(callbacks)));
    CHECK_JVMTI_ERROR(jvmti, error) << "Unable to set event callbacks";
  }

  // Enable events notification.
  {
    error = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                            JVMTI_EVENT_SINGLE_STEP,
                                            nullptr /* all threads */);
    CHECK_JVMTI_ERROR(jvmti, error)
      << "Failed to enable SINGLE_STEP notification";

    error = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                            JVMTI_EVENT_DATA_DUMP_REQUEST,
                                            nullptr /* all threads */);
    CHECK_JVMTI_ERROR(jvmti, error)
      << "Failed to enable DATA_DUMP_REQUEST notification";
  }

  return JNI_OK;
}

// Note: This is not called for normal Android apps,
// use "kill -SIGQUIT" instead to generate a data dump request.
JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* vm ATTRIBUTE_UNUSED) {
  using namespace titrace;  // NOLINT [build/namespaces] [5]
  LOG(INFO) << "Agent_OnUnload: Goodbye";

  TraceStatistics::GetSingleton().Log();
}

