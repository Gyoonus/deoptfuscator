/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "fault_handler.h"

#include <setjmp.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ucontext.h>

#include "art_method-inl.h"
#include "base/logging.h"  // For VLOG
#include "base/safe_copy.h"
#include "base/stl_util.h"
#include "dex/dex_file_types.h"
#include "mirror/class.h"
#include "mirror/object_reference.h"
#include "oat_quick_method_header.h"
#include "sigchain.h"
#include "thread-current-inl.h"
#include "verify_object-inl.h"

namespace art {
// Static fault manger object accessed by signal handler.
FaultManager fault_manager;

// This needs to be NO_INLINE since some debuggers do not read the inline-info to set a breakpoint
// if it isn't.
extern "C" NO_INLINE __attribute__((visibility("default"))) void art_sigsegv_fault() {
  // Set a breakpoint here to be informed when a SIGSEGV is unhandled by ART.
  VLOG(signals)<< "Caught unknown SIGSEGV in ART fault handler - chaining to next handler.";
}

// Signal handler called on SIGSEGV.
static bool art_fault_handler(int sig, siginfo_t* info, void* context) {
  return fault_manager.HandleFault(sig, info, context);
}

#if defined(__linux__)

// Change to verify the safe implementations against the original ones.
constexpr bool kVerifySafeImpls = false;

// Provide implementations of ArtMethod::GetDeclaringClass and VerifyClassClass that use SafeCopy
// to safely dereference pointers which are potentially garbage.
// Only available on Linux due to availability of SafeCopy.

static mirror::Class* SafeGetDeclaringClass(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  char* method_declaring_class =
      reinterpret_cast<char*>(method) + ArtMethod::DeclaringClassOffset().SizeValue();

  // ArtMethod::declaring_class_ is a GcRoot<mirror::Class>.
  // Read it out into as a CompressedReference directly for simplicity's sake.
  mirror::CompressedReference<mirror::Class> cls;
  ssize_t rc = SafeCopy(&cls, method_declaring_class, sizeof(cls));
  CHECK_NE(-1, rc);

  if (kVerifySafeImpls) {
    mirror::Class* actual_class = method->GetDeclaringClassUnchecked<kWithoutReadBarrier>();
    CHECK_EQ(actual_class, cls.AsMirrorPtr());
  }

  if (rc != sizeof(cls)) {
    return nullptr;
  }

  return cls.AsMirrorPtr();
}

static mirror::Class* SafeGetClass(mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_) {
  char* obj_cls = reinterpret_cast<char*>(obj) + mirror::Object::ClassOffset().SizeValue();

  mirror::HeapReference<mirror::Class> cls;
  ssize_t rc = SafeCopy(&cls, obj_cls, sizeof(cls));
  CHECK_NE(-1, rc);

  if (kVerifySafeImpls) {
    mirror::Class* actual_class = obj->GetClass<kVerifyNone>();
    CHECK_EQ(actual_class, cls.AsMirrorPtr());
  }

  if (rc != sizeof(cls)) {
    return nullptr;
  }

  return cls.AsMirrorPtr();
}

static bool SafeVerifyClassClass(mirror::Class* cls) REQUIRES_SHARED(Locks::mutator_lock_) {
  mirror::Class* c_c = SafeGetClass(cls);
  bool result = c_c != nullptr && c_c == SafeGetClass(c_c);

  if (kVerifySafeImpls) {
    CHECK_EQ(VerifyClassClass(cls), result);
  }

  return result;
}

#else

static mirror::Class* SafeGetDeclaringClass(ArtMethod* method_obj)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return method_obj->GetDeclaringClassUnchecked<kWithoutReadBarrier>();
}

static bool SafeVerifyClassClass(mirror::Class* cls) REQUIRES_SHARED(Locks::mutator_lock_) {
  return VerifyClassClass(cls);
}
#endif


FaultManager::FaultManager() : initialized_(false) {
  sigaction(SIGSEGV, nullptr, &oldaction_);
}

FaultManager::~FaultManager() {
}

