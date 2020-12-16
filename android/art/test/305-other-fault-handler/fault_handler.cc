/*
 * Copyright (C) 2018 The Android Open Source Project
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


#include <atomic>
#include <memory>

#include <jni.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mman.h>

#include "fault_handler.h"
#include "globals.h"
#include "mem_map.h"

namespace art {

class TestFaultHandler FINAL : public FaultHandler {
 public:
  explicit TestFaultHandler(FaultManager* manager)
      : FaultHandler(manager),
        map_error_(""),
        target_map_(MemMap::MapAnonymous("test-305-mmap",
                                         /* addr */ nullptr,
                                         /* byte_count */ kPageSize,
                                         /* prot */ PROT_NONE,
                                         /* low_4gb */ false,
                                         /* reuse */ false,
                                         /* error_msg */ &map_error_,
                                         /* use_ashmem */ false)),
        was_hit_(false) {
    CHECK(target_map_ != nullptr) << "Unable to create segfault target address " << map_error_;
    manager_->AddHandler(this, /*in_generated_code*/false);
  }

  virtual ~TestFaultHandler() {
    manager_->RemoveHandler(this);
  }

  bool Action(int sig, siginfo_t* siginfo, void* context ATTRIBUTE_UNUSED) OVERRIDE {
    CHECK_EQ(sig, SIGSEGV);
    CHECK_EQ(reinterpret_cast<uint32_t*>(siginfo->si_addr),
             GetTargetPointer()) << "Segfault on unexpected address!";
    CHECK(!was_hit_) << "Recursive signal!";
    was_hit_ = true;

    LOG(INFO) << "SEGV Caught. mprotecting map.";
    CHECK(target_map_->Protect(PROT_READ | PROT_WRITE)) << "Failed to mprotect R/W";
    LOG(INFO) << "Setting value to be read.";
    *GetTargetPointer() = kDataValue;
    LOG(INFO) << "Changing prot to be read-only.";
    CHECK(target_map_->Protect(PROT_READ)) << "Failed to mprotect R-only";
    return true;
  }

  void CauseSegfault() {
    CHECK_EQ(target_map_->GetProtect(), PROT_NONE);

    // This will segfault. The handler should deal with it though and we will get a value out of it.
    uint32_t data = *GetTargetPointer();

    // Prevent re-ordering around the *GetTargetPointer by the compiler
    std::atomic_signal_fence(std::memory_order_seq_cst);

    CHECK(was_hit_);
    CHECK_EQ(data, kDataValue) << "Unexpected read value from mmap";
    CHECK_EQ(target_map_->GetProtect(), PROT_READ);
    LOG(INFO) << "Success!";
  }

 private:
  uint32_t* GetTargetPointer() {
    return reinterpret_cast<uint32_t*>(target_map_->Begin() + 8);
  }

  static constexpr uint32_t kDataValue = 0xDEADBEEF;

  std::string map_error_;
  std::unique_ptr<MemMap> target_map_;
  bool was_hit_;
};

extern "C" JNIEXPORT void JNICALL Java_Main_runFaultHandlerTest(JNIEnv*, jclass) {
  std::unique_ptr<TestFaultHandler> handler(new TestFaultHandler(&fault_manager));
  handler->CauseSegfault();
}

}  // namespace art
