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

// A simple implementation of the native-bridge interface.

#include <dlfcn.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <jni.h>
#include <nativebridge/native_bridge.h>

#include "base/macros.h"

struct NativeBridgeMethod {
  const char* name;
  const char* signature;
  bool static_method;
  void* fnPtr;
  void* trampoline;
};

static NativeBridgeMethod* find_native_bridge_method(const char *name);
static const android::NativeBridgeRuntimeCallbacks* gNativeBridgeArtCallbacks;

static jint trampoline_JNI_OnLoad(JavaVM* vm, void* reserved) {
  JNIEnv* env = nullptr;
  typedef jint (*FnPtr_t)(JavaVM*, void*);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>(find_native_bridge_method("JNI_OnLoad")->fnPtr);

  vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
  if (env == nullptr) {
    return 0;
  }

  jclass klass = env->FindClass("Main");
  if (klass != nullptr) {
    int i, count1, count2;
    count1 = gNativeBridgeArtCallbacks->getNativeMethodCount(env, klass);
    std::unique_ptr<JNINativeMethod[]> methods(new JNINativeMethod[count1]);
    if (methods == nullptr) {
      return 0;
    }
    count2 = gNativeBridgeArtCallbacks->getNativeMethods(env, klass, methods.get(), count1);
    if (count1 == count2) {
      printf("Test ART callbacks: all JNI function number is %d.\n", count1);
    }

    for (i = 0; i < count1; i++) {
      NativeBridgeMethod* nb_method = find_native_bridge_method(methods[i].name);
      if (nb_method != nullptr) {
        jmethodID mid = nullptr;
        if (nb_method->static_method) {
          mid = env->GetStaticMethodID(klass, methods[i].name, nb_method->signature);
        } else {
          mid = env->GetMethodID(klass, methods[i].name, nb_method->signature);
        }
        if (mid != nullptr) {
          const char* shorty = gNativeBridgeArtCallbacks->getMethodShorty(env, mid);
          if (strcmp(shorty, methods[i].signature) == 0) {
            printf("    name:%s, signature:%s, shorty:%s.\n",
                   methods[i].name, nb_method->signature, shorty);
          }
        }
      }
    }
    methods.release();
  }

  printf("%s called!\n", __FUNCTION__);
  return fnPtr(vm, reserved);
}

static void trampoline_Java_Main_testFindClassOnAttachedNativeThread(JNIEnv* env,
                                                                     jclass klass) {
  typedef void (*FnPtr_t)(JNIEnv*, jclass);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>
    (find_native_bridge_method("testFindClassOnAttachedNativeThread")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass);
}

static void trampoline_Java_Main_testFindFieldOnAttachedNativeThreadNative(JNIEnv* env,
                                                                           jclass klass) {
  typedef void (*FnPtr_t)(JNIEnv*, jclass);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>
    (find_native_bridge_method("testFindFieldOnAttachedNativeThreadNative")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass);
}

static void trampoline_Java_Main_testCallStaticVoidMethodOnSubClassNative(JNIEnv* env,
                                                                          jclass klass) {
  typedef void (*FnPtr_t)(JNIEnv*, jclass);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>
    (find_native_bridge_method("testCallStaticVoidMethodOnSubClassNative")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass);
}

static jobject trampoline_Java_Main_testGetMirandaMethodNative(JNIEnv* env, jclass klass) {
  typedef jobject (*FnPtr_t)(JNIEnv*, jclass);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>
    (find_native_bridge_method("testGetMirandaMethodNative")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass);
}

static void trampoline_Java_Main_testNewStringObject(JNIEnv* env, jclass klass) {
  typedef void (*FnPtr_t)(JNIEnv*, jclass);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>
    (find_native_bridge_method("testNewStringObject")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass);
}

static void trampoline_Java_Main_testZeroLengthByteBuffers(JNIEnv* env, jclass klass) {
  typedef void (*FnPtr_t)(JNIEnv*, jclass);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>
    (find_native_bridge_method("testZeroLengthByteBuffers")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass);
}