void FaultManager::Init() {
  CHECK(!initialized_);
  sigset_t mask;
  sigfillset(&mask);
  sigdelset(&mask, SIGABRT);
  sigdelset(&mask, SIGBUS);
  sigdelset(&mask, SIGFPE);
  sigdelset(&mask, SIGILL);
  sigdelset(&mask, SIGSEGV);

  SigchainAction sa = {
    .sc_sigaction = art_fault_handler,
    .sc_mask = mask,
    .sc_flags = 0UL,
  };

  AddSpecialSignalHandlerFn(SIGSEGV, &sa);
  initialized_ = true;
}

void FaultManager::Release() {
  if (initialized_) {
    RemoveSpecialSignalHandlerFn(SIGSEGV, art_fault_handler);
    initialized_ = false;
  }
}

void FaultManager::Shutdown() {
  if (initialized_) {
    Release();

    // Free all handlers.
    STLDeleteElements(&generated_code_handlers_);
    STLDeleteElements(&other_handlers_);
  }
}

bool FaultManager::HandleFaultByOtherHandlers(int sig, siginfo_t* info, void* context) {
  if (other_handlers_.empty()) {
    return false;
  }

  Thread* self = Thread::Current();

  DCHECK(self != nullptr);
  DCHECK(Runtime::Current() != nullptr);
  DCHECK(Runtime::Current()->IsStarted());
  for (const auto& handler : other_handlers_) {
    if (handler->Action(sig, info, context)) {
      return true;
    }
  }
  return false;
}

static const char* SignalCodeName(int sig, int code) {
  if (sig != SIGSEGV) {
    return "UNKNOWN";
  } else {
    switch (code) {
      case SEGV_MAPERR: return "SEGV_MAPERR";
      case SEGV_ACCERR: return "SEGV_ACCERR";
      default:          return "UNKNOWN";
    }
  }
}
static std::ostream& PrintSignalInfo(std::ostream& os, siginfo_t* info) {
  os << "  si_signo: " << info->si_signo << " (" << strsignal(info->si_signo) << ")\n"
     << "  si_code: " << info->si_code
     << " (" << SignalCodeName(info->si_signo, info->si_code) << ")";
  if (info->si_signo == SIGSEGV) {
    os << "\n" << "  si_addr: " << info->si_addr;
  }
  return os;
}

bool FaultManager::HandleFault(int sig, siginfo_t* info, void* context) {
  if (VLOG_IS_ON(signals)) {
    PrintSignalInfo(VLOG_STREAM(signals) << "Handling fault:" << "\n", info);
  }

#ifdef TEST_NESTED_SIGNAL
  // Simulate a crash in a handler.
  raise(SIGSEGV);
#endif

  if (IsInGeneratedCode(info, context, true)) {
    VLOG(signals) << "in generated code, looking for handler";
    for (const auto& handler : generated_code_handlers_) {
      VLOG(signals) << "invoking Action on handler " << handler;
      if (handler->Action(sig, info, context)) {
        // We have handled a signal so it's time to return from the
        // signal handler to the appropriate place.
        return true;
      }
    }
  }

  // We hit a signal we didn't handle.  This might be something for which
  // we can give more information about so call all registered handlers to
  // see if it is.
  if (HandleFaultByOtherHandlers(sig, info, context)) {
    return true;
  }

  // Set a breakpoint in this function to catch unhandled signals.
  art_sigsegv_fault();
  return false;
}

void FaultManager::AddHandler(FaultHandler* handler, bool generated_code) {
  DCHECK(initialized_);
  if (generated_code) {
    generated_code_handlers_.push_back(handler);
  } else {
    other_handlers_.push_back(handler);
  }
}

void FaultManager::RemoveHandler(FaultHandler* handler) {
  auto it = std::find(generated_code_handlers_.begin(), generated_code_handlers_.end(), handler);
  if (it != generated_code_handlers_.end()) {
    generated_code_handlers_.erase(it);
    return;
  }
  auto it2 = std::find(other_handlers_.begin(), other_handlers_.end(), handler);
  if (it2 != other_handlers_.end()) {
    other_handlers_.erase(it2);
    return;
  }
  LOG(FATAL) << "Attempted to remove non existent handler " << handler;
}