static jbyte trampoline_Java_Main_byteMethod(JNIEnv* env, jclass klass, jbyte b1, jbyte b2,
                                             jbyte b3, jbyte b4, jbyte b5, jbyte b6,
                                             jbyte b7, jbyte b8, jbyte b9, jbyte b10) {
  typedef jbyte (*FnPtr_t)(JNIEnv*, jclass, jbyte, jbyte, jbyte, jbyte, jbyte,
                           jbyte, jbyte, jbyte, jbyte, jbyte);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>(find_native_bridge_method("byteMethod")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10);
}

static jshort trampoline_Java_Main_shortMethod(JNIEnv* env, jclass klass, jshort s1, jshort s2,
                                               jshort s3, jshort s4, jshort s5, jshort s6,
                                               jshort s7, jshort s8, jshort s9, jshort s10) {
  typedef jshort (*FnPtr_t)(JNIEnv*, jclass, jshort, jshort, jshort, jshort, jshort,
                            jshort, jshort, jshort, jshort, jshort);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>(find_native_bridge_method("shortMethod")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10);
}

static jboolean trampoline_Java_Main_booleanMethod(JNIEnv* env, jclass klass, jboolean b1,
                                                   jboolean b2, jboolean b3, jboolean b4,
                                                   jboolean b5, jboolean b6, jboolean b7,
                                                   jboolean b8, jboolean b9, jboolean b10) {
  typedef jboolean (*FnPtr_t)(JNIEnv*, jclass, jboolean, jboolean, jboolean, jboolean, jboolean,
                              jboolean, jboolean, jboolean, jboolean, jboolean);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>(find_native_bridge_method("booleanMethod")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10);
}

static jchar trampoline_Java_Main_charMethod(JNIEnv* env, jclass klass, jchar c1, jchar c2,
                                             jchar c3, jchar c4, jchar c5, jchar c6,
                                             jchar c7, jchar c8, jchar c9, jchar c10) {
  typedef jchar (*FnPtr_t)(JNIEnv*, jclass, jchar, jchar, jchar, jchar, jchar,
                           jchar, jchar, jchar, jchar, jchar);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>(find_native_bridge_method("charMethod")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10);
}

// This code is adapted from 004-SignalTest and causes a segfault.
char *go_away_compiler = nullptr;

[[ noreturn ]] static void test_sigaction_handler(int sig ATTRIBUTE_UNUSED,
                                                  siginfo_t* info ATTRIBUTE_UNUSED,
                                                  void* context ATTRIBUTE_UNUSED) {
  printf("Should not reach the test sigaction handler.");
  abort();
}

static void raise_sigsegv() {
#if defined(__arm__) || defined(__i386__) || defined(__aarch64__)
  *go_away_compiler = 'a';
#elif defined(__x86_64__)
  // Cause a SEGV using an instruction known to be 2 bytes long to account for hardcoded jump
  // in the signal handler
  asm volatile("movl $0, %%eax;" "movb %%ah, (%%rax);" : : : "%eax");
#else
  // On other architectures we simulate SEGV.
  kill(getpid(), SIGSEGV);
#endif
}

static jint trampoline_Java_Main_testSignal(JNIEnv*, jclass) {
  // Install the sigaction handler above, which should *not* be reached as the native-bridge
  // handler should be called first. Note: we won't chain at all, if we ever get here, we'll die.
  struct sigaction tmp;
  sigemptyset(&tmp.sa_mask);
  tmp.sa_sigaction = test_sigaction_handler;
#if !defined(__APPLE__) && !defined(__mips__)
  tmp.sa_restorer = nullptr;
#endif

  // Test segv
  sigaction(SIGSEGV, &tmp, nullptr);
  raise_sigsegv();

  // Test sigill
  sigaction(SIGILL, &tmp, nullptr);
  kill(getpid(), SIGILL);

#if defined(__BIONIC__)
  // Do the same again, but with sigaction64.
  struct sigaction64 tmp2;
  sigemptyset64(&tmp2.sa_mask);
  tmp2.sa_sigaction = test_sigaction_handler;
  tmp2.sa_restorer = nullptr;

  sigaction64(SIGSEGV, &tmp2, nullptr);
  sigaction64(SIGILL, &tmp2, nullptr);
#endif

  // Reraise SIGSEGV/SIGILL even on non-bionic, so that the expected output is
  // the same.
  raise_sigsegv();
  kill(getpid(), SIGILL);

  return 1234;
}

// Status of the tricky control path of testSignalHandlerNotReturn.
//
// "kNone" is the default status except testSignalHandlerNotReturn,
// others are used by testSignalHandlerNotReturn.
enum class TestStatus {
  kNone,
  kRaiseFirst,
  kHandleFirst,
  kRaiseSecond,
  kHandleSecond,
};

// State transition helper for testSignalHandlerNotReturn.
class SignalHandlerTestStatus {
 public:
  SignalHandlerTestStatus() : state_(TestStatus::kNone) {
  }

  TestStatus Get() {
    return state_;
  }

  void Reset() {
    Set(TestStatus::kNone);
  }

  void Set(TestStatus state) {
    switch (state) {
      case TestStatus::kNone:
        AssertState(TestStatus::kHandleSecond);
        break;

      case TestStatus::kRaiseFirst:
        AssertState(TestStatus::kNone);
        break;

      case TestStatus::kHandleFirst:
        AssertState(TestStatus::kRaiseFirst);
        break;

      case TestStatus::kRaiseSecond:
        AssertState(TestStatus::kHandleFirst);
        break;

      case TestStatus::kHandleSecond:
        AssertState(TestStatus::kRaiseSecond);
        break;

      default:
        printf("ERROR: unknown state\n");
        abort();
    }

    state_ = state;
  }

 private:
  TestStatus state_;

  void AssertState(TestStatus expected) {
    if (state_ != expected) {
      printf("ERROR: unexpected state, was %d, expected %d\n", state_, expected);
    }
  }
};

static SignalHandlerTestStatus gSignalTestStatus;
// The context is used to jump out from signal handler.
static sigjmp_buf gSignalTestJmpBuf;

// Test whether NativeBridge can receive future signal when its handler doesn't return.
//
// Control path:
//  1. Raise first SIGSEGV in test function.
//  2. Raise another SIGSEGV in NativeBridge's signal handler which is handling
//     the first SIGSEGV.
//  3. Expect that NativeBridge's signal handler invokes again. And jump back
//     to test function in when handling second SIGSEGV.
//  4. Exit test.
//
// NOTE: sigchain should be aware that "special signal handler" may not return.
//       Pay attention if this case fails.
static void trampoline_Java_Main_testSignalHandlerNotReturn(JNIEnv*, jclass) {
  if (gSignalTestStatus.Get() != TestStatus::kNone) {
    printf("ERROR: test already started?\n");
    return;
  }
  printf("start testSignalHandlerNotReturn\n");

  if (sigsetjmp(gSignalTestJmpBuf, 1) == 0) {
    gSignalTestStatus.Set(TestStatus::kRaiseFirst);
    printf("raising first SIGSEGV\n");
    raise_sigsegv();
  } else {
    // jump to here from signal handler when handling second SIGSEGV.
    if (gSignalTestStatus.Get() != TestStatus::kHandleSecond) {
      printf("ERROR: not jump from second SIGSEGV?\n");
      return;
    }
    gSignalTestStatus.Reset();
    printf("back to test from signal handler via siglongjmp(), and done!\n");
  }
}