// This function is called within the signal handler.  It checks that
// the mutator_lock is held (shared).  No annotalysis is done.
bool FaultManager::IsInGeneratedCode(siginfo_t* siginfo, void* context, bool check_dex_pc) {
  // We can only be running Java code in the current thread if it
  // is in Runnable state.
  VLOG(signals) << "Checking for generated code";
  Thread* thread = Thread::Current();
  if (thread == nullptr) {
    VLOG(signals) << "no current thread";
    return false;
  }

  ThreadState state = thread->GetState();
  if (state != kRunnable) {
    VLOG(signals) << "not runnable";
    return false;
  }

  // Current thread is runnable.
  // Make sure it has the mutator lock.
  if (!Locks::mutator_lock_->IsSharedHeld(thread)) {
    VLOG(signals) << "no lock";
    return false;
  }

  ArtMethod* method_obj = nullptr;
  uintptr_t return_pc = 0;
  uintptr_t sp = 0;

  // Get the architecture specific method address and return address.  These
  // are in architecture specific files in arch/<arch>/fault_handler_<arch>.
  GetMethodAndReturnPcAndSp(siginfo, context, &method_obj, &return_pc, &sp);

  // If we don't have a potential method, we're outta here.
  VLOG(signals) << "potential method: " << method_obj;
  // TODO: Check linear alloc and image.
  DCHECK_ALIGNED(ArtMethod::Size(kRuntimePointerSize), sizeof(void*))
      << "ArtMethod is not pointer aligned";
  if (method_obj == nullptr || !IsAligned<sizeof(void*)>(method_obj)) {
    VLOG(signals) << "no method";
    return false;
  }

  // Verify that the potential method is indeed a method.
  // TODO: check the GC maps to make sure it's an object.
  // Check that the class pointer inside the object is not null and is aligned.
  // No read barrier because method_obj may not be a real object.
  mirror::Class* cls = SafeGetDeclaringClass(method_obj);
  if (cls == nullptr) {
    VLOG(signals) << "not a class";
    return false;
  }

  if (!IsAligned<kObjectAlignment>(cls)) {
    VLOG(signals) << "not aligned";
    return false;
  }

  if (!SafeVerifyClassClass(cls)) {
    VLOG(signals) << "not a class class";
    return false;
  }

  const OatQuickMethodHeader* method_header = method_obj->GetOatQuickMethodHeader(return_pc);

  // We can be certain that this is a method now.  Check if we have a GC map
  // at the return PC address.
  if (true || kIsDebugBuild) {
    VLOG(signals) << "looking for dex pc for return pc " << std::hex << return_pc;
    uint32_t sought_offset = return_pc -
        reinterpret_cast<uintptr_t>(method_header->GetEntryPoint());
    VLOG(signals) << "pc offset: " << std::hex << sought_offset;
  }
  uint32_t dexpc = method_header->ToDexPc(method_obj, return_pc, false);
  VLOG(signals) << "dexpc: " << dexpc;
  return !check_dex_pc || dexpc != dex::kDexNoIndex;
}

FaultHandler::FaultHandler(FaultManager* manager) : manager_(manager) {
}

//
// Null pointer fault handler
//
NullPointerHandler::NullPointerHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, true);
}

//
// Suspension fault handler
//
SuspensionHandler::SuspensionHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, true);
}

//
// Stack overflow fault handler
//
StackOverflowHandler::StackOverflowHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, true);
}

//
// Stack trace handler, used to help get a stack trace from SIGSEGV inside of compiled code.
//
JavaStackTraceHandler::JavaStackTraceHandler(FaultManager* manager) : FaultHandler(manager) {
  manager_->AddHandler(this, false);
}

bool JavaStackTraceHandler::Action(int sig ATTRIBUTE_UNUSED, siginfo_t* siginfo, void* context) {
  // Make sure that we are in the generated code, but we may not have a dex pc.
  bool in_generated_code = manager_->IsInGeneratedCode(siginfo, context, false);
  if (in_generated_code) {
    LOG(ERROR) << "Dumping java stack trace for crash in generated code";
    ArtMethod* method = nullptr;
    uintptr_t return_pc = 0;
    uintptr_t sp = 0;
    Thread* self = Thread::Current();

    manager_->GetMethodAndReturnPcAndSp(siginfo, context, &method, &return_pc, &sp);
    // Inside of generated code, sp[0] is the method, so sp is the frame.
    self->SetTopOfStack(reinterpret_cast<ArtMethod**>(sp));
    self->DumpJavaStack(LOG_STREAM(ERROR));
  }

  return false;  // Return false since we want to propagate the fault to the main signal handler.
}

}   // namespace art