// Signal handler for testSignalHandlerNotReturn.
// This handler won't return.
static bool NotReturnSignalHandler() {
  if (gSignalTestStatus.Get() == TestStatus::kRaiseFirst) {
    // handling first SIGSEGV
    gSignalTestStatus.Set(TestStatus::kHandleFirst);
    printf("handling first SIGSEGV, will raise another\n");
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGSEGV);
    printf("unblock SIGSEGV in handler\n");
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
    gSignalTestStatus.Set(TestStatus::kRaiseSecond);
    printf("raising second SIGSEGV\n");
    raise_sigsegv();    // raise second SIGSEGV
  } else if (gSignalTestStatus.Get() == TestStatus::kRaiseSecond) {
    // handling second SIGSEGV
    gSignalTestStatus.Set(TestStatus::kHandleSecond);
    printf("handling second SIGSEGV, will jump back to test function\n");
    siglongjmp(gSignalTestJmpBuf, 1);
  }
  printf("ERROR: should not reach here!\n");
  return false;
}

NativeBridgeMethod gNativeBridgeMethods[] = {
  { "JNI_OnLoad", "", true, nullptr,
    reinterpret_cast<void*>(trampoline_JNI_OnLoad) },
  { "booleanMethod", "(ZZZZZZZZZZ)Z", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_booleanMethod) },
  { "byteMethod", "(BBBBBBBBBB)B", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_byteMethod) },
  { "charMethod", "(CCCCCCCCCC)C", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_charMethod) },
  { "shortMethod", "(SSSSSSSSSS)S", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_shortMethod) },
  { "testCallStaticVoidMethodOnSubClassNative", "()V", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testCallStaticVoidMethodOnSubClassNative) },
  { "testFindClassOnAttachedNativeThread", "()V", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testFindClassOnAttachedNativeThread) },
  { "testFindFieldOnAttachedNativeThreadNative", "()V", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testFindFieldOnAttachedNativeThreadNative) },
  { "testGetMirandaMethodNative", "()Ljava/lang/reflect/Method;", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testGetMirandaMethodNative) },
  { "testNewStringObject", "()V", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testNewStringObject) },
  { "testZeroLengthByteBuffers", "()V", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testZeroLengthByteBuffers) },
  { "testSignal", "()I", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testSignal) },
  { "testSignalHandlerNotReturn", "()V", true, nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testSignalHandlerNotReturn) },
};

static NativeBridgeMethod* find_native_bridge_method(const char *name) {
  const char* pname = name;
  if (strncmp(name, "Java_Main_", 10) == 0) {
    pname += 10;
  }

  for (size_t i = 0; i < sizeof(gNativeBridgeMethods) / sizeof(gNativeBridgeMethods[0]); i++) {
    if (strcmp(pname, gNativeBridgeMethods[i].name) == 0) {
      return &gNativeBridgeMethods[i];
    }
  }
  return nullptr;
}

// NativeBridgeCallbacks implementations
extern "C" bool native_bridge_initialize(const android::NativeBridgeRuntimeCallbacks* art_cbs,
                                         const char* app_code_cache_dir,
                                         const char* isa ATTRIBUTE_UNUSED) {
  struct stat st;
  if (app_code_cache_dir != nullptr) {
    if (stat(app_code_cache_dir, &st) == 0) {
      if (!S_ISDIR(st.st_mode)) {
        printf("Code cache is not a directory.\n");
      }
    } else {
      perror("Error when stat-ing the code_cache:");
    }
  }

  if (art_cbs != nullptr) {
    gNativeBridgeArtCallbacks = art_cbs;
    printf("Native bridge initialized.\n");
  }
  return true;
}

extern "C" void* native_bridge_loadLibrary(const char* libpath, int flag) {
  if (strstr(libpath, "libinvalid.so") != nullptr) {
    printf("Was to load 'libinvalid.so', force fail.\n");
    return nullptr;
  }
  size_t len = strlen(libpath);
  char* tmp = new char[len + 10];
  strncpy(tmp, libpath, len);
  tmp[len - 3] = '2';
  tmp[len - 2] = '.';
  tmp[len - 1] = 's';
  tmp[len] = 'o';
  tmp[len + 1] = 0;
  void* handle = dlopen(tmp, flag);
  delete[] tmp;

  if (handle == nullptr) {
    printf("Handle = nullptr!\n");
    printf("Was looking for %s.\n", libpath);
    printf("Error = %s.\n", dlerror());
    char cwd[1024] = {'\0'};
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
      printf("Current working dir: %s\n", cwd);
    }
  }
  return handle;
}

extern "C" void* native_bridge_getTrampoline(void* handle, const char* name, const char* shorty,
                                             uint32_t len ATTRIBUTE_UNUSED) {
  printf("Getting trampoline for %s with shorty %s.\n", name, shorty);

  // The name here is actually the JNI name, so we can directly do the lookup.
  void* sym = dlsym(handle, name);
  NativeBridgeMethod* method = find_native_bridge_method(name);
  if (method == nullptr)
    return nullptr;
  method->fnPtr = sym;

  return method->trampoline;
}

extern "C" bool native_bridge_isSupported(const char* libpath) {
  printf("Checking for support.\n");

  if (libpath == nullptr) {
    return false;
  }
  // We don't want to hijack javacore. So we should get libarttest...
  return strcmp(libpath, "libjavacore.so") != 0;
}

namespace android {

// Environment values required by the apps running with native bridge.
struct NativeBridgeRuntimeValues {
  const char* os_arch;
  const char* cpu_abi;
  const char* cpu_abi2;
  const char* *supported_abis;
  int32_t abi_count;
};

}  // namespace android

const char* supported_abis[] = {
    "supported1", "supported2", "supported3"
};

const struct android::NativeBridgeRuntimeValues nb_env {
    .os_arch = "os.arch",
    .cpu_abi = "cpu_abi",
    .cpu_abi2 = "cpu_abi2",
    .supported_abis = supported_abis,
    .abi_count = 3
};

extern "C" const struct android::NativeBridgeRuntimeValues* native_bridge_getAppEnv(
    const char* abi) {
  printf("Checking for getEnvValues.\n");

  if (abi == nullptr) {
    return nullptr;
  }

  return &nb_env;
}

// v2 parts.

extern "C" bool native_bridge_isCompatibleWith(uint32_t bridge_version ATTRIBUTE_UNUSED) {
  return true;
}

#if defined(__i386__) || defined(__x86_64__)
#if defined(__APPLE__)
#define ucontext __darwin_ucontext

#if defined(__x86_64__)
// 64 bit mac build.
#define CTX_EIP uc_mcontext->__ss.__rip
#else
// 32 bit mac build.
#define CTX_EIP uc_mcontext->__ss.__eip
#endif

#elif defined(__x86_64__)
// 64 bit linux build.
#define CTX_EIP uc_mcontext.gregs[REG_RIP]
#else
// 32 bit linux build.
#define CTX_EIP uc_mcontext.gregs[REG_EIP]
#endif
#endif

static bool StandardSignalHandler(int sig, siginfo_t* info ATTRIBUTE_UNUSED,
                                     void* context) {
  if (sig == SIGSEGV) {
#if defined(__arm__)
    struct ucontext *uc = reinterpret_cast<struct ucontext*>(context);
    struct sigcontext *sc = reinterpret_cast<struct sigcontext*>(&uc->uc_mcontext);
    sc->arm_pc += 2;          // Skip instruction causing segv & sigill.
#elif defined(__aarch64__)
    struct ucontext *uc = reinterpret_cast<struct ucontext*>(context);
    struct sigcontext *sc = reinterpret_cast<struct sigcontext*>(&uc->uc_mcontext);
    sc->pc += 4;          // Skip instruction causing segv & sigill.
#elif defined(__i386__)
    struct ucontext *uc = reinterpret_cast<struct ucontext*>(context);
    uc->CTX_EIP += 3;
#elif defined(__x86_64__)
    struct ucontext *uc = reinterpret_cast<struct ucontext*>(context);
    uc->CTX_EIP += 2;
#else
    UNUSED(context);
#endif
  }

  // We handled this...
  return true;
}

// A dummy special handler, continueing after the faulting location. This code comes from
// 004-SignalTest.
static bool nb_signalhandler(int sig, siginfo_t* info, void* context) {
  printf("NB signal handler with signal %d.\n", sig);

  if (gSignalTestStatus.Get() == TestStatus::kNone) {
    return StandardSignalHandler(sig, info, context);
  } else if (sig == SIGSEGV) {
    return NotReturnSignalHandler();
  } else {
    printf("ERROR: should not reach here!\n");
    return false;
  }
}

static ::android::NativeBridgeSignalHandlerFn native_bridge_getSignalHandler(int signal) {
  // Test segv for already claimed signal, and sigill for not claimed signal
  if ((signal == SIGSEGV) || (signal == SIGILL)) {
    return &nb_signalhandler;
  }
  return nullptr;
}

extern "C" int native_bridge_unloadLibrary(void* handle ATTRIBUTE_UNUSED) {
  printf("dlclose() in native bridge.\n");
  return 0;
}

extern "C" const char* native_bridge_getError() {
  printf("getError() in native bridge.\n");
  return "";
}

extern "C" bool native_bridge_isPathSupported(const char* library_path ATTRIBUTE_UNUSED) {
  printf("Checking for path support in native bridge.\n");
  return false;
}

extern "C" bool native_bridge_initAnonymousNamespace(const char* public_ns_sonames ATTRIBUTE_UNUSED,
                                                     const char* anon_ns_library_path ATTRIBUTE_UNUSED) {
  printf("Initializing anonymous namespace in native bridge.\n");
  return false;
}

extern "C" android::native_bridge_namespace_t*
native_bridge_createNamespace(const char* name ATTRIBUTE_UNUSED,
                              const char* ld_library_path ATTRIBUTE_UNUSED,
                              const char* default_library_path ATTRIBUTE_UNUSED,
                              uint64_t type ATTRIBUTE_UNUSED,
                              const char* permitted_when_isolated_path ATTRIBUTE_UNUSED,
                              android::native_bridge_namespace_t* parent_ns ATTRIBUTE_UNUSED) {
  printf("Creating namespace in native bridge.\n");
  return nullptr;
}

extern "C" bool native_bridge_linkNamespaces(android::native_bridge_namespace_t* from ATTRIBUTE_UNUSED,
                                             android::native_bridge_namespace_t* to ATTRIBUTE_UNUSED,
                                             const char* shared_libs_sonames ATTRIBUTE_UNUSED) {
  printf("Linking namespaces in native bridge.\n");
  return false;
}

extern "C" void* native_bridge_loadLibraryExt(const char* libpath ATTRIBUTE_UNUSED,
                                               int flag ATTRIBUTE_UNUSED,
                                               android::native_bridge_namespace_t* ns ATTRIBUTE_UNUSED) {
    printf("Loading library with Extension in native bridge.\n");
    return nullptr;
}

// "NativeBridgeItf" is effectively an API (it is the name of the symbol that will be loaded
// by the native bridge library).
android::NativeBridgeCallbacks NativeBridgeItf {
  // v1
  .version = 3,
  .initialize = &native_bridge_initialize,
  .loadLibrary = &native_bridge_loadLibrary,
  .getTrampoline = &native_bridge_getTrampoline,
  .isSupported = &native_bridge_isSupported,
  .getAppEnv = &native_bridge_getAppEnv,
  // v2
  .isCompatibleWith = &native_bridge_isCompatibleWith,
  .getSignalHandler = &native_bridge_getSignalHandler,
  // v3
  .unloadLibrary = &native_bridge_unloadLibrary,
  .getError = &native_bridge_getError,
  .isPathSupported = &native_bridge_isPathSupported,
  .initAnonymousNamespace = &native_bridge_initAnonymousNamespace,
  .createNamespace = &native_bridge_createNamespace,
  .linkNamespaces = &native_bridge_linkNamespaces,
  .loadLibraryExt = &native_bridge_loadLibraryExt
};
