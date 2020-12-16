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

#include "check_jni.h"

#include <sys/mman.h>
#include <zlib.h>

#include <iomanip>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/macros.h"
#include "base/to_str.h"
#include "base/time_utils.h"
#include "class_linker-inl.h"
#include "class_linker.h"
#include "dex/descriptors_names.h"
#include "dex/dex_file-inl.h"
#include "gc/space/space.h"
#include "java_vm_ext.h"
#include "jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/string-inl.h"
#include "mirror/throwable.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "well_known_classes.h"

namespace art {
namespace {

using android::base::StringAppendF;
using android::base::StringPrintf;

/*
 * ===========================================================================
 *      JNI function helpers
 * ===========================================================================
 */

// Warn if a JNI critical is held for longer than 16ms.
static constexpr uint64_t kCriticalWarnTimeUs = MsToUs(16);
static_assert(kCriticalWarnTimeUs > 0, "No JNI critical warn time set");

// True if primitives within specific ranges cause a fatal error,
// otherwise just warn.
static constexpr bool kBrokenPrimitivesAreFatal = kIsDebugBuild;

// Flags passed into ScopedCheck.
static constexpr uint16_t kFlag_Default = 0x0000;

// Calling while in critical is not allowed.
static constexpr uint16_t kFlag_CritBad = 0x0000;
// Calling while in critical is allowed.
static constexpr uint16_t kFlag_CritOkay = 0x0001;
// This is a critical "get".
static constexpr uint16_t kFlag_CritGet = 0x0002;
// This is a critical "release".
static constexpr uint16_t kFlag_CritRelease = 0x0003;
// Bit mask to get "crit" value.
static constexpr uint16_t kFlag_CritMask = 0x0003;

// Raised exceptions are allowed.
static constexpr uint16_t kFlag_ExcepOkay = 0x0004;

// Are we in a non-critical release function?
static constexpr uint16_t kFlag_Release = 0x0010;
// Are our UTF parameters nullable?
static constexpr uint16_t kFlag_NullableUtf = 0x0020;

// Part of the invocation interface (JavaVM*).
static constexpr uint16_t kFlag_Invocation = 0x0100;

// Add this to a JNI function's flags if you want to trace every call.
static constexpr uint16_t kFlag_ForceTrace = 0x8000;

class VarArgs;
/*
 * Java primitive types:
 * B - jbyte
 * C - jchar
 * D - jdouble
 * F - jfloat
 * I - jint
 * J - jlong
 * S - jshort
 * Z - jboolean (shown as true and false)
 * V - void
 *
 * Java reference types:
 * L - jobject
 * a - jarray
 * c - jclass
 * s - jstring
 * t - jthrowable
 *
 * JNI types:
 * b - jboolean (shown as JNI_TRUE and JNI_FALSE)
 * f - jfieldID
 * i - JNI error value (JNI_OK, JNI_ERR, JNI_EDETACHED, JNI_EVERSION)
 * m - jmethodID
 * p - void*
 * r - jint (for release mode arguments)
 * u - const char* (Modified UTF-8)
 * z - jsize (for lengths; use i if negative values are okay)
 * v - JavaVM*
 * w - jobjectRefType
 * E - JNIEnv*
 * . - no argument; just print "..." (used for varargs JNI calls)
 *
 */
union JniValueType {
  jarray a;
  jboolean b;
  jclass c;
  jfieldID f;
  jint i;
  jmethodID m;
  const void* p;  // Pointer.
  jint r;  // Release mode.
  jstring s;
  jthrowable t;
  const char* u;  // Modified UTF-8.
  JavaVM* v;
  jobjectRefType w;
  jsize z;
  jbyte B;
  jchar C;
  jdouble D;
  JNIEnv* E;
  jfloat F;
  jint I;
  jlong J;
  jobject L;
  jshort S;
  const void* V;  // void
  jboolean Z;
  const VarArgs* va;
};

/*
 * A structure containing all the information needed to validate varargs arguments.
 *
 * Note that actually getting the arguments from this structure mutates it so should only be done on
 * owned copies.
 */
class VarArgs {
 public:
  VarArgs(jmethodID m, va_list var) : m_(m), type_(kTypeVaList), cnt_(0) {
    va_copy(vargs_, var);
  }

  VarArgs(jmethodID m, const jvalue* vals) : m_(m), type_(kTypePtr), cnt_(0), ptr_(vals) {}

  ~VarArgs() {
    if (type_ == kTypeVaList) {
      va_end(vargs_);
    }
  }

  VarArgs(VarArgs&& other) {
    m_ = other.m_;
    cnt_ = other.cnt_;
    type_ = other.type_;
    if (other.type_ == kTypeVaList) {
      va_copy(vargs_, other.vargs_);
    } else {
      ptr_ = other.ptr_;
    }
  }

  // This method is const because we need to ensure that one only uses the GetValue method on an
  // owned copy of the VarArgs. This is because getting the next argument from a va_list is a
  // mutating operation. Therefore we pass around these VarArgs with the 'const' qualifier and when
  // we want to use one we need to Clone() it.
  VarArgs Clone() const {
    if (type_ == kTypeVaList) {
      // const_cast needed to make sure the compiler is okay with va_copy, which (being a macro) is
      // messed up if the source argument is not the exact type 'va_list'.
      return VarArgs(m_, cnt_, const_cast<VarArgs*>(this)->vargs_);
    } else {
      return VarArgs(m_, cnt_, ptr_);
    }
  }

  jmethodID GetMethodID() const {
    return m_;
  }

  JniValueType GetValue(char fmt) {
    JniValueType o;
    if (type_ == kTypeVaList) {
      switch (fmt) {
        // Assign a full int for va_list values as this is what is done in reflection.cc.
        // TODO(b/73656264): avoid undefined behavior.
        case 'Z': FALLTHROUGH_INTENDED;
        case 'B': FALLTHROUGH_INTENDED;
        case 'C': FALLTHROUGH_INTENDED;
        case 'S': FALLTHROUGH_INTENDED;
        case 'I': o.I = va_arg(vargs_, jint); break;
        case 'J': o.J = va_arg(vargs_, jlong); break;
        case 'F': o.F = static_cast<jfloat>(va_arg(vargs_, jdouble)); break;
        case 'D': o.D = va_arg(vargs_, jdouble); break;
        case 'L': o.L = va_arg(vargs_, jobject); break;
        default:
          LOG(FATAL) << "Illegal type format char " << fmt;
          UNREACHABLE();
      }
    } else {
      CHECK(type_ == kTypePtr);
      jvalue v = ptr_[cnt_];
      cnt_++;
      switch (fmt) {
        // Copy just the amount of the jvalue necessary, as done in
        // reflection.cc, but extend to an int to be consistent with
        // var args in CheckNonHeapValue.
        // TODO(b/73656264): avoid undefined behavior.
        case 'Z': o.I = v.z; break;
        case 'B': o.I = v.b; break;
        case 'C': o.I = v.c; break;
        case 'S': o.I = v.s; break;
        case 'I': o.I = v.i; break;
        case 'J': o.J = v.j; break;
        case 'F': o.F = v.f; break;
        case 'D': o.D = v.d; break;
        case 'L': o.L = v.l; break;
        default:
          LOG(FATAL) << "Illegal type format char " << fmt;
          UNREACHABLE();
      }
    }
    return o;
  }

 private:
  VarArgs(jmethodID m, uint32_t cnt, va_list var) : m_(m), type_(kTypeVaList), cnt_(cnt) {
    va_copy(vargs_, var);
  }

  VarArgs(jmethodID m, uint32_t cnt, const jvalue* vals) : m_(m), type_(kTypePtr), cnt_(cnt), ptr_(vals) {}

  enum VarArgsType {
    kTypeVaList,
    kTypePtr,
  };

  jmethodID m_;
  VarArgsType type_;
  uint32_t cnt_;
  union {
    va_list vargs_;
    const jvalue* ptr_;
  };
};

// Check whether the current thread is attached. This is usually required
// to be the first check, as ScopedCheck needs a ScopedObjectAccess for
// checking heap values (and that will fail with unattached threads).
bool CheckAttachedThread(const char* function_name) {
  Thread* self = Thread::Current();
  if (UNLIKELY(self == nullptr)) {
    // Need to attach this thread for a proper abort to work. We prefer this
    // to get reasonable stacks and environment, rather than relying on
    // tombstoned.
    JNIEnv* env;
    Runtime::Current()->GetJavaVM()->AttachCurrentThread(&env, /* thread_args */ nullptr);

    std::string tmp = android::base::StringPrintf(
        "a thread (tid %" PRId64 " is making JNI calls without being attached",
        static_cast<int64_t>(GetTid()));
    Runtime::Current()->GetJavaVM()->JniAbort(function_name, tmp.c_str());

    CHECK_NE(Runtime::Current()->GetJavaVM()->DetachCurrentThread(), JNI_ERR);
    return false;
  }
  return true;
}

// Macro helpers for the above.
#define CHECK_ATTACHED_THREAD(function_name, fail_val)  \
  do {                                                  \
    if (!CheckAttachedThread((function_name))) {        \
      return fail_val;                                  \
    }                                                   \
  } while (false)
#define CHECK_ATTACHED_THREAD_VOID(function_name)       \
  do {                                                  \
    if (!CheckAttachedThread((function_name))) {        \
      return;                                           \
    }                                                   \
  } while (false)

class ScopedCheck {
 public:
  ScopedCheck(uint16_t flags, const char* functionName, bool has_method = true)
      : function_name_(functionName), indent_(0), flags_(flags), has_method_(has_method) {
  }

  ~ScopedCheck() {}

  // Checks that 'class_name' is a valid "fully-qualified" JNI class name, like "java/lang/Thread"
  // or "[Ljava/lang/Object;". A ClassLoader can actually normalize class names a couple of
  // times, so using "java.lang.Thread" instead of "java/lang/Thread" might work in some
  // circumstances, but this is incorrect.
  bool CheckClassName(const char* class_name) {
    if ((class_name == nullptr) || !IsValidJniClassName(class_name)) {
      AbortF("illegal class name '%s'\n"
             "    (should be of the form 'package/Class', [Lpackage/Class;' or '[[B')",
             class_name);
      return false;
    }
    return true;
  }

  /*
   * Verify that this instance field ID is valid for this object.
   *
   * Assumes "jobj" has already been validated.
   */
  bool CheckInstanceFieldID(ScopedObjectAccess& soa, jobject java_object, jfieldID fid)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ObjPtr<mirror::Object> o = soa.Decode<mirror::Object>(java_object);
    if (o == nullptr) {
      AbortF("field operation on NULL object: %p", java_object);
      return false;
    }
    if (!Runtime::Current()->GetHeap()->IsValidObjectAddress(o.Ptr())) {
      Runtime::Current()->GetHeap()->DumpSpaces(LOG_STREAM(ERROR));
      AbortF("field operation on invalid %s: %p",
             GetIndirectRefKindString(IndirectReferenceTable::GetIndirectRefKind(java_object)),
             java_object);
      return false;
    }

    ArtField* f = CheckFieldID(fid);
    if (f == nullptr) {
      return false;
    }
    mirror::Class* c = o->GetClass();
    if (c->FindInstanceField(f->GetName(), f->GetTypeDescriptor()) == nullptr) {
      AbortF("jfieldID %s not valid for an object of class %s",
             f->PrettyField().c_str(), o->PrettyTypeOf().c_str());
      return false;
    }
    return true;
  }

  /*
   * Verify that the pointer value is non-null.
   */
  bool CheckNonNull(const void* ptr) {
    if (UNLIKELY(ptr == nullptr)) {
      AbortF("non-nullable argument was NULL");
      return false;
    }
    return true;
  }

  /*
   * Verify that the method's return type matches the type of call.
   * 'expectedType' will be "L" for all objects, including arrays.
   */
  bool CheckMethodAndSig(ScopedObjectAccess& soa, jobject jobj, jclass jc,
                         jmethodID mid, Primitive::Type type, InvokeType invoke)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = CheckMethodID(mid);
    if (m == nullptr) {
      return false;
    }
    if (type != Primitive::GetType(m->GetShorty()[0])) {
      AbortF("the return type of %s does not match %s", function_name_, m->PrettyMethod().c_str());
      return false;
    }
    bool is_static = (invoke == kStatic);
    if (is_static != m->IsStatic()) {
      if (is_static) {
        AbortF("calling non-static method %s with %s",
               m->PrettyMethod().c_str(), function_name_);
      } else {
        AbortF("calling static method %s with %s",
               m->PrettyMethod().c_str(), function_name_);
      }
      return false;
    }
    if (invoke != kVirtual) {
      ObjPtr<mirror::Class> c = soa.Decode<mirror::Class>(jc);
      if (!m->GetDeclaringClass()->IsAssignableFrom(c)) {
        AbortF("can't call %s %s with class %s", invoke == kStatic ? "static" : "nonvirtual",
            m->PrettyMethod().c_str(), mirror::Class::PrettyClass(c).c_str());
        return false;
      }
    }
    if (invoke != kStatic) {
      ObjPtr<mirror::Object> o = soa.Decode<mirror::Object>(jobj);
      if (o == nullptr) {
        AbortF("can't call %s on null object", m->PrettyMethod().c_str());
        return false;
      } else if (!o->InstanceOf(m->GetDeclaringClass())) {
        AbortF("can't call %s on instance of %s", m->PrettyMethod().c_str(),
               o->PrettyTypeOf().c_str());
        return false;
      }
    }
    return true;
  }

  /*
   * Verify that this static field ID is valid for this class.
   *
   * Assumes "java_class" has already been validated.
   */
  bool CheckStaticFieldID(ScopedObjectAccess& soa, jclass java_class, jfieldID fid)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ObjPtr<mirror::Class> c = soa.Decode<mirror::Class>(java_class);
    ArtField* f = CheckFieldID(fid);
    if (f == nullptr) {
      return false;
    }
    if (!f->GetDeclaringClass()->IsAssignableFrom(c)) {
      AbortF("static jfieldID %p not valid for class %s", fid,
             mirror::Class::PrettyClass(c).c_str());
      return false;
    }
    return true;
  }

  /*
   * Verify that "mid" is appropriate for "java_class".
   *
   * A mismatch isn't dangerous, because the jmethodID defines the class.  In
   * fact, java_class is unused in the implementation.  It's best if we don't
   * allow bad code in the system though.
   *
   * Instances of "java_class" must be instances of the method's declaring class.
   */
  bool CheckStaticMethod(ScopedObjectAccess& soa, jclass java_class, jmethodID mid)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = CheckMethodID(mid);
    if (m == nullptr) {
      return false;
    }
    ObjPtr<mirror::Class> c = soa.Decode<mirror::Class>(java_class);
    if (!m->GetDeclaringClass()->IsAssignableFrom(c)) {
      AbortF("can't call static %s on class %s", m->PrettyMethod().c_str(),
             mirror::Class::PrettyClass(c).c_str());
      return false;
    }
    return true;
  }

  /*
   * Verify that "mid" is appropriate for "jobj".
   *
   * Make sure the object is an instance of the method's declaring class.
   * (Note the mid might point to a declaration in an interface; this
   * will be handled automatically by the instanceof check.)
   */
  bool CheckVirtualMethod(ScopedObjectAccess& soa, jobject java_object, jmethodID mid)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = CheckMethodID(mid);
    if (m == nullptr) {
      return false;
    }
    ObjPtr<mirror::Object> o = soa.Decode<mirror::Object>(java_object);
    if (o == nullptr) {
      AbortF("can't call %s on null object", m->PrettyMethod().c_str());
      return false;
    } else if (!o->InstanceOf(m->GetDeclaringClass())) {
      AbortF("can't call %s on instance of %s", m->PrettyMethod().c_str(),
             o->PrettyTypeOf().c_str());
      return false;
    }
    return true;
  }

  /**
   * The format string is a sequence of the following characters,
   * and must be followed by arguments of the corresponding types
   * in the same order.
   *
   * Java primitive types:
   * B - jbyte
   * C - jchar
   * D - jdouble
   * F - jfloat
   * I - jint
   * J - jlong
   * S - jshort
   * Z - jboolean (shown as true and false)
   * V - void
   *
   * Java reference types:
   * L - jobject
   * a - jarray
   * c - jclass
   * s - jstring
   *
   * JNI types:
   * b - jboolean (shown as JNI_TRUE and JNI_FALSE)
   * f - jfieldID
   * m - jmethodID
   * p - void*
   * r - jint (for release mode arguments)
   * u - const char* (Modified UTF-8)
   * z - jsize (for lengths; use i if negative values are okay)
   * v - JavaVM*
   * E - JNIEnv*
   * . - VarArgs* for Jni calls with variable length arguments
   *
   * Use the kFlag_NullableUtf flag where 'u' field(s) are nullable.
   */
  bool Check(ScopedObjectAccess& soa, bool entry, const char* fmt, JniValueType* args)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* traceMethod = nullptr;
    if (has_method_ && soa.Vm()->IsTracingEnabled()) {
      // We need to guard some of the invocation interface's calls: a bad caller might
      // use DetachCurrentThread or GetEnv on a thread that's not yet attached.
      Thread* self = Thread::Current();
      if ((flags_ & kFlag_Invocation) == 0 || self != nullptr) {
        traceMethod = self->GetCurrentMethod(nullptr);
      }
    }

    if (((flags_ & kFlag_ForceTrace) != 0) ||
        (traceMethod != nullptr && soa.Vm()->ShouldTrace(traceMethod))) {
      std::string msg;
      for (size_t i = 0; fmt[i] != '\0'; ++i) {
        TracePossibleHeapValue(soa, entry, fmt[i], args[i], &msg);
        if (fmt[i + 1] != '\0') {
          StringAppendF(&msg, ", ");
        }
      }

      if ((flags_ & kFlag_ForceTrace) != 0) {
        LOG(INFO) << "JNI: call to " << function_name_ << "(" << msg << ")";
      } else if (entry) {
        if (has_method_) {
          std::string methodName(ArtMethod::PrettyMethod(traceMethod, false));
          LOG(INFO) << "JNI: " << methodName << " -> " << function_name_ << "(" << msg << ")";
          indent_ = methodName.size() + 1;
        } else {
          LOG(INFO) << "JNI: -> " << function_name_ << "(" << msg << ")";
          indent_ = 0;
        }
      } else {
        LOG(INFO) << StringPrintf("JNI: %*s<- %s returned %s", indent_, "", function_name_, msg.c_str());
      }
    }

    // We always do the thorough checks on entry, and never on exit...
    if (entry) {
      for (size_t i = 0; fmt[i] != '\0'; ++i) {
        if (!CheckPossibleHeapValue(soa, fmt[i], args[i])) {
          return false;
        }
      }
    }
    return true;
  }

  bool CheckNonHeap(JavaVMExt* vm, bool entry, const char* fmt, JniValueType* args) {
    bool should_trace = (flags_ & kFlag_ForceTrace) != 0;
    if (!should_trace && vm != nullptr && vm->IsTracingEnabled()) {
      // We need to guard some of the invocation interface's calls: a bad caller might
      // use DetachCurrentThread or GetEnv on a thread that's not yet attached.
      Thread* self = Thread::Current();
      if ((flags_ & kFlag_Invocation) == 0 || self != nullptr) {
        ScopedObjectAccess soa(self);
        ArtMethod* traceMethod = self->GetCurrentMethod(nullptr);
        should_trace = (traceMethod != nullptr && vm->ShouldTrace(traceMethod));
      }
    }
    if (should_trace) {
      std::string msg;
      for (size_t i = 0; fmt[i] != '\0'; ++i) {
        TraceNonHeapValue(fmt[i], args[i], &msg);
        if (fmt[i + 1] != '\0') {
          StringAppendF(&msg, ", ");
        }
      }

      if ((flags_ & kFlag_ForceTrace) != 0) {
        LOG(INFO) << "JNI: call to " << function_name_ << "(" << msg << ")";
      } else if (entry) {
        if (has_method_) {
          Thread* self = Thread::Current();
          ScopedObjectAccess soa(self);
          ArtMethod* traceMethod = self->GetCurrentMethod(nullptr);
          std::string methodName(ArtMethod::PrettyMethod(traceMethod, false));
          LOG(INFO) << "JNI: " << methodName << " -> " << function_name_ << "(" << msg << ")";
          indent_ = methodName.size() + 1;
        } else {
          LOG(INFO) << "JNI: -> " << function_name_ << "(" << msg << ")";
          indent_ = 0;
        }
      } else {
        LOG(INFO) << StringPrintf("JNI: %*s<- %s returned %s", indent_, "", function_name_, msg.c_str());
      }
    }

    // We always do the thorough checks on entry, and never on exit...
    if (entry) {
      for (size_t i = 0; fmt[i] != '\0'; ++i) {
        if (!CheckNonHeapValue(fmt[i], args[i])) {
          return false;
        }
      }
    }
    return true;
  }

  bool CheckReflectedMethod(ScopedObjectAccess& soa, jobject jmethod)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ObjPtr<mirror::Object> method = soa.Decode<mirror::Object>(jmethod);
    if (method == nullptr) {
      AbortF("expected non-null method");
      return false;
    }
    mirror::Class* c = method->GetClass();
    if (soa.Decode<mirror::Class>(WellKnownClasses::java_lang_reflect_Method) != c &&
        soa.Decode<mirror::Class>(WellKnownClasses::java_lang_reflect_Constructor) != c) {
      AbortF("expected java.lang.reflect.Method or "
          "java.lang.reflect.Constructor but got object of type %s: %p",
          method->PrettyTypeOf().c_str(), jmethod);
      return false;
    }
    return true;
  }

  bool CheckConstructor(jmethodID mid) REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* method = jni::DecodeArtMethod(mid);
    if (method == nullptr) {
      AbortF("expected non-null constructor");
      return false;
    }
    if (!method->IsConstructor() || method->IsStatic()) {
      AbortF("expected a constructor but %s: %p", method->PrettyMethod().c_str(), mid);
      return false;
    }
    return true;
  }

  bool CheckReflectedField(ScopedObjectAccess& soa, jobject jfield)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ObjPtr<mirror::Object> field = soa.Decode<mirror::Object>(jfield);
    if (field == nullptr) {
      AbortF("expected non-null java.lang.reflect.Field");
      return false;
    }
    mirror::Class* c = field->GetClass();
    if (soa.Decode<mirror::Class>(WellKnownClasses::java_lang_reflect_Field) != c) {
      AbortF("expected java.lang.reflect.Field but got object of type %s: %p",
             field->PrettyTypeOf().c_str(), jfield);
      return false;
    }
    return true;
  }

  bool CheckThrowable(ScopedObjectAccess& soa, jthrowable jobj)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ObjPtr<mirror::Object> obj = soa.Decode<mirror::Object>(jobj);
    if (!obj->GetClass()->IsThrowableClass()) {
      AbortF("expected java.lang.Throwable but got object of type "
             "%s: %p", obj->PrettyTypeOf().c_str(), obj.Ptr());
      return false;
    }
    return true;
  }

  bool CheckThrowableClass(ScopedObjectAccess& soa, jclass jc)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ObjPtr<mirror::Class> c = soa.Decode<mirror::Class>(jc);
    if (!c->IsThrowableClass()) {
      AbortF("expected java.lang.Throwable class but got object of "
             "type %s: %p", c->PrettyDescriptor().c_str(), c.Ptr());
      return false;
    }
    return true;
  }

  bool CheckReferenceKind(IndirectRefKind expected_kind, Thread* self, jobject obj) {
    IndirectRefKind found_kind;
    if (expected_kind == kLocal) {
      found_kind = IndirectReferenceTable::GetIndirectRefKind(obj);
      if (found_kind == kHandleScopeOrInvalid && self->HandleScopeContains(obj)) {
        found_kind = kLocal;
      }
    } else {
      found_kind = IndirectReferenceTable::GetIndirectRefKind(obj);
    }
    if (obj != nullptr && found_kind != expected_kind) {
      AbortF("expected reference of kind %s but found %s: %p",
             GetIndirectRefKindString(expected_kind),
             GetIndirectRefKindString(IndirectReferenceTable::GetIndirectRefKind(obj)),
             obj);
      return false;
    }
    return true;
  }

  bool CheckInstantiableNonArray(ScopedObjectAccess& soa, jclass jc)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ObjPtr<mirror::Class> c = soa.Decode<mirror::Class>(jc);
    if (!c->IsInstantiableNonArray()) {
      AbortF("can't make objects of type %s: %p", c->PrettyDescriptor().c_str(), c.Ptr());
      return false;
    }
    return true;
  }

  bool CheckPrimitiveArrayType(ScopedObjectAccess& soa, jarray array, Primitive::Type type)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!CheckArray(soa, array)) {
      return false;
    }
    ObjPtr<mirror::Array> a = soa.Decode<mirror::Array>(array);
    if (a->GetClass()->GetComponentType()->GetPrimitiveType() != type) {
      AbortF("incompatible array type %s expected %s[]: %p",
             a->GetClass()->PrettyDescriptor().c_str(), PrettyDescriptor(type).c_str(), array);
      return false;
    }
    return true;
  }

  bool CheckFieldAccess(ScopedObjectAccess& soa, jobject obj, jfieldID fid, bool is_static,
                        Primitive::Type type)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (is_static && !CheckStaticFieldID(soa, down_cast<jclass>(obj), fid)) {
      return false;
    }
    if (!is_static && !CheckInstanceFieldID(soa, obj, fid)) {
      return false;
    }
    ArtField* field = jni::DecodeArtField(fid);
    DCHECK(field != nullptr);  // Already checked by Check.
    if (is_static != field->IsStatic()) {
      AbortF("attempt to access %s field %s: %p",
             field->IsStatic() ? "static" : "non-static", field->PrettyField().c_str(), fid);
      return false;
    }
    if (type != field->GetTypeAsPrimitiveType()) {
      AbortF("attempt to access field %s of type %s with the wrong type %s: %p",
             field->PrettyField().c_str(),
             PrettyDescriptor(field->GetTypeDescriptor()).c_str(),
             PrettyDescriptor(type).c_str(), fid);
      return false;
    }
    if (is_static) {
      ObjPtr<mirror::Object> o = soa.Decode<mirror::Object>(obj);
      if (o == nullptr || !o->IsClass()) {
        AbortF("attempt to access static field %s with a class argument of type %s: %p",
               field->PrettyField().c_str(), o->PrettyTypeOf().c_str(), fid);
        return false;
      }
      ObjPtr<mirror::Class> c = o->AsClass();
      if (!field->GetDeclaringClass()->IsAssignableFrom(c)) {
        AbortF("attempt to access static field %s with an incompatible class argument of %s: %p",
               field->PrettyField().c_str(), mirror::Class::PrettyDescriptor(c).c_str(), fid);
        return false;
      }
    } else {
      ObjPtr<mirror::Object> o = soa.Decode<mirror::Object>(obj);
      if (o == nullptr || !field->GetDeclaringClass()->IsAssignableFrom(o->GetClass())) {
        AbortF("attempt to access field %s from an object argument of type %s: %p",
               field->PrettyField().c_str(), o->PrettyTypeOf().c_str(), fid);
        return false;
      }
    }
    return true;
  }

 private:
  enum InstanceKind {
    kClass,
    kDirectByteBuffer,
    kObject,
    kString,
    kThrowable,
  };

  /*
   * Verify that "jobj" is a valid non-null object reference, and points to
   * an instance of expectedClass.
   *
   * Because we're looking at an object on the GC heap, we have to switch
   * to "running" mode before doing the checks.
   */
  bool CheckInstance(ScopedObjectAccess& soa, InstanceKind kind, jobject java_object, bool null_ok)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const char* what = nullptr;
    switch (kind) {
    case kClass:
      what = "jclass";
      break;
    case kDirectByteBuffer:
      what = "direct ByteBuffer";
      break;
    case kObject:
      what = "jobject";
      break;
    case kString:
      what = "jstring";
      break;
    case kThrowable:
      what = "jthrowable";
      break;
    default:
      LOG(FATAL) << "Unknown kind " << static_cast<int>(kind);
    }

    if (java_object == nullptr) {
      if (null_ok) {
        return true;
      } else {
        AbortF("%s received NULL %s", function_name_, what);
        return false;
      }
    }

    ObjPtr<mirror::Object> obj = soa.Decode<mirror::Object>(java_object);
    if (obj == nullptr) {
      // Either java_object is invalid or is a cleared weak.
      IndirectRef ref = reinterpret_cast<IndirectRef>(java_object);
      bool okay;
      if (IndirectReferenceTable::GetIndirectRefKind(ref) != kWeakGlobal) {
        okay = false;
      } else {
        obj = soa.Vm()->DecodeWeakGlobal(soa.Self(), ref);
        okay = Runtime::Current()->IsClearedJniWeakGlobal(obj);
      }
      if (!okay) {
        AbortF("%s is an invalid %s: %p (%p)",
               what,
               GetIndirectRefKindString(IndirectReferenceTable::GetIndirectRefKind(java_object)),
               java_object,
               obj.Ptr());
        return false;
      }
    }

    if (!Runtime::Current()->GetHeap()->IsValidObjectAddress(obj.Ptr())) {
      Runtime::Current()->GetHeap()->DumpSpaces(LOG_STREAM(ERROR));
      AbortF("%s is an invalid %s: %p (%p)",
             what,
             GetIndirectRefKindString(IndirectReferenceTable::GetIndirectRefKind(java_object)),
             java_object,
             obj.Ptr());
      return false;
    }

    bool okay = true;
    switch (kind) {
    case kClass:
      okay = obj->IsClass();
      break;
    case kDirectByteBuffer:
      UNIMPLEMENTED(FATAL);
      break;
    case kString:
      okay = obj->GetClass()->IsStringClass();
      break;
    case kThrowable:
      okay = obj->GetClass()->IsThrowableClass();
      break;
    case kObject:
      break;
    }
    if (!okay) {
      AbortF("%s has wrong type: %s", what, mirror::Object::PrettyTypeOf(obj).c_str());
      return false;
    }

    return true;
  }

  /*
   * Verify that the "mode" argument passed to a primitive array Release
   * function is one of the valid values.
   */
  bool CheckReleaseMode(jint mode) {
    if (mode != 0 && mode != JNI_COMMIT && mode != JNI_ABORT) {
      AbortF("unknown value for release mode: %d", mode);
      return false;
    }
    return true;
  }

  bool CheckPossibleHeapValue(ScopedObjectAccess& soa, char fmt, JniValueType arg)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    switch (fmt) {
      case 'a':  // jarray
        return CheckArray(soa, arg.a);
      case 'c':  // jclass
        return CheckInstance(soa, kClass, arg.c, false);
      case 'f':  // jfieldID
        return CheckFieldID(arg.f) != nullptr;
      case 'm':  // jmethodID
        return CheckMethodID(arg.m) != nullptr;
      case 'r':  // release int
        return CheckReleaseMode(arg.r);
      case 's':  // jstring
        return CheckInstance(soa, kString, arg.s, false);
      case 't':  // jthrowable
        return CheckInstance(soa, kThrowable, arg.t, false);
      case 'E':  // JNIEnv*
        return CheckThread(arg.E);
      case 'L':  // jobject
        return CheckInstance(soa, kObject, arg.L, true);
      case '.':  // A VarArgs list
        return CheckVarArgs(soa, arg.va);
      default:
        return CheckNonHeapValue(fmt, arg);
    }
  }

  bool CheckVarArgs(ScopedObjectAccess& soa, const VarArgs* args_p)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CHECK(args_p != nullptr);
    VarArgs args(args_p->Clone());
    ArtMethod* m = CheckMethodID(args.GetMethodID());
    if (m == nullptr) {
      return false;
    }
    uint32_t len = 0;
    const char* shorty = m->GetShorty(&len);
    // Skip the return type
    CHECK_GE(len, 1u);
    len--;
    shorty++;
    for (uint32_t i = 0; i < len; i++) {
      if (!CheckPossibleHeapValue(soa, shorty[i], args.GetValue(shorty[i]))) {
        return false;
      }
    }
    return true;
  }

  bool CheckNonHeapValue(char fmt, JniValueType arg) {
    switch (fmt) {
      case 'p':  // TODO: pointer - null or readable?
      case 'v':  // JavaVM*
      case 'D':  // jdouble
      case 'F':  // jfloat
      case 'J':  // jlong
      case 'I':  // jint
        break;  // Ignored.
      case 'b':  // jboolean, why two? Fall-through.
      case 'Z':
        return CheckBoolean(arg.I);
      case 'B':  // jbyte
        return CheckByte(arg.I);
      case 'C':  // jchar
        return CheckChar(arg.I);
      case 'S':  // jshort
        return CheckShort(arg.I);
      case 'u':  // utf8
        if ((flags_ & kFlag_Release) != 0) {
          return CheckNonNull(arg.u);
        } else {
          bool nullable = ((flags_ & kFlag_NullableUtf) != 0);
          return CheckUtfString(arg.u, nullable);
        }
      case 'w':  // jobjectRefType
        switch (arg.w) {
          case JNIInvalidRefType:
          case JNILocalRefType:
          case JNIGlobalRefType:
          case JNIWeakGlobalRefType:
            break;
          default:
            AbortF("Unknown reference type");
            return false;
        }
        break;
      case 'z':  // jsize
        return CheckLengthPositive(arg.z);
      default:
        AbortF("unknown format specifier: '%c'", fmt);
        return false;
    }
    return true;
  }

  void TracePossibleHeapValue(ScopedObjectAccess& soa, bool entry, char fmt, JniValueType arg,
                              std::string* msg)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    switch (fmt) {
      case 'L':  // jobject fall-through.
      case 'a':  // jarray fall-through.
      case 's':  // jstring fall-through.
      case 't':  // jthrowable fall-through.
        if (arg.L == nullptr) {
          *msg += "NULL";
        } else {
          StringAppendF(msg, "%p", arg.L);
        }
        break;
      case 'c': {  // jclass
        jclass jc = arg.c;
        ObjPtr<mirror::Class> c = soa.Decode<mirror::Class>(jc);
        if (c == nullptr) {
          *msg += "NULL";
        } else if (!Runtime::Current()->GetHeap()->IsValidObjectAddress(c.Ptr())) {
          StringAppendF(msg, "INVALID POINTER:%p", jc);
        } else if (!c->IsClass()) {
          *msg += "INVALID NON-CLASS OBJECT OF TYPE:" + c->PrettyTypeOf();
        } else {
          *msg += c->PrettyClass();
          if (!entry) {
            StringAppendF(msg, " (%p)", jc);
          }
        }
        break;
      }
      case 'f': {  // jfieldID
        jfieldID fid = arg.f;
        ArtField* f = jni::DecodeArtField(fid);
        *msg += ArtField::PrettyField(f);
        if (!entry) {
          StringAppendF(msg, " (%p)", fid);
        }
        break;
      }
      case 'm': {  // jmethodID
        jmethodID mid = arg.m;
        ArtMethod* m = jni::DecodeArtMethod(mid);
        *msg += ArtMethod::PrettyMethod(m);
        if (!entry) {
          StringAppendF(msg, " (%p)", mid);
        }
        break;
      }
      case '.': {
        const VarArgs* va = arg.va;
        VarArgs args(va->Clone());
        ArtMethod* m = jni::DecodeArtMethod(args.GetMethodID());
        uint32_t len;
        const char* shorty = m->GetShorty(&len);
        CHECK_GE(len, 1u);
        // Skip past return value.
        len--;
        shorty++;
        // Remove the previous ', ' from the message.
        msg->erase(msg->length() - 2);
        for (uint32_t i = 0; i < len; i++) {
          *msg += ", ";
          TracePossibleHeapValue(soa, entry, shorty[i], args.GetValue(shorty[i]), msg);
        }
        break;
      }
      default:
        TraceNonHeapValue(fmt, arg, msg);
        break;
    }
  }

  void TraceNonHeapValue(char fmt, JniValueType arg, std::string* msg) {
    switch (fmt) {
      case 'B':  // jbyte
        if (arg.B >= 0 && arg.B < 10) {
          StringAppendF(msg, "%d", arg.B);
        } else {
          StringAppendF(msg, "%#x (%d)", arg.B, arg.B);
        }
        break;
      case 'C':  // jchar
        if (arg.C < 0x7f && arg.C >= ' ') {
          StringAppendF(msg, "U+%x ('%c')", arg.C, arg.C);
        } else {
          StringAppendF(msg, "U+%x", arg.C);
        }
        break;
      case 'F':  // jfloat
        StringAppendF(msg, "%g", arg.F);
        break;
      case 'D':  // jdouble
        StringAppendF(msg, "%g", arg.D);
        break;
      case 'S':  // jshort
        StringAppendF(msg, "%d", arg.S);
        break;
      case 'i':  // jint - fall-through.
      case 'I':  // jint
        StringAppendF(msg, "%d", arg.I);
        break;
      case 'J':  // jlong
        StringAppendF(msg, "%" PRId64, arg.J);
        break;
      case 'Z':  // jboolean
      case 'b':  // jboolean (JNI-style)
        *msg += arg.b == JNI_TRUE ? "true" : "false";
        break;
      case 'V':  // void
        DCHECK(arg.V == nullptr);
        *msg += "void";
        break;
      case 'v':  // JavaVM*
        StringAppendF(msg, "(JavaVM*)%p", arg.v);
        break;
      case 'E':
        StringAppendF(msg, "(JNIEnv*)%p", arg.E);
        break;
      case 'z':  // non-negative jsize
        // You might expect jsize to be size_t, but it's not; it's the same as jint.
        // We only treat this specially so we can do the non-negative check.
        // TODO: maybe this wasn't worth it?
        StringAppendF(msg, "%d", arg.z);
        break;
      case 'p':  // void* ("pointer")
        if (arg.p == nullptr) {
          *msg += "NULL";
        } else {
          StringAppendF(msg, "(void*) %p", arg.p);
        }
        break;
      case 'r': {  // jint (release mode)
        jint releaseMode = arg.r;
        if (releaseMode == 0) {
          *msg += "0";
        } else if (releaseMode == JNI_ABORT) {
          *msg += "JNI_ABORT";
        } else if (releaseMode == JNI_COMMIT) {
          *msg += "JNI_COMMIT";
        } else {
          StringAppendF(msg, "invalid release mode %d", releaseMode);
        }
        break;
      }
      case 'u':  // const char* (Modified UTF-8)
        if (arg.u == nullptr) {
          *msg += "NULL";
        } else {
          StringAppendF(msg, "\"%s\"", arg.u);
        }
        break;
      case 'w':  // jobjectRefType
        switch (arg.w) {
          case JNIInvalidRefType:
            *msg += "invalid reference type";
            break;
          case JNILocalRefType:
            *msg += "local ref type";
            break;
          case JNIGlobalRefType:
            *msg += "global ref type";
            break;
          case JNIWeakGlobalRefType:
            *msg += "weak global ref type";
            break;
          default:
            *msg += "unknown ref type";
            break;
        }
        break;
      default:
        LOG(FATAL) << function_name_ << ": unknown trace format specifier: '" << fmt << "'";
    }
  }
  /*
   * Verify that "array" is non-null and points to an Array object.
   *
   * Since we're dealing with objects, switch to "running" mode.
   */
  bool CheckArray(ScopedObjectAccess& soa, jarray java_array)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (UNLIKELY(java_array == nullptr)) {
      AbortF("jarray was NULL");
      return false;
    }

    ObjPtr<mirror::Array> a = soa.Decode<mirror::Array>(java_array);
    if (UNLIKELY(!Runtime::Current()->GetHeap()->IsValidObjectAddress(a.Ptr()))) {
      Runtime::Current()->GetHeap()->DumpSpaces(LOG_STREAM(ERROR));
      AbortF("jarray is an invalid %s: %p (%p)",
             GetIndirectRefKindString(IndirectReferenceTable::GetIndirectRefKind(java_array)),
             java_array,
             a.Ptr());
      return false;
    } else if (!a->IsArrayInstance()) {
      AbortF("jarray argument has non-array type: %s", a->PrettyTypeOf().c_str());
      return false;
    }
    return true;
  }

  bool CheckBoolean(jint z) {
    if (z != JNI_TRUE && z != JNI_FALSE) {
      // Note, broken booleans are always fatal.
      AbortF("unexpected jboolean value: %d", z);
      return false;
    }
    return true;
  }

  bool CheckByte(jint b) {
    if (b < std::numeric_limits<jbyte>::min() ||
        b > std::numeric_limits<jbyte>::max()) {
      if (kBrokenPrimitivesAreFatal) {
        AbortF("unexpected jbyte value: %d", b);
        return false;
      } else {
        LOG(WARNING) << "Unexpected jbyte value: " << b;
      }
    }
    return true;
  }

  bool CheckShort(jint s) {
    if (s < std::numeric_limits<jshort>::min() ||
        s > std::numeric_limits<jshort>::max()) {
      if (kBrokenPrimitivesAreFatal) {
        AbortF("unexpected jshort value: %d", s);
        return false;
      } else {
        LOG(WARNING) << "Unexpected jshort value: " << s;
      }
    }
    return true;
  }

  bool CheckChar(jint c) {
    if (c < std::numeric_limits<jchar>::min() ||
        c > std::numeric_limits<jchar>::max()) {
      if (kBrokenPrimitivesAreFatal) {
        AbortF("unexpected jchar value: %d", c);
        return false;
      } else {
        LOG(WARNING) << "Unexpected jchar value: " << c;
      }
    }
    return true;
  }

  bool CheckLengthPositive(jsize length) {
    if (length < 0) {
      AbortF("negative jsize: %d", length);
      return false;
    }
    return true;
  }

  ArtField* CheckFieldID(jfieldID fid) REQUIRES_SHARED(Locks::mutator_lock_) {
    if (fid == nullptr) {
      AbortF("jfieldID was NULL");
      return nullptr;
    }
    ArtField* f = jni::DecodeArtField(fid);
    // TODO: Better check here.
    if (!Runtime::Current()->GetHeap()->IsValidObjectAddress(f->GetDeclaringClass().Ptr())) {
      Runtime::Current()->GetHeap()->DumpSpaces(LOG_STREAM(ERROR));
      AbortF("invalid jfieldID: %p", fid);
      return nullptr;
    }
    return f;
  }

  ArtMethod* CheckMethodID(jmethodID mid) REQUIRES_SHARED(Locks::mutator_lock_) {
    if (mid == nullptr) {
      AbortF("jmethodID was NULL");
      return nullptr;
    }
    ArtMethod* m = jni::DecodeArtMethod(mid);
    // TODO: Better check here.
    if (!Runtime::Current()->GetHeap()->IsValidObjectAddress(m->GetDeclaringClass())) {
      Runtime::Current()->GetHeap()->DumpSpaces(LOG_STREAM(ERROR));
      AbortF("invalid jmethodID: %p", mid);
      return nullptr;
    }
    return m;
  }

  bool CheckThread(JNIEnv* env) REQUIRES_SHARED(Locks::mutator_lock_) {
    Thread* self = Thread::Current();
    CHECK(self != nullptr);

    // Get the current thread's JNIEnv by going through our TLS pointer.
    JNIEnvExt* threadEnv = self->GetJniEnv();

    // Verify that the current thread is (a) attached and (b) associated with
    // this particular instance of JNIEnv.
    if (env != threadEnv) {
      // Get the thread owning the JNIEnv that's being used.
      Thread* envThread = reinterpret_cast<JNIEnvExt*>(env)->GetSelf();
      AbortF("thread %s using JNIEnv* from thread %s",
             ToStr<Thread>(*self).c_str(), ToStr<Thread>(*envThread).c_str());
      return false;
    }

    // Verify that, if this thread previously made a critical "get" call, we
    // do the corresponding "release" call before we try anything else.
    switch (flags_ & kFlag_CritMask) {
    case kFlag_CritOkay:    // okay to call this method
      break;
    case kFlag_CritBad:     // not okay to call
      if (threadEnv->GetCritical() > 0) {
        AbortF("thread %s using JNI after critical get",
               ToStr<Thread>(*self).c_str());
        return false;
      }
      break;
    case kFlag_CritGet:     // this is a "get" call
      // Don't check here; we allow nested gets.
      if (threadEnv->GetCritical() == 0) {
        threadEnv->SetCriticalStartUs(self->GetCpuMicroTime());
      }
      threadEnv->SetCritical(threadEnv->GetCritical() + 1);
      break;
    case kFlag_CritRelease:  // this is a "release" call
      if (threadEnv->GetCritical() == 0) {
        AbortF("thread %s called too many critical releases",
               ToStr<Thread>(*self).c_str());
        return false;
      } else if (threadEnv->GetCritical() == 1) {
        // Leaving the critical region, possibly warn about long critical regions.
        uint64_t critical_duration_us = self->GetCpuMicroTime() - threadEnv->GetCriticalStartUs();
        if (critical_duration_us > kCriticalWarnTimeUs) {
          LOG(WARNING) << "JNI critical lock held for "
                       << PrettyDuration(UsToNs(critical_duration_us)) << " on " << *self;
        }
      }
      threadEnv->SetCritical(threadEnv->GetCritical() - 1);
      break;
    default:
      LOG(FATAL) << "Bad flags (internal error): " << flags_;
    }

    // Verify that, if an exception has been raised, the native code doesn't
    // make any JNI calls other than the Exception* methods.
    if ((flags_ & kFlag_ExcepOkay) == 0 && self->IsExceptionPending()) {
      mirror::Throwable* exception = self->GetException();
      AbortF("JNI %s called with pending exception %s",
             function_name_,
             exception->Dump().c_str());
      return false;
    }
    return true;
  }

  // Verifies that "bytes" points to valid Modified UTF-8 data.
  bool CheckUtfString(const char* bytes, bool nullable) {
    if (bytes == nullptr) {
      if (!nullable) {
        AbortF("non-nullable const char* was NULL");
        return false;
      }
      return true;
    }

    const char* errorKind = nullptr;
    const uint8_t* utf8 = CheckUtfBytes(bytes, &errorKind);
    if (errorKind != nullptr) {
      // This is an expensive loop that will resize often, but this isn't supposed to hit in
      // practice anyways.
      std::ostringstream oss;
      oss << std::hex;
      const uint8_t* tmp = reinterpret_cast<const uint8_t*>(bytes);
      while (*tmp != 0) {
        if (tmp == utf8) {
          oss << "<";
        }
        oss << "0x" << std::setfill('0') << std::setw(2) << static_cast<uint32_t>(*tmp);
        if (tmp == utf8) {
          oss << '>';
        }
        tmp++;
        if (*tmp != 0) {
          oss << ' ';
        }
      }

      AbortF("input is not valid Modified UTF-8: illegal %s byte %#x\n"
          "    string: '%s'\n    input: '%s'", errorKind, *utf8, bytes, oss.str().c_str());
      return false;
    }
    return true;
  }

  // Checks whether |bytes| is valid modified UTF-8. We also accept 4 byte UTF
  // sequences in place of encoded surrogate pairs.
  static const uint8_t* CheckUtfBytes(const char* bytes, const char** errorKind) {
    while (*bytes != '\0') {
      const uint8_t* utf8 = reinterpret_cast<const uint8_t*>(bytes++);
      // Switch on the high four bits.
      switch (*utf8 >> 4) {
      case 0x00:
      case 0x01:
      case 0x02:
      case 0x03:
      case 0x04:
      case 0x05:
      case 0x06:
      case 0x07:
        // Bit pattern 0xxx. No need for any extra bytes.
        break;
      case 0x08:
      case 0x09:
      case 0x0a:
      case 0x0b:
         // Bit patterns 10xx, which are illegal start bytes.
        *errorKind = "start";
        return utf8;
      case 0x0f:
        // Bit pattern 1111, which might be the start of a 4 byte sequence.
        if ((*utf8 & 0x08) == 0) {
          // Bit pattern 1111 0xxx, which is the start of a 4 byte sequence.
          // We consume one continuation byte here, and fall through to consume two more.
          utf8 = reinterpret_cast<const uint8_t*>(bytes++);
          if ((*utf8 & 0xc0) != 0x80) {
            *errorKind = "continuation";
            return utf8;
          }
        } else {
          *errorKind = "start";
          return utf8;
        }

        // Fall through to the cases below to consume two more continuation bytes.
        FALLTHROUGH_INTENDED;
      case 0x0e:
        // Bit pattern 1110, so there are two additional bytes.
        utf8 = reinterpret_cast<const uint8_t*>(bytes++);
        if ((*utf8 & 0xc0) != 0x80) {
          *errorKind = "continuation";
          return utf8;
        }

        // Fall through to consume one more continuation byte.
        FALLTHROUGH_INTENDED;
      case 0x0c:
      case 0x0d:
        // Bit pattern 110x, so there is one additional byte.
        utf8 = reinterpret_cast<const uint8_t*>(bytes++);
        if ((*utf8 & 0xc0) != 0x80) {
          *errorKind = "continuation";
          return utf8;
        }
        break;
      }
    }
    return 0;
  }

  void AbortF(const char* fmt, ...) __attribute__((__format__(__printf__, 2, 3))) {
    va_list args;
    va_start(args, fmt);
    Runtime::Current()->GetJavaVM()->JniAbortV(function_name_, fmt, args);
    va_end(args);
  }

  // The name of the JNI function being checked.
  const char* const function_name_;

  int indent_;

  const uint16_t flags_;

  const bool has_method_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCheck);
};

/*
 * ===========================================================================
 *      Guarded arrays
 * ===========================================================================
 */

/* this gets tucked in at the start of the buffer; struct size must be even */
class GuardedCopy {
 public:
  /*
   * Create an over-sized buffer to hold the contents of "buf".  Copy it in,
   * filling in the area around it with guard data.
   */
  static void* Create(void* original_buf, size_t len, bool mod_okay) {
    const size_t new_len = LengthIncludingRedZones(len);
    uint8_t* const new_buf = DebugAlloc(new_len);

    // If modification is not expected, grab a checksum.
    uLong adler = 0;
    if (!mod_okay) {
      adler = adler32(adler32(0L, Z_NULL, 0), reinterpret_cast<const Bytef*>(original_buf), len);
    }

    GuardedCopy* copy = new (new_buf) GuardedCopy(original_buf, len, adler);

    // Fill begin region with canary pattern.
    const size_t kStartCanaryLength = (GuardedCopy::kRedZoneSize / 2) - sizeof(GuardedCopy);
    for (size_t i = 0, j = 0; i < kStartCanaryLength; ++i) {
      const_cast<char*>(copy->StartRedZone())[i] = kCanary[j];
      if (kCanary[j] == '\0') {
        j = 0;
      } else {
        j++;
      }
    }

    // Copy the data in; note "len" could be zero.
    memcpy(const_cast<uint8_t*>(copy->BufferWithinRedZones()), original_buf, len);

    // Fill end region with canary pattern.
    for (size_t i = 0, j = 0; i < kEndCanaryLength; ++i) {
      const_cast<char*>(copy->EndRedZone())[i] = kCanary[j];
      if (kCanary[j] == '\0') {
        j = 0;
      } else {
        j++;
      }
    }

    return const_cast<uint8_t*>(copy->BufferWithinRedZones());
  }

  /*
   * Create a guarded copy of a primitive array.  Modifications to the copied
   * data are allowed.  Returns a pointer to the copied data.
   */
  static void* CreateGuardedPACopy(JNIEnv* env, const jarray java_array, jboolean* is_copy,
                                   void* original_ptr) {
    ScopedObjectAccess soa(env);

    ObjPtr<mirror::Array> a = soa.Decode<mirror::Array>(java_array);
    size_t component_size = a->GetClass()->GetComponentSize();
    size_t byte_count = a->GetLength() * component_size;
    void* result = Create(original_ptr, byte_count, true);
    if (is_copy != nullptr) {
      *is_copy = JNI_TRUE;
    }
    return result;
  }

  /*
   * Perform the array "release" operation, which may or may not copy data
   * back into the managed heap, and may or may not release the underlying storage.
   */
  static void* ReleaseGuardedPACopy(const char* function_name, JNIEnv* env,
                                    jarray java_array ATTRIBUTE_UNUSED, void* embedded_buf,
                                    int mode) {
    ScopedObjectAccess soa(env);
    if (!GuardedCopy::Check(function_name, embedded_buf, true)) {
      return nullptr;
    }
    GuardedCopy* const copy = FromEmbedded(embedded_buf);
    void* original_ptr = copy->original_ptr_;
    if (mode != JNI_ABORT) {
      memcpy(original_ptr, embedded_buf, copy->original_length_);
    }
    if (mode != JNI_COMMIT) {
      Destroy(embedded_buf);
    }
    return original_ptr;
  }


  /*
   * Free up the guard buffer, scrub it, and return the original pointer.
   */
  static void* Destroy(void* embedded_buf) {
    GuardedCopy* copy = FromEmbedded(embedded_buf);
    void* original_ptr = const_cast<void*>(copy->original_ptr_);
    size_t len = LengthIncludingRedZones(copy->original_length_);
    DebugFree(copy, len);
    return original_ptr;
  }

  /*
   * Verify the guard area and, if "modOkay" is false, that the data itself
   * has not been altered.
   *
   * The caller has already checked that "dataBuf" is non-null.
   */
  static bool Check(const char* function_name, const void* embedded_buf, bool mod_okay) {
    const GuardedCopy* copy = FromEmbedded(embedded_buf);
    return copy->CheckHeader(function_name, mod_okay) && copy->CheckRedZones(function_name);
  }

 private:
  GuardedCopy(void* original_buf, size_t len, uLong adler) :
    magic_(kGuardMagic), adler_(adler), original_ptr_(original_buf), original_length_(len) {
  }

  static uint8_t* DebugAlloc(size_t len) {
    void* result = mmap(nullptr, len, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    if (result == MAP_FAILED) {
      PLOG(FATAL) << "GuardedCopy::create mmap(" << len << ") failed";
    }
    return reinterpret_cast<uint8_t*>(result);
  }

  static void DebugFree(void* buf, size_t len) {
    if (munmap(buf, len) != 0) {
      PLOG(FATAL) << "munmap(" << buf << ", " << len << ") failed";
    }
  }

  static size_t LengthIncludingRedZones(size_t len) {
    return len + kRedZoneSize;
  }

  // Get the GuardedCopy from the interior pointer.
  static GuardedCopy* FromEmbedded(void* embedded_buf) {
    return reinterpret_cast<GuardedCopy*>(
        reinterpret_cast<uint8_t*>(embedded_buf) - (kRedZoneSize / 2));
  }

  static const GuardedCopy* FromEmbedded(const void* embedded_buf) {
    return reinterpret_cast<const GuardedCopy*>(
        reinterpret_cast<const uint8_t*>(embedded_buf) - (kRedZoneSize / 2));
  }

  static void AbortF(const char* jni_function_name, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Runtime::Current()->GetJavaVM()->JniAbortV(jni_function_name, fmt, args);
    va_end(args);
  }

  bool CheckHeader(const char* function_name, bool mod_okay) const {
    static const uint32_t kMagicCmp = kGuardMagic;

    // Before we do anything with "pExtra", check the magic number.  We
    // do the check with memcmp rather than "==" in case the pointer is
    // unaligned.  If it points to completely bogus memory we're going
    // to crash, but there's no easy way around that.
    if (UNLIKELY(memcmp(&magic_, &kMagicCmp, 4) != 0)) {
      uint8_t buf[4];
      memcpy(buf, &magic_, 4);
      AbortF(function_name,
             "guard magic does not match (found 0x%02x%02x%02x%02x) -- incorrect data pointer %p?",
             buf[3], buf[2], buf[1], buf[0], this);  // Assumes little-endian.
      return false;
    }

    // If modification is not expected, verify checksum. Strictly speaking this is wrong: if we
    // told the client that we made a copy, there's no reason they can't alter the buffer.
    if (!mod_okay) {
      uLong computed_adler =
          adler32(adler32(0L, Z_NULL, 0), BufferWithinRedZones(), original_length_);
      if (computed_adler != adler_) {
        AbortF(function_name, "buffer modified (0x%08lx vs 0x%08lx) at address %p",
               computed_adler, adler_, this);
        return false;
      }
    }
    return true;
  }

  bool CheckRedZones(const char* function_name) const {
    // Check the begin red zone.
    const size_t kStartCanaryLength = (GuardedCopy::kRedZoneSize / 2) - sizeof(GuardedCopy);
    for (size_t i = 0, j = 0; i < kStartCanaryLength; ++i) {
      if (UNLIKELY(StartRedZone()[i] != kCanary[j])) {
        AbortF(function_name, "guard pattern before buffer disturbed at %p +%zd", this, i);
        return false;
      }
      if (kCanary[j] == '\0') {
        j = 0;
      } else {
        j++;
      }
    }

    // Check end region.
    for (size_t i = 0, j = 0; i < kEndCanaryLength; ++i) {
      if (UNLIKELY(EndRedZone()[i] != kCanary[j])) {
        size_t offset_from_buffer_start =
            &(EndRedZone()[i]) - &(StartRedZone()[kStartCanaryLength]);
        AbortF(function_name, "guard pattern after buffer disturbed at %p +%zd", this,
               offset_from_buffer_start);
        return false;
      }
      if (kCanary[j] == '\0') {
        j = 0;
      } else {
        j++;
      }
    }
    return true;
  }

  // Location that canary value will be written before the guarded region.
  const char* StartRedZone() const {
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(this);
    return reinterpret_cast<const char*>(buf + sizeof(GuardedCopy));
  }

  // Return the interior embedded buffer.
  const uint8_t* BufferWithinRedZones() const {
    const uint8_t* embedded_buf = reinterpret_cast<const uint8_t*>(this) + (kRedZoneSize / 2);
    return embedded_buf;
  }

  // Location that canary value will be written after the guarded region.
  const char* EndRedZone() const {
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(this);
    size_t buf_len = LengthIncludingRedZones(original_length_);
    return reinterpret_cast<const char*>(buf + (buf_len - (kRedZoneSize / 2)));
  }

  static constexpr size_t kRedZoneSize = 512;
  static constexpr size_t kEndCanaryLength = kRedZoneSize / 2;

  // Value written before and after the guarded array.
  static const char* const kCanary;

  static constexpr uint32_t kGuardMagic = 0xffd5aa96;

  const uint32_t magic_;
  const uLong adler_;
  void* const original_ptr_;
  const size_t original_length_;
};
const char* const GuardedCopy::kCanary = "JNI BUFFER RED ZONE";

/*
 * ===========================================================================
 *      JNI functions
 * ===========================================================================
 */

class CheckJNI {
 public:
  static jint GetVersion(JNIEnv* env) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[1] = {{.E = env }};
    if (sc.Check(soa, true, "E", args)) {
      JniValueType result;
      result.I = baseEnv(env)->GetVersion(env);
      if (sc.Check(soa, false, "I", &result)) {
        return result.I;
      }
    }
    return JNI_ERR;
  }

  static jint GetJavaVM(JNIEnv *env, JavaVM **vm) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env }, {.p = vm}};
    if (sc.Check(soa, true, "Ep", args)) {
      JniValueType result;
      result.i = baseEnv(env)->GetJavaVM(env, vm);
      if (sc.Check(soa, false, "i", &result)) {
        return result.i;
      }
    }
    return JNI_ERR;
  }

  static jint RegisterNatives(JNIEnv* env, jclass c, const JNINativeMethod* methods, jint nMethods) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[4] = {{.E = env }, {.c = c}, {.p = methods}, {.I = nMethods}};
    if (sc.Check(soa, true, "EcpI", args)) {
      JniValueType result;
      result.i = baseEnv(env)->RegisterNatives(env, c, methods, nMethods);
      if (sc.Check(soa, false, "i", &result)) {
        return result.i;
      }
    }
    return JNI_ERR;
  }

  static jint UnregisterNatives(JNIEnv* env, jclass c) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env }, {.c = c}};
    if (sc.Check(soa, true, "Ec", args)) {
      JniValueType result;
      result.i = baseEnv(env)->UnregisterNatives(env, c);
      if (sc.Check(soa, false, "i", &result)) {
        return result.i;
      }
    }
    return JNI_ERR;
  }

  static jobjectRefType GetObjectRefType(JNIEnv* env, jobject obj) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNIInvalidRefType);
    // Note: we use "EL" here but "Ep" has been used in the past on the basis that we'd like to
    // know the object is invalid. The spec says that passing invalid objects or even ones that
    // are deleted isn't supported.
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env }, {.L = obj}};
    if (sc.Check(soa, true, "EL", args)) {
      JniValueType result;
      result.w = baseEnv(env)->GetObjectRefType(env, obj);
      if (sc.Check(soa, false, "w", &result)) {
        return result.w;
      }
    }
    return JNIInvalidRefType;
  }

  static jclass DefineClass(JNIEnv* env, const char* name, jobject loader, const jbyte* buf,
                            jsize bufLen) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[5] = {{.E = env}, {.u = name}, {.L = loader}, {.p = buf}, {.z = bufLen}};
    if (sc.Check(soa, true, "EuLpz", args) && sc.CheckClassName(name)) {
      JniValueType result;
      result.c = baseEnv(env)->DefineClass(env, name, loader, buf, bufLen);
      if (sc.Check(soa, false, "c", &result)) {
        return result.c;
      }
    }
    return nullptr;
  }

  static jclass FindClass(JNIEnv* env, const char* name) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.u = name}};
    if (sc.Check(soa, true, "Eu", args) && sc.CheckClassName(name)) {
      JniValueType result;
      result.c = baseEnv(env)->FindClass(env, name);
      if (sc.Check(soa, false, "c", &result)) {
        return result.c;
      }
    }
    return nullptr;
  }

  static jclass GetSuperclass(JNIEnv* env, jclass c) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.c = c}};
    if (sc.Check(soa, true, "Ec", args)) {
      JniValueType result;
      result.c = baseEnv(env)->GetSuperclass(env, c);
      if (sc.Check(soa, false, "c", &result)) {
        return result.c;
      }
    }
    return nullptr;
  }

  static jboolean IsAssignableFrom(JNIEnv* env, jclass c1, jclass c2) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_FALSE);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[3] = {{.E = env}, {.c = c1}, {.c = c2}};
    if (sc.Check(soa, true, "Ecc", args)) {
      JniValueType result;
      result.b = baseEnv(env)->IsAssignableFrom(env, c1, c2);
      if (sc.Check(soa, false, "b", &result)) {
        return result.b;
      }
    }
    return JNI_FALSE;
  }

  static jmethodID FromReflectedMethod(JNIEnv* env, jobject method) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.L = method}};
    if (sc.Check(soa, true, "EL", args) && sc.CheckReflectedMethod(soa, method)) {
      JniValueType result;
      result.m = baseEnv(env)->FromReflectedMethod(env, method);
      if (sc.Check(soa, false, "m", &result)) {
        return result.m;
      }
    }
    return nullptr;
  }

  static jfieldID FromReflectedField(JNIEnv* env, jobject field) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.L = field}};
    if (sc.Check(soa, true, "EL", args) && sc.CheckReflectedField(soa, field)) {
      JniValueType result;
      result.f = baseEnv(env)->FromReflectedField(env, field);
      if (sc.Check(soa, false, "f", &result)) {
        return result.f;
      }
    }
    return nullptr;
  }

  static jobject ToReflectedMethod(JNIEnv* env, jclass cls, jmethodID mid, jboolean isStatic) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[4] = {{.E = env}, {.c = cls}, {.m = mid}, {.I = isStatic}};
    if (sc.Check(soa, true, "Ecmb", args)) {
      JniValueType result;
      result.L = baseEnv(env)->ToReflectedMethod(env, cls, mid, isStatic);
      if (sc.Check(soa, false, "L", &result) && (result.L != nullptr)) {
        DCHECK(sc.CheckReflectedMethod(soa, result.L));
        return result.L;
      }
    }
    return nullptr;
  }

  static jobject ToReflectedField(JNIEnv* env, jclass cls, jfieldID fid, jboolean isStatic) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[4] = {{.E = env}, {.c = cls}, {.f = fid}, {.I = isStatic}};
    if (sc.Check(soa, true, "Ecfb", args)) {
      JniValueType result;
      result.L = baseEnv(env)->ToReflectedField(env, cls, fid, isStatic);
      if (sc.Check(soa, false, "L", &result) && (result.L != nullptr)) {
        DCHECK(sc.CheckReflectedField(soa, result.L));
        return result.L;
      }
    }
    return nullptr;
  }

  static jint Throw(JNIEnv* env, jthrowable obj) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.t = obj}};
    if (sc.Check(soa, true, "Et", args) && sc.CheckThrowable(soa, obj)) {
      JniValueType result;
      result.i = baseEnv(env)->Throw(env, obj);
      if (sc.Check(soa, false, "i", &result)) {
        return result.i;
      }
    }
    return JNI_ERR;
  }

  static jint ThrowNew(JNIEnv* env, jclass c, const char* message) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_NullableUtf, __FUNCTION__);
    JniValueType args[3] = {{.E = env}, {.c = c}, {.u = message}};
    if (sc.Check(soa, true, "Ecu", args) && sc.CheckThrowableClass(soa, c)) {
      JniValueType result;
      result.i = baseEnv(env)->ThrowNew(env, c, message);
      if (sc.Check(soa, false, "i", &result)) {
        return result.i;
      }
    }
    return JNI_ERR;
  }

  static jthrowable ExceptionOccurred(JNIEnv* env) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_ExcepOkay, __FUNCTION__);
    JniValueType args[1] = {{.E = env}};
    if (sc.Check(soa, true, "E", args)) {
      JniValueType result;
      result.t = baseEnv(env)->ExceptionOccurred(env);
      if (sc.Check(soa, false, "t", &result)) {
        return result.t;
      }
    }
    return nullptr;
  }

  static void ExceptionDescribe(JNIEnv* env) {
    CHECK_ATTACHED_THREAD_VOID(__FUNCTION__);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_ExcepOkay, __FUNCTION__);
    JniValueType args[1] = {{.E = env}};
    if (sc.Check(soa, true, "E", args)) {
      JniValueType result;
      baseEnv(env)->ExceptionDescribe(env);
      result.V = nullptr;
      sc.Check(soa, false, "V", &result);
    }
  }

  static void ExceptionClear(JNIEnv* env) {
    CHECK_ATTACHED_THREAD_VOID(__FUNCTION__);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_ExcepOkay, __FUNCTION__);
    JniValueType args[1] = {{.E = env}};
    if (sc.Check(soa, true, "E", args)) {
      JniValueType result;
      baseEnv(env)->ExceptionClear(env);
      result.V = nullptr;
      sc.Check(soa, false, "V", &result);
    }
  }

  static jboolean ExceptionCheck(JNIEnv* env) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_FALSE);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_CritOkay | kFlag_ExcepOkay, __FUNCTION__);
    JniValueType args[1] = {{.E = env}};
    if (sc.Check(soa, true, "E", args)) {
      JniValueType result;
      result.b = baseEnv(env)->ExceptionCheck(env);
      if (sc.Check(soa, false, "b", &result)) {
        return result.b;
      }
    }
    return JNI_FALSE;
  }

  static void FatalError(JNIEnv* env, const char* msg) {
    CHECK_ATTACHED_THREAD_VOID(__FUNCTION__);
    // The JNI specification doesn't say it's okay to call FatalError with a pending exception,
    // but you're about to abort anyway, and it's quite likely that you have a pending exception,
    // and it's not unimaginable that you don't know that you do. So we allow it.
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_ExcepOkay | kFlag_NullableUtf, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.u = msg}};
    if (sc.Check(soa, true, "Eu", args)) {
      JniValueType result;
      baseEnv(env)->FatalError(env, msg);
      // Unreachable.
      result.V = nullptr;
      sc.Check(soa, false, "V", &result);
    }
  }

  static jint PushLocalFrame(JNIEnv* env, jint capacity) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_ExcepOkay, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.I = capacity}};
    if (sc.Check(soa, true, "EI", args)) {
      JniValueType result;
      result.i = baseEnv(env)->PushLocalFrame(env, capacity);
      if (sc.Check(soa, false, "i", &result)) {
        return result.i;
      }
    }
    return JNI_ERR;
  }

  static jobject PopLocalFrame(JNIEnv* env, jobject res) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_ExcepOkay, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.L = res}};
    if (sc.Check(soa, true, "EL", args)) {
      JniValueType result;
      result.L = baseEnv(env)->PopLocalFrame(env, res);
      sc.Check(soa, false, "L", &result);
      return result.L;
    }
    return nullptr;
  }

  static jobject NewGlobalRef(JNIEnv* env, jobject obj) {
    return NewRef(__FUNCTION__, env, obj, kGlobal);
  }

  static jobject NewLocalRef(JNIEnv* env, jobject obj) {
    return NewRef(__FUNCTION__, env, obj, kLocal);
  }

  static jweak NewWeakGlobalRef(JNIEnv* env, jobject obj) {
    return NewRef(__FUNCTION__, env, obj, kWeakGlobal);
  }

  static void DeleteGlobalRef(JNIEnv* env, jobject obj) {
    DeleteRef(__FUNCTION__, env, obj, kGlobal);
  }

  static void DeleteWeakGlobalRef(JNIEnv* env, jweak obj) {
    DeleteRef(__FUNCTION__, env, obj, kWeakGlobal);
  }

  static void DeleteLocalRef(JNIEnv* env, jobject obj) {
    DeleteRef(__FUNCTION__, env, obj, kLocal);
  }

  static jint EnsureLocalCapacity(JNIEnv *env, jint capacity) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.I = capacity}};
    if (sc.Check(soa, true, "EI", args)) {
      JniValueType result;
      result.i = baseEnv(env)->EnsureLocalCapacity(env, capacity);
      if (sc.Check(soa, false, "i", &result)) {
        return result.i;
      }
    }
    return JNI_ERR;
  }

  static jboolean IsSameObject(JNIEnv* env, jobject ref1, jobject ref2) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_FALSE);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[3] = {{.E = env}, {.L = ref1}, {.L = ref2}};
    if (sc.Check(soa, true, "ELL", args)) {
      JniValueType result;
      result.b = baseEnv(env)->IsSameObject(env, ref1, ref2);
      if (sc.Check(soa, false, "b", &result)) {
        return result.b;
      }
    }
    return JNI_FALSE;
  }

  static jobject AllocObject(JNIEnv* env, jclass c) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.c = c}};
    if (sc.Check(soa, true, "Ec", args) && sc.CheckInstantiableNonArray(soa, c)) {
      JniValueType result;
      result.L = baseEnv(env)->AllocObject(env, c);
      if (sc.Check(soa, false, "L", &result)) {
        return result.L;
      }
    }
    return nullptr;
  }

  static jobject NewObjectV(JNIEnv* env, jclass c, jmethodID mid, va_list vargs) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    VarArgs rest(mid, vargs);
    JniValueType args[4] = {{.E = env}, {.c = c}, {.m = mid}, {.va = &rest}};
    if (sc.Check(soa, true, "Ecm.", args) && sc.CheckInstantiableNonArray(soa, c) &&
        sc.CheckConstructor(mid)) {
      JniValueType result;
      result.L = baseEnv(env)->NewObjectV(env, c, mid, vargs);
      if (sc.Check(soa, false, "L", &result)) {
        return result.L;
      }
    }
    return nullptr;
  }

  static jobject NewObject(JNIEnv* env, jclass c, jmethodID mid, ...) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    va_list args;
    va_start(args, mid);
    jobject result = NewObjectV(env, c, mid, args);
    va_end(args);
    return result;
  }

  static jobject NewObjectA(JNIEnv* env, jclass c, jmethodID mid, jvalue* vargs) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    VarArgs rest(mid, vargs);
    JniValueType args[4] = {{.E = env}, {.c = c}, {.m = mid}, {.va = &rest}};
    if (sc.Check(soa, true, "Ecm.", args) && sc.CheckInstantiableNonArray(soa, c) &&
        sc.CheckConstructor(mid)) {
      JniValueType result;
      result.L = baseEnv(env)->NewObjectA(env, c, mid, vargs);
      if (sc.Check(soa, false, "L", &result)) {
        return result.L;
      }
    }
    return nullptr;
  }

  static jclass GetObjectClass(JNIEnv* env, jobject obj) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.L = obj}};
    if (sc.Check(soa, true, "EL", args)) {
      JniValueType result;
      result.c = baseEnv(env)->GetObjectClass(env, obj);
      if (sc.Check(soa, false, "c", &result)) {
        return result.c;
      }
    }
    return nullptr;
  }

  static jboolean IsInstanceOf(JNIEnv* env, jobject obj, jclass c) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_FALSE);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[3] = {{.E = env}, {.L = obj}, {.c = c}};
    if (sc.Check(soa, true, "ELc", args)) {
      JniValueType result;
      result.b = baseEnv(env)->IsInstanceOf(env, obj, c);
      if (sc.Check(soa, false, "b", &result)) {
        return result.b;
      }
    }
    return JNI_FALSE;
  }

  static jmethodID GetMethodID(JNIEnv* env, jclass c, const char* name, const char* sig) {
    return GetMethodIDInternal(__FUNCTION__, env, c, name, sig, false);
  }

  static jmethodID GetStaticMethodID(JNIEnv* env, jclass c, const char* name, const char* sig) {
    return GetMethodIDInternal(__FUNCTION__, env, c, name, sig, true);
  }

  static jfieldID GetFieldID(JNIEnv* env, jclass c, const char* name, const char* sig) {
    return GetFieldIDInternal(__FUNCTION__, env, c, name, sig, false);
  }

  static jfieldID GetStaticFieldID(JNIEnv* env, jclass c, const char* name, const char* sig) {
    return GetFieldIDInternal(__FUNCTION__, env, c, name, sig, true);
  }

#define FIELD_ACCESSORS(jtype, name, ptype, shorty, slot_sized_shorty)  \
  static jtype GetStatic##name##Field(JNIEnv* env, jclass c, jfieldID fid) { \
    return GetField(__FUNCTION__, env, c, fid, true, ptype).shorty; \
  } \
  \
  static jtype Get##name##Field(JNIEnv* env, jobject obj, jfieldID fid) { \
    return GetField(__FUNCTION__, env, obj, fid, false, ptype).shorty; \
  } \
  \
  static void SetStatic##name##Field(JNIEnv* env, jclass c, jfieldID fid, jtype v) { \
    JniValueType value; \
    value.slot_sized_shorty = v; \
    SetField(__FUNCTION__, env, c, fid, true, ptype, value); \
  } \
  \
  static void Set##name##Field(JNIEnv* env, jobject obj, jfieldID fid, jtype v) { \
    JniValueType value; \
    value.slot_sized_shorty = v; \
    SetField(__FUNCTION__, env, obj, fid, false, ptype, value); \
  }

  FIELD_ACCESSORS(jobject, Object, Primitive::kPrimNot, L, L)
  FIELD_ACCESSORS(jboolean, Boolean, Primitive::kPrimBoolean, Z, I)
  FIELD_ACCESSORS(jbyte, Byte, Primitive::kPrimByte, B, I)
  FIELD_ACCESSORS(jchar, Char, Primitive::kPrimChar, C, I)
  FIELD_ACCESSORS(jshort, Short, Primitive::kPrimShort, S, I)
  FIELD_ACCESSORS(jint, Int, Primitive::kPrimInt, I, I)
  FIELD_ACCESSORS(jlong, Long, Primitive::kPrimLong, J, J)
  FIELD_ACCESSORS(jfloat, Float, Primitive::kPrimFloat, F, F)
  FIELD_ACCESSORS(jdouble, Double, Primitive::kPrimDouble, D, D)
#undef FIELD_ACCESSORS

  static void CallVoidMethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* vargs) {
    CallMethodA(__FUNCTION__, env, obj, nullptr, mid, vargs, Primitive::kPrimVoid, kVirtual);
  }

  static void CallNonvirtualVoidMethodA(JNIEnv* env, jobject obj, jclass c, jmethodID mid,
                                        jvalue* vargs) {
    CallMethodA(__FUNCTION__, env, obj, c, mid, vargs, Primitive::kPrimVoid, kDirect);
  }

  static void CallStaticVoidMethodA(JNIEnv* env, jclass c, jmethodID mid, jvalue* vargs) {
    CallMethodA(__FUNCTION__, env, nullptr, c, mid, vargs, Primitive::kPrimVoid, kStatic);
  }

  static void CallVoidMethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list vargs) {
    CallMethodV(__FUNCTION__, env, obj, nullptr, mid, vargs, Primitive::kPrimVoid, kVirtual);
  }

  static void CallNonvirtualVoidMethodV(JNIEnv* env, jobject obj, jclass c, jmethodID mid,
                                        va_list vargs) {
    CallMethodV(__FUNCTION__, env, obj, c, mid, vargs, Primitive::kPrimVoid, kDirect);
  }

  static void CallStaticVoidMethodV(JNIEnv* env, jclass c, jmethodID mid, va_list vargs) {
    CallMethodV(__FUNCTION__, env, nullptr, c, mid, vargs, Primitive::kPrimVoid, kStatic);
  }

  static void CallVoidMethod(JNIEnv* env, jobject obj, jmethodID mid, ...) {
    va_list vargs;
    va_start(vargs, mid);
    CallMethodV(__FUNCTION__, env, obj, nullptr, mid, vargs, Primitive::kPrimVoid, kVirtual);
    va_end(vargs);
  }

  static void CallNonvirtualVoidMethod(JNIEnv* env, jobject obj, jclass c, jmethodID mid, ...) {
    va_list vargs;
    va_start(vargs, mid);
    CallMethodV(__FUNCTION__, env, obj, c, mid, vargs, Primitive::kPrimVoid, kDirect);
    va_end(vargs);
  }

  static void CallStaticVoidMethod(JNIEnv* env, jclass c, jmethodID mid, ...) {
    va_list vargs;
    va_start(vargs, mid);
    CallMethodV(__FUNCTION__, env, nullptr, c, mid, vargs, Primitive::kPrimVoid, kStatic);
    va_end(vargs);
  }

#define CALL(rtype, name, ptype, shorty) \
  static rtype Call##name##MethodA(JNIEnv* env, jobject obj, jmethodID mid, jvalue* vargs) { \
    return CallMethodA(__FUNCTION__, env, obj, nullptr, mid, vargs, ptype, kVirtual).shorty; \
  } \
  \
  static rtype CallNonvirtual##name##MethodA(JNIEnv* env, jobject obj, jclass c, jmethodID mid, \
                                             jvalue* vargs) { \
    return CallMethodA(__FUNCTION__, env, obj, c, mid, vargs, ptype, kDirect).shorty; \
  } \
  \
  static rtype CallStatic##name##MethodA(JNIEnv* env, jclass c, jmethodID mid, jvalue* vargs) { \
    return CallMethodA(__FUNCTION__, env, nullptr, c, mid, vargs, ptype, kStatic).shorty; \
  } \
  \
  static rtype Call##name##MethodV(JNIEnv* env, jobject obj, jmethodID mid, va_list vargs) { \
    return CallMethodV(__FUNCTION__, env, obj, nullptr, mid, vargs, ptype, kVirtual).shorty; \
  } \
  \
  static rtype CallNonvirtual##name##MethodV(JNIEnv* env, jobject obj, jclass c, jmethodID mid, \
                                             va_list vargs) { \
    return CallMethodV(__FUNCTION__, env, obj, c, mid, vargs, ptype, kDirect).shorty; \
  } \
  \
  static rtype CallStatic##name##MethodV(JNIEnv* env, jclass c, jmethodID mid, va_list vargs) { \
    return CallMethodV(__FUNCTION__, env, nullptr, c, mid, vargs, ptype, kStatic).shorty; \
  } \
  \
  static rtype Call##name##Method(JNIEnv* env, jobject obj, jmethodID mid, ...) { \
    va_list vargs; \
    va_start(vargs, mid); \
    rtype result = \
        CallMethodV(__FUNCTION__, env, obj, nullptr, mid, vargs, ptype, kVirtual).shorty; \
    va_end(vargs); \
    return result; \
  } \
  \
  static rtype CallNonvirtual##name##Method(JNIEnv* env, jobject obj, jclass c, jmethodID mid, \
                                            ...) { \
    va_list vargs; \
    va_start(vargs, mid); \
    rtype result = \
        CallMethodV(__FUNCTION__, env, obj, c, mid, vargs, ptype, kDirect).shorty; \
    va_end(vargs); \
    return result; \
  } \
  \
  static rtype CallStatic##name##Method(JNIEnv* env, jclass c, jmethodID mid, ...) { \
    va_list vargs; \
    va_start(vargs, mid); \
    rtype result = \
        CallMethodV(__FUNCTION__, env, nullptr, c, mid, vargs, ptype, kStatic).shorty; \
    va_end(vargs); \
    return result; \
  }

  CALL(jobject, Object, Primitive::kPrimNot, L)
  CALL(jboolean, Boolean, Primitive::kPrimBoolean, Z)
  CALL(jbyte, Byte, Primitive::kPrimByte, B)
  CALL(jchar, Char, Primitive::kPrimChar, C)
  CALL(jshort, Short, Primitive::kPrimShort, S)
  CALL(jint, Int, Primitive::kPrimInt, I)
  CALL(jlong, Long, Primitive::kPrimLong, J)
  CALL(jfloat, Float, Primitive::kPrimFloat, F)
  CALL(jdouble, Double, Primitive::kPrimDouble, D)
#undef CALL

  static jstring NewString(JNIEnv* env, const jchar* unicode_chars, jsize len) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[3] = {{.E = env}, {.p = unicode_chars}, {.z = len}};
    if (sc.Check(soa, true, "Epz", args)) {
      JniValueType result;
      result.s = baseEnv(env)->NewString(env, unicode_chars, len);
      if (sc.Check(soa, false, "s", &result)) {
        return result.s;
      }
    }
    return nullptr;
  }

  static jstring NewStringUTF(JNIEnv* env, const char* chars) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_NullableUtf, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.u = chars}};
    if (sc.Check(soa, true, "Eu", args)) {
      JniValueType result;
      // TODO: stale? show pointer and truncate string.
      result.s = baseEnv(env)->NewStringUTF(env, chars);
      if (sc.Check(soa, false, "s", &result)) {
        return result.s;
      }
    }
    return nullptr;
  }

  static jsize GetStringLength(JNIEnv* env, jstring string) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_CritOkay, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.s = string}};
    if (sc.Check(soa, true, "Es", args)) {
      JniValueType result;
      result.z = baseEnv(env)->GetStringLength(env, string);
      if (sc.Check(soa, false, "z", &result)) {
        return result.z;
      }
    }
    return JNI_ERR;
  }

  static jsize GetStringUTFLength(JNIEnv* env, jstring string) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_CritOkay, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.s = string}};
    if (sc.Check(soa, true, "Es", args)) {
      JniValueType result;
      result.z = baseEnv(env)->GetStringUTFLength(env, string);
      if (sc.Check(soa, false, "z", &result)) {
        return result.z;
      }
    }
    return JNI_ERR;
  }

  static const jchar* GetStringChars(JNIEnv* env, jstring string, jboolean* is_copy) {
    return reinterpret_cast<const jchar*>(GetStringCharsInternal(__FUNCTION__, env, string,
                                                                 is_copy, false, false));
  }

  static const char* GetStringUTFChars(JNIEnv* env, jstring string, jboolean* is_copy) {
    return reinterpret_cast<const char*>(GetStringCharsInternal(__FUNCTION__, env, string,
                                                                is_copy, true, false));
  }

  static const jchar* GetStringCritical(JNIEnv* env, jstring string, jboolean* is_copy) {
    return reinterpret_cast<const jchar*>(GetStringCharsInternal(__FUNCTION__, env, string,
                                                                 is_copy, false, true));
  }

  static void ReleaseStringChars(JNIEnv* env, jstring string, const jchar* chars) {
    ReleaseStringCharsInternal(__FUNCTION__, env, string, chars, false, false);
  }

  static void ReleaseStringUTFChars(JNIEnv* env, jstring string, const char* utf) {
    ReleaseStringCharsInternal(__FUNCTION__, env, string, utf, true, false);
  }

  static void ReleaseStringCritical(JNIEnv* env, jstring string, const jchar* chars) {
    ReleaseStringCharsInternal(__FUNCTION__, env, string, chars, false, true);
  }

  static void GetStringRegion(JNIEnv* env, jstring string, jsize start, jsize len, jchar* buf) {
    CHECK_ATTACHED_THREAD_VOID(__FUNCTION__);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_CritOkay, __FUNCTION__);
    JniValueType args[5] = {{.E = env}, {.s = string}, {.z = start}, {.z = len}, {.p = buf}};
    // Note: the start and len arguments are checked as 'I' rather than 'z' as invalid indices
    // result in ArrayIndexOutOfBoundsExceptions in the base implementation.
    if (sc.Check(soa, true, "EsIIp", args)) {
      baseEnv(env)->GetStringRegion(env, string, start, len, buf);
      JniValueType result;
      result.V = nullptr;
      sc.Check(soa, false, "V", &result);
    }
  }

  static void GetStringUTFRegion(JNIEnv* env, jstring string, jsize start, jsize len, char* buf) {
    CHECK_ATTACHED_THREAD_VOID(__FUNCTION__);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_CritOkay, __FUNCTION__);
    JniValueType args[5] = {{.E = env}, {.s = string}, {.z = start}, {.z = len}, {.p = buf}};
    // Note: the start and len arguments are checked as 'I' rather than 'z' as invalid indices
    // result in ArrayIndexOutOfBoundsExceptions in the base implementation.
    if (sc.Check(soa, true, "EsIIp", args)) {
      baseEnv(env)->GetStringUTFRegion(env, string, start, len, buf);
      JniValueType result;
      result.V = nullptr;
      sc.Check(soa, false, "V", &result);
    }
  }

  static jsize GetArrayLength(JNIEnv* env, jarray array) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_CritOkay, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.a = array}};
    if (sc.Check(soa, true, "Ea", args)) {
      JniValueType result;
      result.z = baseEnv(env)->GetArrayLength(env, array);
      if (sc.Check(soa, false, "z", &result)) {
        return result.z;
      }
    }
    return JNI_ERR;
  }

  static jobjectArray NewObjectArray(JNIEnv* env, jsize length, jclass element_class,
                                     jobject initial_element) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[4] =
        {{.E = env}, {.z = length}, {.c = element_class}, {.L = initial_element}};
    if (sc.Check(soa, true, "EzcL", args)) {
      JniValueType result;
      // Note: assignability tests of initial_element are done in the base implementation.
      result.a = baseEnv(env)->NewObjectArray(env, length, element_class, initial_element);
      if (sc.Check(soa, false, "a", &result)) {
        return down_cast<jobjectArray>(result.a);
      }
    }
    return nullptr;
  }

  static jobject GetObjectArrayElement(JNIEnv* env, jobjectArray array, jsize index) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[3] = {{.E = env}, {.a = array}, {.z = index}};
    if (sc.Check(soa, true, "Eaz", args)) {
      JniValueType result;
      result.L = baseEnv(env)->GetObjectArrayElement(env, array, index);
      if (sc.Check(soa, false, "L", &result)) {
        return result.L;
      }
    }
    return nullptr;
  }

  static void SetObjectArrayElement(JNIEnv* env, jobjectArray array, jsize index, jobject value) {
    CHECK_ATTACHED_THREAD_VOID(__FUNCTION__);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[4] = {{.E = env}, {.a = array}, {.z = index}, {.L = value}};
    // Note: the index arguments is checked as 'I' rather than 'z' as invalid indices result in
    // ArrayIndexOutOfBoundsExceptions in the base implementation. Similarly invalid stores result
    // in ArrayStoreExceptions.
    if (sc.Check(soa, true, "EaIL", args)) {
      baseEnv(env)->SetObjectArrayElement(env, array, index, value);
      JniValueType result;
      result.V = nullptr;
      sc.Check(soa, false, "V", &result);
    }
  }

  static jbooleanArray NewBooleanArray(JNIEnv* env, jsize length) {
    return down_cast<jbooleanArray>(NewPrimitiveArray(__FUNCTION__, env, length,
                                                      Primitive::kPrimBoolean));
  }

  static jbyteArray NewByteArray(JNIEnv* env, jsize length) {
    return down_cast<jbyteArray>(NewPrimitiveArray(__FUNCTION__, env, length,
                                                   Primitive::kPrimByte));
  }

  static jcharArray NewCharArray(JNIEnv* env, jsize length) {
    return down_cast<jcharArray>(NewPrimitiveArray(__FUNCTION__, env, length,
                                                   Primitive::kPrimChar));
  }

  static jshortArray NewShortArray(JNIEnv* env, jsize length) {
    return down_cast<jshortArray>(NewPrimitiveArray(__FUNCTION__, env, length,
                                                    Primitive::kPrimShort));
  }

  static jintArray NewIntArray(JNIEnv* env, jsize length) {
    return down_cast<jintArray>(NewPrimitiveArray(__FUNCTION__, env, length, Primitive::kPrimInt));
  }

  static jlongArray NewLongArray(JNIEnv* env, jsize length) {
    return down_cast<jlongArray>(NewPrimitiveArray(__FUNCTION__, env, length,
                                                   Primitive::kPrimLong));
  }

  static jfloatArray NewFloatArray(JNIEnv* env, jsize length) {
    return down_cast<jfloatArray>(NewPrimitiveArray(__FUNCTION__, env, length,
                                                    Primitive::kPrimFloat));
  }

  static jdoubleArray NewDoubleArray(JNIEnv* env, jsize length) {
    return down_cast<jdoubleArray>(NewPrimitiveArray(__FUNCTION__, env, length,
                                                     Primitive::kPrimDouble));
  }

// NOLINT added to avoid wrong warning/fix from clang-tidy.
#define PRIMITIVE_ARRAY_FUNCTIONS(ctype, name, ptype) \
  static ctype* Get##name##ArrayElements(JNIEnv* env, ctype##Array array, jboolean* is_copy) { /* NOLINT */ \
    return reinterpret_cast<ctype*>( /* NOLINT */ \
        GetPrimitiveArrayElements(__FUNCTION__, ptype, env, array, is_copy)); \
  } \
  \
  static void Release##name##ArrayElements(JNIEnv* env, ctype##Array array, ctype* elems, /* NOLINT */ \
                                           jint mode) { \
    ReleasePrimitiveArrayElements(__FUNCTION__, ptype, env, array, elems, mode); \
  } \
  \
  static void Get##name##ArrayRegion(JNIEnv* env, ctype##Array array, jsize start, jsize len, \
                                     ctype* buf) { /* NOLINT */ \
    GetPrimitiveArrayRegion(__FUNCTION__, ptype, env, array, start, len, buf); \
  } \
  \
  static void Set##name##ArrayRegion(JNIEnv* env, ctype##Array array, jsize start, jsize len, \
                                     const ctype* buf) { \
    SetPrimitiveArrayRegion(__FUNCTION__, ptype, env, array, start, len, buf); \
  }

  PRIMITIVE_ARRAY_FUNCTIONS(jboolean, Boolean, Primitive::kPrimBoolean)
  PRIMITIVE_ARRAY_FUNCTIONS(jbyte, Byte, Primitive::kPrimByte)
  PRIMITIVE_ARRAY_FUNCTIONS(jchar, Char, Primitive::kPrimChar)
  PRIMITIVE_ARRAY_FUNCTIONS(jshort, Short, Primitive::kPrimShort)
  PRIMITIVE_ARRAY_FUNCTIONS(jint, Int, Primitive::kPrimInt)
  PRIMITIVE_ARRAY_FUNCTIONS(jlong, Long, Primitive::kPrimLong)
  PRIMITIVE_ARRAY_FUNCTIONS(jfloat, Float, Primitive::kPrimFloat)
  PRIMITIVE_ARRAY_FUNCTIONS(jdouble, Double, Primitive::kPrimDouble)
#undef PRIMITIVE_ARRAY_FUNCTIONS

  static jint MonitorEnter(JNIEnv* env, jobject obj) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.L = obj}};
    if (sc.Check(soa, true, "EL", args)) {
      if (obj != nullptr) {
        down_cast<JNIEnvExt*>(env)->RecordMonitorEnter(obj);
      }
      JniValueType result;
      result.i = baseEnv(env)->MonitorEnter(env, obj);
      if (sc.Check(soa, false, "i", &result)) {
        return result.i;
      }
    }
    return JNI_ERR;
  }

  static jint MonitorExit(JNIEnv* env, jobject obj) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_ExcepOkay, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.L = obj}};
    if (sc.Check(soa, true, "EL", args)) {
      if (obj != nullptr) {
        down_cast<JNIEnvExt*>(env)->CheckMonitorRelease(obj);
      }
      JniValueType result;
      result.i = baseEnv(env)->MonitorExit(env, obj);
      if (sc.Check(soa, false, "i", &result)) {
        return result.i;
      }
    }
    return JNI_ERR;
  }

  static void* GetPrimitiveArrayCritical(JNIEnv* env, jarray array, jboolean* is_copy) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_CritGet, __FUNCTION__);
    JniValueType args[3] = {{.E = env}, {.a = array}, {.p = is_copy}};
    if (sc.Check(soa, true, "Eap", args)) {
      JniValueType result;
      void* ptr = baseEnv(env)->GetPrimitiveArrayCritical(env, array, is_copy);
      if (ptr != nullptr && soa.ForceCopy()) {
        ptr = GuardedCopy::CreateGuardedPACopy(env, array, is_copy, ptr);
      }
      result.p = ptr;
      if (sc.Check(soa, false, "p", &result)) {
        return const_cast<void*>(result.p);
      }
    }
    return nullptr;
  }

  static void ReleasePrimitiveArrayCritical(JNIEnv* env, jarray array, void* carray, jint mode) {
    CHECK_ATTACHED_THREAD_VOID(__FUNCTION__);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_CritRelease | kFlag_ExcepOkay, __FUNCTION__);
    sc.CheckNonNull(carray);
    JniValueType args[4] = {{.E = env}, {.a = array}, {.p = carray}, {.r = mode}};
    if (sc.Check(soa, true, "Eapr", args)) {
      if (soa.ForceCopy()) {
        carray = GuardedCopy::ReleaseGuardedPACopy(__FUNCTION__, env, array, carray, mode);
      }
      baseEnv(env)->ReleasePrimitiveArrayCritical(env, array, carray, mode);
      JniValueType result;
      result.V = nullptr;
      sc.Check(soa, false, "V", &result);
    }
  }

  static jobject NewDirectByteBuffer(JNIEnv* env, void* address, jlong capacity) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[3] = {{.E = env}, {.p = address}, {.J = capacity}};
    if (sc.Check(soa, true, "EpJ", args)) {
      JniValueType result;
      // Note: the validity of address and capacity are checked in the base implementation.
      result.L = baseEnv(env)->NewDirectByteBuffer(env, address, capacity);
      if (sc.Check(soa, false, "L", &result)) {
        return result.L;
      }
    }
    return nullptr;
  }

  static void* GetDirectBufferAddress(JNIEnv* env, jobject buf) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.L = buf}};
    if (sc.Check(soa, true, "EL", args)) {
      JniValueType result;
      // Note: this is implemented in the base environment by a GetLongField which will sanity
      // check the type of buf in GetLongField above.
      result.p = baseEnv(env)->GetDirectBufferAddress(env, buf);
      if (sc.Check(soa, false, "p", &result)) {
        return const_cast<void*>(result.p);
      }
    }
    return nullptr;
  }

  static jlong GetDirectBufferCapacity(JNIEnv* env, jobject buf) {
    CHECK_ATTACHED_THREAD(__FUNCTION__, JNI_ERR);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, __FUNCTION__);
    JniValueType args[2] = {{.E = env}, {.L = buf}};
    if (sc.Check(soa, true, "EL", args)) {
      JniValueType result;
      // Note: this is implemented in the base environment by a GetIntField which will sanity
      // check the type of buf in GetIntField above.
      result.J = baseEnv(env)->GetDirectBufferCapacity(env, buf);
      if (sc.Check(soa, false, "J", &result)) {
        return result.J;
      }
    }
    return JNI_ERR;
  }

 private:
  static JavaVMExt* GetJavaVMExt(JNIEnv* env) {
    return reinterpret_cast<JNIEnvExt*>(env)->GetVm();
  }

  static const JNINativeInterface* baseEnv(JNIEnv* env) {
    return reinterpret_cast<JNIEnvExt*>(env)->GetUncheckedFunctions();
  }

  static jobject NewRef(const char* function_name, JNIEnv* env, jobject obj, IndirectRefKind kind) {
    CHECK_ATTACHED_THREAD(function_name, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, function_name);
    JniValueType args[2] = {{.E = env}, {.L = obj}};
    if (sc.Check(soa, true, "EL", args)) {
      JniValueType result;
      switch (kind) {
        case kGlobal:
          result.L = baseEnv(env)->NewGlobalRef(env, obj);
          break;
        case kLocal:
          result.L = baseEnv(env)->NewLocalRef(env, obj);
          break;
        case kWeakGlobal:
          result.L = baseEnv(env)->NewWeakGlobalRef(env, obj);
          break;
        default:
          LOG(FATAL) << "Unexpected reference kind: " << kind;
      }
      if (sc.Check(soa, false, "L", &result)) {
        DCHECK_EQ(IsSameObject(env, obj, result.L), JNI_TRUE);
        DCHECK(sc.CheckReferenceKind(kind, soa.Self(), result.L));
        return result.L;
      }
    }
    return nullptr;
  }

  static void DeleteRef(const char* function_name, JNIEnv* env, jobject obj, IndirectRefKind kind) {
    CHECK_ATTACHED_THREAD_VOID(function_name);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_ExcepOkay, function_name);
    JniValueType args[2] = {{.E = env}, {.L = obj}};
    sc.Check(soa, true, "EL", args);
    if (sc.CheckReferenceKind(kind, soa.Self(), obj)) {
      JniValueType result;
      switch (kind) {
        case kGlobal:
          baseEnv(env)->DeleteGlobalRef(env, obj);
          break;
        case kLocal:
          baseEnv(env)->DeleteLocalRef(env, obj);
          break;
        case kWeakGlobal:
          baseEnv(env)->DeleteWeakGlobalRef(env, obj);
          break;
        default:
          LOG(FATAL) << "Unexpected reference kind: " << kind;
      }
      result.V = nullptr;
      sc.Check(soa, false, "V", &result);
    }
  }

  static jmethodID GetMethodIDInternal(const char* function_name, JNIEnv* env, jclass c,
                                       const char* name, const char* sig, bool is_static) {
    CHECK_ATTACHED_THREAD(function_name, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, function_name);
    JniValueType args[4] = {{.E = env}, {.c = c}, {.u = name}, {.u = sig}};
    if (sc.Check(soa, true, "Ecuu", args)) {
      JniValueType result;
      if (is_static) {
        result.m = baseEnv(env)->GetStaticMethodID(env, c, name, sig);
      } else {
        result.m = baseEnv(env)->GetMethodID(env, c, name, sig);
      }
      if (sc.Check(soa, false, "m", &result)) {
        return result.m;
      }
    }
    return nullptr;
  }

  static jfieldID GetFieldIDInternal(const char* function_name, JNIEnv* env, jclass c,
                                     const char* name, const char* sig, bool is_static) {
    CHECK_ATTACHED_THREAD(function_name, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, function_name);
    JniValueType args[4] = {{.E = env}, {.c = c}, {.u = name}, {.u = sig}};
    if (sc.Check(soa, true, "Ecuu", args)) {
      JniValueType result;
      if (is_static) {
        result.f = baseEnv(env)->GetStaticFieldID(env, c, name, sig);
      } else {
        result.f = baseEnv(env)->GetFieldID(env, c, name, sig);
      }
      if (sc.Check(soa, false, "f", &result)) {
        return result.f;
      }
    }
    return nullptr;
  }

  static JniValueType GetField(const char* function_name, JNIEnv* env, jobject obj, jfieldID fid,
                               bool is_static, Primitive::Type type) {
    CHECK_ATTACHED_THREAD(function_name, JniValueType());
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, function_name);
    JniValueType args[3] = {{.E = env}, {.L = obj}, {.f = fid}};
    JniValueType result;
    if (sc.Check(soa, true, is_static ? "Ecf" : "ELf", args) &&
        sc.CheckFieldAccess(soa, obj, fid, is_static, type)) {
      const char* result_check = nullptr;
      switch (type) {
        case Primitive::kPrimNot:
          if (is_static) {
            result.L = baseEnv(env)->GetStaticObjectField(env, down_cast<jclass>(obj), fid);
          } else {
            result.L = baseEnv(env)->GetObjectField(env, obj, fid);
          }
          result_check = "L";
          break;
        case Primitive::kPrimBoolean:
          if (is_static) {
            result.Z = baseEnv(env)->GetStaticBooleanField(env, down_cast<jclass>(obj), fid);
          } else {
            result.Z = baseEnv(env)->GetBooleanField(env, obj, fid);
          }
          result_check = "Z";
          break;
        case Primitive::kPrimByte:
          if (is_static) {
            result.B = baseEnv(env)->GetStaticByteField(env, down_cast<jclass>(obj), fid);
          } else {
            result.B = baseEnv(env)->GetByteField(env, obj, fid);
          }
          result_check = "B";
          break;
        case Primitive::kPrimChar:
          if (is_static) {
            result.C = baseEnv(env)->GetStaticCharField(env, down_cast<jclass>(obj), fid);
          } else {
            result.C = baseEnv(env)->GetCharField(env, obj, fid);
          }
          result_check = "C";
          break;
        case Primitive::kPrimShort:
          if (is_static) {
            result.S = baseEnv(env)->GetStaticShortField(env, down_cast<jclass>(obj), fid);
          } else {
            result.S = baseEnv(env)->GetShortField(env, obj, fid);
          }
          result_check = "S";
          break;
        case Primitive::kPrimInt:
          if (is_static) {
            result.I = baseEnv(env)->GetStaticIntField(env, down_cast<jclass>(obj), fid);
          } else {
            result.I = baseEnv(env)->GetIntField(env, obj, fid);
          }
          result_check = "I";
          break;
        case Primitive::kPrimLong:
          if (is_static) {
            result.J = baseEnv(env)->GetStaticLongField(env, down_cast<jclass>(obj), fid);
          } else {
            result.J = baseEnv(env)->GetLongField(env, obj, fid);
          }
          result_check = "J";
          break;
        case Primitive::kPrimFloat:
          if (is_static) {
            result.F = baseEnv(env)->GetStaticFloatField(env, down_cast<jclass>(obj), fid);
          } else {
            result.F = baseEnv(env)->GetFloatField(env, obj, fid);
          }
          result_check = "F";
          break;
        case Primitive::kPrimDouble:
          if (is_static) {
            result.D = baseEnv(env)->GetStaticDoubleField(env, down_cast<jclass>(obj), fid);
          } else {
            result.D = baseEnv(env)->GetDoubleField(env, obj, fid);
          }
          result_check = "D";
          break;
        case Primitive::kPrimVoid:
          LOG(FATAL) << "Unexpected type: " << type;
          break;
      }
      if (sc.Check(soa, false, result_check, &result)) {
        return result;
      }
    }
    result.J = 0;
    return result;
  }

  static void SetField(const char* function_name, JNIEnv* env, jobject obj, jfieldID fid,
                       bool is_static, Primitive::Type type, JniValueType value) {
    CHECK_ATTACHED_THREAD_VOID(function_name);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, function_name);
    JniValueType args[4] = {{.E = env}, {.L = obj}, {.f = fid}, value};
    char sig[5] = { 'E', is_static ? 'c' : 'L', 'f',
        type == Primitive::kPrimNot ? 'L' : Primitive::Descriptor(type)[0], '\0'};
    if (sc.Check(soa, true, sig, args) &&
        sc.CheckFieldAccess(soa, obj, fid, is_static, type)) {
      switch (type) {
        case Primitive::kPrimNot:
          if (is_static) {
            baseEnv(env)->SetStaticObjectField(env, down_cast<jclass>(obj), fid, value.L);
          } else {
            baseEnv(env)->SetObjectField(env, obj, fid, value.L);
          }
          break;
        case Primitive::kPrimBoolean:
          if (is_static) {
            baseEnv(env)->SetStaticBooleanField(env, down_cast<jclass>(obj), fid, value.Z);
          } else {
            baseEnv(env)->SetBooleanField(env, obj, fid, value.Z);
          }
          break;
        case Primitive::kPrimByte:
          if (is_static) {
            baseEnv(env)->SetStaticByteField(env, down_cast<jclass>(obj), fid, value.B);
          } else {
            baseEnv(env)->SetByteField(env, obj, fid, value.B);
          }
          break;
        case Primitive::kPrimChar:
          if (is_static) {
            baseEnv(env)->SetStaticCharField(env, down_cast<jclass>(obj), fid, value.C);
          } else {
            baseEnv(env)->SetCharField(env, obj, fid, value.C);
          }
          break;
        case Primitive::kPrimShort:
          if (is_static) {
            baseEnv(env)->SetStaticShortField(env, down_cast<jclass>(obj), fid, value.S);
          } else {
            baseEnv(env)->SetShortField(env, obj, fid, value.S);
          }
          break;
        case Primitive::kPrimInt:
          if (is_static) {
            baseEnv(env)->SetStaticIntField(env, down_cast<jclass>(obj), fid, value.I);
          } else {
            baseEnv(env)->SetIntField(env, obj, fid, value.I);
          }
          break;
        case Primitive::kPrimLong:
          if (is_static) {
            baseEnv(env)->SetStaticLongField(env, down_cast<jclass>(obj), fid, value.J);
          } else {
            baseEnv(env)->SetLongField(env, obj, fid, value.J);
          }
          break;
        case Primitive::kPrimFloat:
          if (is_static) {
            baseEnv(env)->SetStaticFloatField(env, down_cast<jclass>(obj), fid, value.F);
          } else {
            baseEnv(env)->SetFloatField(env, obj, fid, value.F);
          }
          break;
        case Primitive::kPrimDouble:
          if (is_static) {
            baseEnv(env)->SetStaticDoubleField(env, down_cast<jclass>(obj), fid, value.D);
          } else {
            baseEnv(env)->SetDoubleField(env, obj, fid, value.D);
          }
          break;
        case Primitive::kPrimVoid:
          LOG(FATAL) << "Unexpected type: " << type;
          break;
      }
      JniValueType result;
      result.V = nullptr;
      sc.Check(soa, false, "V", &result);
    }
  }

  static bool CheckCallArgs(ScopedObjectAccess& soa, ScopedCheck& sc, JNIEnv* env, jobject obj,
                            jclass c, jmethodID mid, InvokeType invoke, const VarArgs* vargs)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    bool checked;
    switch (invoke) {
      case kVirtual: {
        DCHECK(c == nullptr);
        JniValueType args[4] = {{.E = env}, {.L = obj}, {.m = mid}, {.va = vargs}};
        checked = sc.Check(soa, true, "ELm.", args);
        break;
      }
      case kDirect: {
        JniValueType args[5] = {{.E = env}, {.L = obj}, {.c = c}, {.m = mid}, {.va = vargs}};
        checked = sc.Check(soa, true, "ELcm.", args);
        break;
      }
      case kStatic: {
        DCHECK(obj == nullptr);
        JniValueType args[4] = {{.E = env}, {.c = c}, {.m = mid}, {.va = vargs}};
        checked = sc.Check(soa, true, "Ecm.", args);
        break;
      }
      default:
        LOG(FATAL) << "Unexpected invoke: " << invoke;
        checked = false;
        break;
    }
    return checked;
  }

  static JniValueType CallMethodA(const char* function_name, JNIEnv* env, jobject obj, jclass c,
                                  jmethodID mid, jvalue* vargs, Primitive::Type type,
                                  InvokeType invoke) {
    CHECK_ATTACHED_THREAD(function_name, JniValueType());
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, function_name);
    JniValueType result;
    VarArgs rest(mid, vargs);
    if (CheckCallArgs(soa, sc, env, obj, c, mid, invoke, &rest) &&
        sc.CheckMethodAndSig(soa, obj, c, mid, type, invoke)) {
      const char* result_check;
      switch (type) {
        case Primitive::kPrimNot:
          result_check = "L";
          switch (invoke) {
            case kVirtual:
              result.L = baseEnv(env)->CallObjectMethodA(env, obj, mid, vargs);
              break;
            case kDirect:
              result.L = baseEnv(env)->CallNonvirtualObjectMethodA(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.L = baseEnv(env)->CallStaticObjectMethodA(env, c, mid, vargs);
              break;
            default:
              break;
          }
          break;
        case Primitive::kPrimBoolean:
          result_check = "Z";
          switch (invoke) {
            case kVirtual:
              result.Z = baseEnv(env)->CallBooleanMethodA(env, obj, mid, vargs);
              break;
            case kDirect:
              result.Z = baseEnv(env)->CallNonvirtualBooleanMethodA(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.Z = baseEnv(env)->CallStaticBooleanMethodA(env, c, mid, vargs);
              break;
            default:
              break;
          }
          break;
        case Primitive::kPrimByte:
          result_check = "B";
          switch (invoke) {
            case kVirtual:
              result.B = baseEnv(env)->CallByteMethodA(env, obj, mid, vargs);
              break;
            case kDirect:
              result.B = baseEnv(env)->CallNonvirtualByteMethodA(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.B = baseEnv(env)->CallStaticByteMethodA(env, c, mid, vargs);
              break;
            default:
              break;
          }
          break;
        case Primitive::kPrimChar:
          result_check = "C";
          switch (invoke) {
            case kVirtual:
              result.C = baseEnv(env)->CallCharMethodA(env, obj, mid, vargs);
              break;
            case kDirect:
              result.C = baseEnv(env)->CallNonvirtualCharMethodA(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.C = baseEnv(env)->CallStaticCharMethodA(env, c, mid, vargs);
              break;
            default:
              break;
          }
          break;
        case Primitive::kPrimShort:
          result_check = "S";
          switch (invoke) {
            case kVirtual:
              result.S = baseEnv(env)->CallShortMethodA(env, obj, mid, vargs);
              break;
            case kDirect:
              result.S = baseEnv(env)->CallNonvirtualShortMethodA(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.S = baseEnv(env)->CallStaticShortMethodA(env, c, mid, vargs);
              break;
            default:
              break;
          }
          break;
        case Primitive::kPrimInt:
          result_check = "I";
          switch (invoke) {
            case kVirtual:
              result.I = baseEnv(env)->CallIntMethodA(env, obj, mid, vargs);
              break;
            case kDirect:
              result.I = baseEnv(env)->CallNonvirtualIntMethodA(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.I = baseEnv(env)->CallStaticIntMethodA(env, c, mid, vargs);
              break;
            default:
              break;
          }
          break;
        case Primitive::kPrimLong:
          result_check = "J";
          switch (invoke) {
            case kVirtual:
              result.J = baseEnv(env)->CallLongMethodA(env, obj, mid, vargs);
              break;
            case kDirect:
              result.J = baseEnv(env)->CallNonvirtualLongMethodA(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.J = baseEnv(env)->CallStaticLongMethodA(env, c, mid, vargs);
              break;
            default:
              break;
          }
          break;
        case Primitive::kPrimFloat:
          result_check = "F";
          switch (invoke) {
            case kVirtual:
              result.F = baseEnv(env)->CallFloatMethodA(env, obj, mid, vargs);
              break;
            case kDirect:
              result.F = baseEnv(env)->CallNonvirtualFloatMethodA(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.F = baseEnv(env)->CallStaticFloatMethodA(env, c, mid, vargs);
              break;
            default:
              break;
          }
          break;
        case Primitive::kPrimDouble:
          result_check = "D";
          switch (invoke) {
            case kVirtual:
              result.D = baseEnv(env)->CallDoubleMethodA(env, obj, mid, vargs);
              break;
            case kDirect:
              result.D = baseEnv(env)->CallNonvirtualDoubleMethodA(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.D = baseEnv(env)->CallStaticDoubleMethodA(env, c, mid, vargs);
              break;
            default:
              break;
          }
          break;
        case Primitive::kPrimVoid:
          result_check = "V";
          result.V = nullptr;
          switch (invoke) {
            case kVirtual:
              baseEnv(env)->CallVoidMethodA(env, obj, mid, vargs);
              break;
            case kDirect:
              baseEnv(env)->CallNonvirtualVoidMethodA(env, obj, c, mid, vargs);
              break;
            case kStatic:
              baseEnv(env)->CallStaticVoidMethodA(env, c, mid, vargs);
              break;
            default:
              LOG(FATAL) << "Unexpected invoke: " << invoke;
          }
          break;
        default:
          LOG(FATAL) << "Unexpected return type: " << type;
          result_check = nullptr;
      }
      if (sc.Check(soa, false, result_check, &result)) {
        return result;
      }
    }
    result.J = 0;
    return result;
  }

  static JniValueType CallMethodV(const char* function_name, JNIEnv* env, jobject obj, jclass c,
                                  jmethodID mid, va_list vargs, Primitive::Type type,
                                  InvokeType invoke) {
    CHECK_ATTACHED_THREAD(function_name, JniValueType());
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, function_name);
    JniValueType result;
    VarArgs rest(mid, vargs);
    if (CheckCallArgs(soa, sc, env, obj, c, mid, invoke, &rest) &&
        sc.CheckMethodAndSig(soa, obj, c, mid, type, invoke)) {
      const char* result_check;
      switch (type) {
        case Primitive::kPrimNot:
          result_check = "L";
          switch (invoke) {
            case kVirtual:
              result.L = baseEnv(env)->CallObjectMethodV(env, obj, mid, vargs);
              break;
            case kDirect:
              result.L = baseEnv(env)->CallNonvirtualObjectMethodV(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.L = baseEnv(env)->CallStaticObjectMethodV(env, c, mid, vargs);
              break;
            default:
              LOG(FATAL) << "Unexpected invoke: " << invoke;
          }
          break;
        case Primitive::kPrimBoolean:
          result_check = "Z";
          switch (invoke) {
            case kVirtual:
              result.Z = baseEnv(env)->CallBooleanMethodV(env, obj, mid, vargs);
              break;
            case kDirect:
              result.Z = baseEnv(env)->CallNonvirtualBooleanMethodV(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.Z = baseEnv(env)->CallStaticBooleanMethodV(env, c, mid, vargs);
              break;
            default:
              LOG(FATAL) << "Unexpected invoke: " << invoke;
          }
          break;
        case Primitive::kPrimByte:
          result_check = "B";
          switch (invoke) {
            case kVirtual:
              result.B = baseEnv(env)->CallByteMethodV(env, obj, mid, vargs);
              break;
            case kDirect:
              result.B = baseEnv(env)->CallNonvirtualByteMethodV(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.B = baseEnv(env)->CallStaticByteMethodV(env, c, mid, vargs);
              break;
            default:
              LOG(FATAL) << "Unexpected invoke: " << invoke;
          }
          break;
        case Primitive::kPrimChar:
          result_check = "C";
          switch (invoke) {
            case kVirtual:
              result.C = baseEnv(env)->CallCharMethodV(env, obj, mid, vargs);
              break;
            case kDirect:
              result.C = baseEnv(env)->CallNonvirtualCharMethodV(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.C = baseEnv(env)->CallStaticCharMethodV(env, c, mid, vargs);
              break;
            default:
              LOG(FATAL) << "Unexpected invoke: " << invoke;
          }
          break;
        case Primitive::kPrimShort:
          result_check = "S";
          switch (invoke) {
            case kVirtual:
              result.S = baseEnv(env)->CallShortMethodV(env, obj, mid, vargs);
              break;
            case kDirect:
              result.S = baseEnv(env)->CallNonvirtualShortMethodV(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.S = baseEnv(env)->CallStaticShortMethodV(env, c, mid, vargs);
              break;
            default:
              LOG(FATAL) << "Unexpected invoke: " << invoke;
          }
          break;
        case Primitive::kPrimInt:
          result_check = "I";
          switch (invoke) {
            case kVirtual:
              result.I = baseEnv(env)->CallIntMethodV(env, obj, mid, vargs);
              break;
            case kDirect:
              result.I = baseEnv(env)->CallNonvirtualIntMethodV(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.I = baseEnv(env)->CallStaticIntMethodV(env, c, mid, vargs);
              break;
            default:
              LOG(FATAL) << "Unexpected invoke: " << invoke;
          }
          break;
        case Primitive::kPrimLong:
          result_check = "J";
          switch (invoke) {
            case kVirtual:
              result.J = baseEnv(env)->CallLongMethodV(env, obj, mid, vargs);
              break;
            case kDirect:
              result.J = baseEnv(env)->CallNonvirtualLongMethodV(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.J = baseEnv(env)->CallStaticLongMethodV(env, c, mid, vargs);
              break;
            default:
              LOG(FATAL) << "Unexpected invoke: " << invoke;
          }
          break;
        case Primitive::kPrimFloat:
          result_check = "F";
          switch (invoke) {
            case kVirtual:
              result.F = baseEnv(env)->CallFloatMethodV(env, obj, mid, vargs);
              break;
            case kDirect:
              result.F = baseEnv(env)->CallNonvirtualFloatMethodV(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.F = baseEnv(env)->CallStaticFloatMethodV(env, c, mid, vargs);
              break;
            default:
              LOG(FATAL) << "Unexpected invoke: " << invoke;
          }
          break;
        case Primitive::kPrimDouble:
          result_check = "D";
          switch (invoke) {
            case kVirtual:
              result.D = baseEnv(env)->CallDoubleMethodV(env, obj, mid, vargs);
              break;
            case kDirect:
              result.D = baseEnv(env)->CallNonvirtualDoubleMethodV(env, obj, c, mid, vargs);
              break;
            case kStatic:
              result.D = baseEnv(env)->CallStaticDoubleMethodV(env, c, mid, vargs);
              break;
            default:
              LOG(FATAL) << "Unexpected invoke: " << invoke;
          }
          break;
        case Primitive::kPrimVoid:
          result_check = "V";
          result.V = nullptr;
          switch (invoke) {
            case kVirtual:
              baseEnv(env)->CallVoidMethodV(env, obj, mid, vargs);
              break;
            case kDirect:
              baseEnv(env)->CallNonvirtualVoidMethodV(env, obj, c, mid, vargs);
              break;
            case kStatic:
              baseEnv(env)->CallStaticVoidMethodV(env, c, mid, vargs);
              break;
            default:
              LOG(FATAL) << "Unexpected invoke: " << invoke;
          }
          break;
        default:
          LOG(FATAL) << "Unexpected return type: " << type;
          result_check = nullptr;
      }
      if (sc.Check(soa, false, result_check, &result)) {
        return result;
      }
    }
    result.J = 0;
    return result;
  }

  static const void* GetStringCharsInternal(const char* function_name, JNIEnv* env, jstring string,
                                            jboolean* is_copy, bool utf, bool critical) {
    CHECK_ATTACHED_THREAD(function_name, nullptr);
    ScopedObjectAccess soa(env);
    int flags = critical ? kFlag_CritGet : kFlag_CritOkay;
    ScopedCheck sc(flags, function_name);
    JniValueType args[3] = {{.E = env}, {.s = string}, {.p = is_copy}};
    if (sc.Check(soa, true, "Esp", args)) {
      JniValueType result;
      void* ptr;
      if (utf) {
        CHECK(!critical);
        ptr = const_cast<char*>(baseEnv(env)->GetStringUTFChars(env, string, is_copy));
        result.u = reinterpret_cast<char*>(ptr);
      } else {
        ptr = const_cast<jchar*>(critical ? baseEnv(env)->GetStringCritical(env, string, is_copy) :
            baseEnv(env)->GetStringChars(env, string, is_copy));
        result.p = ptr;
      }
      // TODO: could we be smarter about not copying when local_is_copy?
      if (ptr != nullptr && soa.ForceCopy()) {
        if (utf) {
          size_t length_in_bytes = strlen(result.u) + 1;
          result.u =
              reinterpret_cast<const char*>(GuardedCopy::Create(ptr, length_in_bytes, false));
        } else {
          size_t length_in_bytes = baseEnv(env)->GetStringLength(env, string) * 2;
          result.p =
              reinterpret_cast<const jchar*>(GuardedCopy::Create(ptr, length_in_bytes, false));
        }
        if (is_copy != nullptr) {
          *is_copy = JNI_TRUE;
        }
      }
      if (sc.Check(soa, false, utf ? "u" : "p", &result)) {
        return utf ? result.u : result.p;
      }
    }
    return nullptr;
  }

  static void ReleaseStringCharsInternal(const char* function_name, JNIEnv* env, jstring string,
                                         const void* chars, bool utf, bool critical) {
    CHECK_ATTACHED_THREAD_VOID(function_name);
    ScopedObjectAccess soa(env);
    int flags = kFlag_ExcepOkay | kFlag_Release;
    if (critical) {
      flags |= kFlag_CritRelease;
    }
    ScopedCheck sc(flags, function_name);
    sc.CheckNonNull(chars);
    bool force_copy_ok = !soa.ForceCopy() || GuardedCopy::Check(function_name, chars, false);
    if (force_copy_ok && soa.ForceCopy()) {
      chars = reinterpret_cast<const jchar*>(GuardedCopy::Destroy(const_cast<void*>(chars)));
    }
    if (force_copy_ok) {
      JniValueType args[3] = {{.E = env}, {.s = string}, {.p = chars}};
      if (sc.Check(soa, true, utf ? "Esu" : "Esp", args)) {
        if (utf) {
          CHECK(!critical);
          baseEnv(env)->ReleaseStringUTFChars(env, string, reinterpret_cast<const char*>(chars));
        } else {
          if (critical) {
            baseEnv(env)->ReleaseStringCritical(env, string, reinterpret_cast<const jchar*>(chars));
          } else {
            baseEnv(env)->ReleaseStringChars(env, string, reinterpret_cast<const jchar*>(chars));
          }
        }
        JniValueType result;
        sc.Check(soa, false, "V", &result);
      }
    }
  }

  static jarray NewPrimitiveArray(const char* function_name, JNIEnv* env, jsize length,
                                  Primitive::Type type) {
    CHECK_ATTACHED_THREAD(function_name, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, function_name);
    JniValueType args[2] = {{.E = env}, {.z = length}};
    if (sc.Check(soa, true, "Ez", args)) {
      JniValueType result;
      switch (type) {
        case Primitive::kPrimBoolean:
          result.a = baseEnv(env)->NewBooleanArray(env, length);
          break;
        case Primitive::kPrimByte:
          result.a = baseEnv(env)->NewByteArray(env, length);
          break;
        case Primitive::kPrimChar:
          result.a = baseEnv(env)->NewCharArray(env, length);
          break;
        case Primitive::kPrimShort:
          result.a = baseEnv(env)->NewShortArray(env, length);
          break;
        case Primitive::kPrimInt:
          result.a = baseEnv(env)->NewIntArray(env, length);
          break;
        case Primitive::kPrimLong:
          result.a = baseEnv(env)->NewLongArray(env, length);
          break;
        case Primitive::kPrimFloat:
          result.a = baseEnv(env)->NewFloatArray(env, length);
          break;
        case Primitive::kPrimDouble:
          result.a = baseEnv(env)->NewDoubleArray(env, length);
          break;
        default:
          LOG(FATAL) << "Unexpected primitive type: " << type;
      }
      if (sc.Check(soa, false, "a", &result)) {
        return result.a;
      }
    }
    return nullptr;
  }

  static void* GetPrimitiveArrayElements(const char* function_name, Primitive::Type type,
                                         JNIEnv* env, jarray array, jboolean* is_copy) {
    CHECK_ATTACHED_THREAD(function_name, nullptr);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, function_name);
    JniValueType args[3] = {{.E = env}, {.a = array}, {.p = is_copy}};
    if (sc.Check(soa, true, "Eap", args) && sc.CheckPrimitiveArrayType(soa, array, type)) {
      JniValueType result;
      void* ptr = nullptr;
      switch (type) {
        case Primitive::kPrimBoolean:
          ptr = baseEnv(env)->GetBooleanArrayElements(env, down_cast<jbooleanArray>(array),
                                                      is_copy);
          break;
        case Primitive::kPrimByte:
          ptr = baseEnv(env)->GetByteArrayElements(env, down_cast<jbyteArray>(array), is_copy);
          break;
        case Primitive::kPrimChar:
          ptr = baseEnv(env)->GetCharArrayElements(env, down_cast<jcharArray>(array), is_copy);
          break;
        case Primitive::kPrimShort:
          ptr = baseEnv(env)->GetShortArrayElements(env, down_cast<jshortArray>(array), is_copy);
          break;
        case Primitive::kPrimInt:
          ptr = baseEnv(env)->GetIntArrayElements(env, down_cast<jintArray>(array), is_copy);
          break;
        case Primitive::kPrimLong:
          ptr = baseEnv(env)->GetLongArrayElements(env, down_cast<jlongArray>(array), is_copy);
          break;
        case Primitive::kPrimFloat:
          ptr = baseEnv(env)->GetFloatArrayElements(env, down_cast<jfloatArray>(array), is_copy);
          break;
        case Primitive::kPrimDouble:
          ptr = baseEnv(env)->GetDoubleArrayElements(env, down_cast<jdoubleArray>(array), is_copy);
          break;
        default:
          LOG(FATAL) << "Unexpected primitive type: " << type;
      }
      if (ptr != nullptr && soa.ForceCopy()) {
        ptr = GuardedCopy::CreateGuardedPACopy(env, array, is_copy, ptr);
        if (is_copy != nullptr) {
          *is_copy = JNI_TRUE;
        }
      }
      result.p = ptr;
      if (sc.Check(soa, false, "p", &result)) {
        return const_cast<void*>(result.p);
      }
    }
    return nullptr;
  }

  static void ReleasePrimitiveArrayElements(const char* function_name, Primitive::Type type,
                                            JNIEnv* env, jarray array, void* elems, jint mode) {
    CHECK_ATTACHED_THREAD_VOID(function_name);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_ExcepOkay, function_name);
    if (sc.CheckNonNull(elems) && sc.CheckPrimitiveArrayType(soa, array, type)) {
      if (soa.ForceCopy()) {
        elems = GuardedCopy::ReleaseGuardedPACopy(function_name, env, array, elems, mode);
      }
      if (!soa.ForceCopy() || elems != nullptr) {
        JniValueType args[4] = {{.E = env}, {.a = array}, {.p = elems}, {.r = mode}};
        if (sc.Check(soa, true, "Eapr", args)) {
          switch (type) {
            case Primitive::kPrimBoolean:
              baseEnv(env)->ReleaseBooleanArrayElements(env, down_cast<jbooleanArray>(array),
                                                        reinterpret_cast<jboolean*>(elems), mode);
              break;
            case Primitive::kPrimByte:
              baseEnv(env)->ReleaseByteArrayElements(env, down_cast<jbyteArray>(array),
                                                     reinterpret_cast<jbyte*>(elems), mode);
              break;
            case Primitive::kPrimChar:
              baseEnv(env)->ReleaseCharArrayElements(env, down_cast<jcharArray>(array),
                                                     reinterpret_cast<jchar*>(elems), mode);
              break;
            case Primitive::kPrimShort:
              baseEnv(env)->ReleaseShortArrayElements(env, down_cast<jshortArray>(array),
                                                      reinterpret_cast<jshort*>(elems), mode);
              break;
            case Primitive::kPrimInt:
              baseEnv(env)->ReleaseIntArrayElements(env, down_cast<jintArray>(array),
                                                    reinterpret_cast<jint*>(elems), mode);
              break;
            case Primitive::kPrimLong:
              baseEnv(env)->ReleaseLongArrayElements(env, down_cast<jlongArray>(array),
                                                     reinterpret_cast<jlong*>(elems), mode);
              break;
            case Primitive::kPrimFloat:
              baseEnv(env)->ReleaseFloatArrayElements(env, down_cast<jfloatArray>(array),
                                                      reinterpret_cast<jfloat*>(elems), mode);
              break;
            case Primitive::kPrimDouble:
              baseEnv(env)->ReleaseDoubleArrayElements(env, down_cast<jdoubleArray>(array),
                                                       reinterpret_cast<jdouble*>(elems), mode);
              break;
            default:
              LOG(FATAL) << "Unexpected primitive type: " << type;
          }
          JniValueType result;
          result.V = nullptr;
          sc.Check(soa, false, "V", &result);
        }
      }
    }
  }

  static void GetPrimitiveArrayRegion(const char* function_name, Primitive::Type type, JNIEnv* env,
                                      jarray array, jsize start, jsize len, void* buf) {
    CHECK_ATTACHED_THREAD_VOID(function_name);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, function_name);
    JniValueType args[5] = {{.E = env}, {.a = array}, {.z = start}, {.z = len}, {.p = buf}};
    // Note: the start and len arguments are checked as 'I' rather than 'z' as invalid indices
    // result in ArrayIndexOutOfBoundsExceptions in the base implementation.
    if (sc.Check(soa, true, "EaIIp", args) && sc.CheckPrimitiveArrayType(soa, array, type)) {
      switch (type) {
        case Primitive::kPrimBoolean:
          baseEnv(env)->GetBooleanArrayRegion(env, down_cast<jbooleanArray>(array), start, len,
                                              reinterpret_cast<jboolean*>(buf));
          break;
        case Primitive::kPrimByte:
          baseEnv(env)->GetByteArrayRegion(env, down_cast<jbyteArray>(array), start, len,
                                           reinterpret_cast<jbyte*>(buf));
          break;
        case Primitive::kPrimChar:
          baseEnv(env)->GetCharArrayRegion(env, down_cast<jcharArray>(array), start, len,
                                           reinterpret_cast<jchar*>(buf));
          break;
        case Primitive::kPrimShort:
          baseEnv(env)->GetShortArrayRegion(env, down_cast<jshortArray>(array), start, len,
                                            reinterpret_cast<jshort*>(buf));
          break;
        case Primitive::kPrimInt:
          baseEnv(env)->GetIntArrayRegion(env, down_cast<jintArray>(array), start, len,
                                          reinterpret_cast<jint*>(buf));
          break;
        case Primitive::kPrimLong:
          baseEnv(env)->GetLongArrayRegion(env, down_cast<jlongArray>(array), start, len,
                                           reinterpret_cast<jlong*>(buf));
          break;
        case Primitive::kPrimFloat:
          baseEnv(env)->GetFloatArrayRegion(env, down_cast<jfloatArray>(array), start, len,
                                            reinterpret_cast<jfloat*>(buf));
          break;
        case Primitive::kPrimDouble:
          baseEnv(env)->GetDoubleArrayRegion(env, down_cast<jdoubleArray>(array), start, len,
                                             reinterpret_cast<jdouble*>(buf));
          break;
        default:
          LOG(FATAL) << "Unexpected primitive type: " << type;
      }
      JniValueType result;
      result.V = nullptr;
      sc.Check(soa, false, "V", &result);
    }
  }

  static void SetPrimitiveArrayRegion(const char* function_name, Primitive::Type type, JNIEnv* env,
                                      jarray array, jsize start, jsize len, const void* buf) {
    CHECK_ATTACHED_THREAD_VOID(function_name);
    ScopedObjectAccess soa(env);
    ScopedCheck sc(kFlag_Default, function_name);
    JniValueType args[5] = {{.E = env}, {.a = array}, {.z = start}, {.z = len}, {.p = buf}};
    // Note: the start and len arguments are checked as 'I' rather than 'z' as invalid indices
    // result in ArrayIndexOutOfBoundsExceptions in the base implementation.
    if (sc.Check(soa, true, "EaIIp", args) && sc.CheckPrimitiveArrayType(soa, array, type)) {
      switch (type) {
        case Primitive::kPrimBoolean:
          baseEnv(env)->SetBooleanArrayRegion(env, down_cast<jbooleanArray>(array), start, len,
                                              reinterpret_cast<const jboolean*>(buf));
          break;
        case Primitive::kPrimByte:
          baseEnv(env)->SetByteArrayRegion(env, down_cast<jbyteArray>(array), start, len,
                                           reinterpret_cast<const jbyte*>(buf));
          break;
        case Primitive::kPrimChar:
          baseEnv(env)->SetCharArrayRegion(env, down_cast<jcharArray>(array), start, len,
                                           reinterpret_cast<const jchar*>(buf));
          break;
        case Primitive::kPrimShort:
          baseEnv(env)->SetShortArrayRegion(env, down_cast<jshortArray>(array), start, len,
                                              reinterpret_cast<const jshort*>(buf));
          break;
        case Primitive::kPrimInt:
          baseEnv(env)->SetIntArrayRegion(env, down_cast<jintArray>(array), start, len,
                                          reinterpret_cast<const jint*>(buf));
          break;
        case Primitive::kPrimLong:
          baseEnv(env)->SetLongArrayRegion(env, down_cast<jlongArray>(array), start, len,
                                              reinterpret_cast<const jlong*>(buf));
          break;
        case Primitive::kPrimFloat:
          baseEnv(env)->SetFloatArrayRegion(env, down_cast<jfloatArray>(array), start, len,
                                            reinterpret_cast<const jfloat*>(buf));
          break;
        case Primitive::kPrimDouble:
          baseEnv(env)->SetDoubleArrayRegion(env, down_cast<jdoubleArray>(array), start, len,
                                             reinterpret_cast<const jdouble*>(buf));
          break;
        default:
          LOG(FATAL) << "Unexpected primitive type: " << type;
      }
      JniValueType result;
      result.V = nullptr;
      sc.Check(soa, false, "V", &result);
    }
  }
};

const JNINativeInterface gCheckNativeInterface = {
  nullptr,  // reserved0.
  nullptr,  // reserved1.
  nullptr,  // reserved2.
  nullptr,  // reserved3.
  CheckJNI::GetVersion,
  CheckJNI::DefineClass,
  CheckJNI::FindClass,
  CheckJNI::FromReflectedMethod,
  CheckJNI::FromReflectedField,
  CheckJNI::ToReflectedMethod,
  CheckJNI::GetSuperclass,
  CheckJNI::IsAssignableFrom,
  CheckJNI::ToReflectedField,
  CheckJNI::Throw,
  CheckJNI::ThrowNew,
  CheckJNI::ExceptionOccurred,
  CheckJNI::ExceptionDescribe,
  CheckJNI::ExceptionClear,
  CheckJNI::FatalError,
  CheckJNI::PushLocalFrame,
  CheckJNI::PopLocalFrame,
  CheckJNI::NewGlobalRef,
  CheckJNI::DeleteGlobalRef,
  CheckJNI::DeleteLocalRef,
  CheckJNI::IsSameObject,
  CheckJNI::NewLocalRef,
  CheckJNI::EnsureLocalCapacity,
  CheckJNI::AllocObject,
  CheckJNI::NewObject,
  CheckJNI::NewObjectV,
  CheckJNI::NewObjectA,
  CheckJNI::GetObjectClass,
  CheckJNI::IsInstanceOf,
  CheckJNI::GetMethodID,
  CheckJNI::CallObjectMethod,
  CheckJNI::CallObjectMethodV,
  CheckJNI::CallObjectMethodA,
  CheckJNI::CallBooleanMethod,
  CheckJNI::CallBooleanMethodV,
  CheckJNI::CallBooleanMethodA,
  CheckJNI::CallByteMethod,
  CheckJNI::CallByteMethodV,
  CheckJNI::CallByteMethodA,
  CheckJNI::CallCharMethod,
  CheckJNI::CallCharMethodV,
  CheckJNI::CallCharMethodA,
  CheckJNI::CallShortMethod,
  CheckJNI::CallShortMethodV,
  CheckJNI::CallShortMethodA,
  CheckJNI::CallIntMethod,
  CheckJNI::CallIntMethodV,
  CheckJNI::CallIntMethodA,
  CheckJNI::CallLongMethod,
  CheckJNI::CallLongMethodV,
  CheckJNI::CallLongMethodA,
  CheckJNI::CallFloatMethod,
  CheckJNI::CallFloatMethodV,
  CheckJNI::CallFloatMethodA,
  CheckJNI::CallDoubleMethod,
  CheckJNI::CallDoubleMethodV,
  CheckJNI::CallDoubleMethodA,
  CheckJNI::CallVoidMethod,
  CheckJNI::CallVoidMethodV,
  CheckJNI::CallVoidMethodA,
  CheckJNI::CallNonvirtualObjectMethod,
  CheckJNI::CallNonvirtualObjectMethodV,
  CheckJNI::CallNonvirtualObjectMethodA,
  CheckJNI::CallNonvirtualBooleanMethod,
  CheckJNI::CallNonvirtualBooleanMethodV,
  CheckJNI::CallNonvirtualBooleanMethodA,
  CheckJNI::CallNonvirtualByteMethod,
  CheckJNI::CallNonvirtualByteMethodV,
  CheckJNI::CallNonvirtualByteMethodA,
  CheckJNI::CallNonvirtualCharMethod,
  CheckJNI::CallNonvirtualCharMethodV,
  CheckJNI::CallNonvirtualCharMethodA,
  CheckJNI::CallNonvirtualShortMethod,
  CheckJNI::CallNonvirtualShortMethodV,
  CheckJNI::CallNonvirtualShortMethodA,
  CheckJNI::CallNonvirtualIntMethod,
  CheckJNI::CallNonvirtualIntMethodV,
  CheckJNI::CallNonvirtualIntMethodA,
  CheckJNI::CallNonvirtualLongMethod,
  CheckJNI::CallNonvirtualLongMethodV,
  CheckJNI::CallNonvirtualLongMethodA,
  CheckJNI::CallNonvirtualFloatMethod,
  CheckJNI::CallNonvirtualFloatMethodV,
  CheckJNI::CallNonvirtualFloatMethodA,
  CheckJNI::CallNonvirtualDoubleMethod,
  CheckJNI::CallNonvirtualDoubleMethodV,
  CheckJNI::CallNonvirtualDoubleMethodA,
  CheckJNI::CallNonvirtualVoidMethod,
  CheckJNI::CallNonvirtualVoidMethodV,
  CheckJNI::CallNonvirtualVoidMethodA,
  CheckJNI::GetFieldID,
  CheckJNI::GetObjectField,
  CheckJNI::GetBooleanField,
  CheckJNI::GetByteField,
  CheckJNI::GetCharField,
  CheckJNI::GetShortField,
  CheckJNI::GetIntField,
  CheckJNI::GetLongField,
  CheckJNI::GetFloatField,
  CheckJNI::GetDoubleField,
  CheckJNI::SetObjectField,
  CheckJNI::SetBooleanField,
  CheckJNI::SetByteField,
  CheckJNI::SetCharField,
  CheckJNI::SetShortField,
  CheckJNI::SetIntField,
  CheckJNI::SetLongField,
  CheckJNI::SetFloatField,
  CheckJNI::SetDoubleField,
  CheckJNI::GetStaticMethodID,
  CheckJNI::CallStaticObjectMethod,
  CheckJNI::CallStaticObjectMethodV,
  CheckJNI::CallStaticObjectMethodA,
  CheckJNI::CallStaticBooleanMethod,
  CheckJNI::CallStaticBooleanMethodV,
  CheckJNI::CallStaticBooleanMethodA,
  CheckJNI::CallStaticByteMethod,
  CheckJNI::CallStaticByteMethodV,
  CheckJNI::CallStaticByteMethodA,
  CheckJNI::CallStaticCharMethod,
  CheckJNI::CallStaticCharMethodV,
  CheckJNI::CallStaticCharMethodA,
  CheckJNI::CallStaticShortMethod,
  CheckJNI::CallStaticShortMethodV,
  CheckJNI::CallStaticShortMethodA,
  CheckJNI::CallStaticIntMethod,
  CheckJNI::CallStaticIntMethodV,
  CheckJNI::CallStaticIntMethodA,
  CheckJNI::CallStaticLongMethod,
  CheckJNI::CallStaticLongMethodV,
  CheckJNI::CallStaticLongMethodA,
  CheckJNI::CallStaticFloatMethod,
  CheckJNI::CallStaticFloatMethodV,
  CheckJNI::CallStaticFloatMethodA,
  CheckJNI::CallStaticDoubleMethod,
  CheckJNI::CallStaticDoubleMethodV,
  CheckJNI::CallStaticDoubleMethodA,
  CheckJNI::CallStaticVoidMethod,
  CheckJNI::CallStaticVoidMethodV,
  CheckJNI::CallStaticVoidMethodA,
  CheckJNI::GetStaticFieldID,
  CheckJNI::GetStaticObjectField,
  CheckJNI::GetStaticBooleanField,
  CheckJNI::GetStaticByteField,
  CheckJNI::GetStaticCharField,
  CheckJNI::GetStaticShortField,
  CheckJNI::GetStaticIntField,
  CheckJNI::GetStaticLongField,
  CheckJNI::GetStaticFloatField,
  CheckJNI::GetStaticDoubleField,
  CheckJNI::SetStaticObjectField,
  CheckJNI::SetStaticBooleanField,
  CheckJNI::SetStaticByteField,
  CheckJNI::SetStaticCharField,
  CheckJNI::SetStaticShortField,
  CheckJNI::SetStaticIntField,
  CheckJNI::SetStaticLongField,
  CheckJNI::SetStaticFloatField,
  CheckJNI::SetStaticDoubleField,
  CheckJNI::NewString,
  CheckJNI::GetStringLength,
  CheckJNI::GetStringChars,
  CheckJNI::ReleaseStringChars,
  CheckJNI::NewStringUTF,
  CheckJNI::GetStringUTFLength,
  CheckJNI::GetStringUTFChars,
  CheckJNI::ReleaseStringUTFChars,
  CheckJNI::GetArrayLength,
  CheckJNI::NewObjectArray,
  CheckJNI::GetObjectArrayElement,
  CheckJNI::SetObjectArrayElement,
  CheckJNI::NewBooleanArray,
  CheckJNI::NewByteArray,
  CheckJNI::NewCharArray,
  CheckJNI::NewShortArray,
  CheckJNI::NewIntArray,
  CheckJNI::NewLongArray,
  CheckJNI::NewFloatArray,
  CheckJNI::NewDoubleArray,
  CheckJNI::GetBooleanArrayElements,
  CheckJNI::GetByteArrayElements,
  CheckJNI::GetCharArrayElements,
  CheckJNI::GetShortArrayElements,
  CheckJNI::GetIntArrayElements,
  CheckJNI::GetLongArrayElements,
  CheckJNI::GetFloatArrayElements,
  CheckJNI::GetDoubleArrayElements,
  CheckJNI::ReleaseBooleanArrayElements,
  CheckJNI::ReleaseByteArrayElements,
  CheckJNI::ReleaseCharArrayElements,
  CheckJNI::ReleaseShortArrayElements,
  CheckJNI::ReleaseIntArrayElements,
  CheckJNI::ReleaseLongArrayElements,
  CheckJNI::ReleaseFloatArrayElements,
  CheckJNI::ReleaseDoubleArrayElements,
  CheckJNI::GetBooleanArrayRegion,
  CheckJNI::GetByteArrayRegion,
  CheckJNI::GetCharArrayRegion,
  CheckJNI::GetShortArrayRegion,
  CheckJNI::GetIntArrayRegion,
  CheckJNI::GetLongArrayRegion,
  CheckJNI::GetFloatArrayRegion,
  CheckJNI::GetDoubleArrayRegion,
  CheckJNI::SetBooleanArrayRegion,
  CheckJNI::SetByteArrayRegion,
  CheckJNI::SetCharArrayRegion,
  CheckJNI::SetShortArrayRegion,
  CheckJNI::SetIntArrayRegion,
  CheckJNI::SetLongArrayRegion,
  CheckJNI::SetFloatArrayRegion,
  CheckJNI::SetDoubleArrayRegion,
  CheckJNI::RegisterNatives,
  CheckJNI::UnregisterNatives,
  CheckJNI::MonitorEnter,
  CheckJNI::MonitorExit,
  CheckJNI::GetJavaVM,
  CheckJNI::GetStringRegion,
  CheckJNI::GetStringUTFRegion,
  CheckJNI::GetPrimitiveArrayCritical,
  CheckJNI::ReleasePrimitiveArrayCritical,
  CheckJNI::GetStringCritical,
  CheckJNI::ReleaseStringCritical,
  CheckJNI::NewWeakGlobalRef,
  CheckJNI::DeleteWeakGlobalRef,
  CheckJNI::ExceptionCheck,
  CheckJNI::NewDirectByteBuffer,
  CheckJNI::GetDirectBufferAddress,
  CheckJNI::GetDirectBufferCapacity,
  CheckJNI::GetObjectRefType,
};

class CheckJII {
 public:
  static jint DestroyJavaVM(JavaVM* vm) {
    ScopedCheck sc(kFlag_Invocation, __FUNCTION__, false);
    JniValueType args[1] = {{.v = vm}};
    sc.CheckNonHeap(reinterpret_cast<JavaVMExt*>(vm), true, "v", args);
    JniValueType result;
    result.i = BaseVm(vm)->DestroyJavaVM(vm);
    // Use null to signal that the JavaVM isn't valid anymore. DestroyJavaVM deletes the runtime,
    // which will delete the JavaVMExt.
    sc.CheckNonHeap(nullptr, false, "i", &result);
    return result.i;
  }

  static jint AttachCurrentThread(JavaVM* vm, JNIEnv** p_env, void* thr_args) {
    ScopedCheck sc(kFlag_Invocation, __FUNCTION__);
    JniValueType args[3] = {{.v = vm}, {.p = p_env}, {.p = thr_args}};
    sc.CheckNonHeap(reinterpret_cast<JavaVMExt*>(vm), true, "vpp", args);
    JniValueType result;
    result.i = BaseVm(vm)->AttachCurrentThread(vm, p_env, thr_args);
    sc.CheckNonHeap(reinterpret_cast<JavaVMExt*>(vm), false, "i", &result);
    return result.i;
  }

  static jint AttachCurrentThreadAsDaemon(JavaVM* vm, JNIEnv** p_env, void* thr_args) {
    ScopedCheck sc(kFlag_Invocation, __FUNCTION__);
    JniValueType args[3] = {{.v = vm}, {.p = p_env}, {.p = thr_args}};
    sc.CheckNonHeap(reinterpret_cast<JavaVMExt*>(vm), true, "vpp", args);
    JniValueType result;
    result.i = BaseVm(vm)->AttachCurrentThreadAsDaemon(vm, p_env, thr_args);
    sc.CheckNonHeap(reinterpret_cast<JavaVMExt*>(vm), false, "i", &result);
    return result.i;
  }

  static jint DetachCurrentThread(JavaVM* vm) {
    ScopedCheck sc(kFlag_Invocation, __FUNCTION__);
    JniValueType args[1] = {{.v = vm}};
    sc.CheckNonHeap(reinterpret_cast<JavaVMExt*>(vm), true, "v", args);
    JniValueType result;
    result.i = BaseVm(vm)->DetachCurrentThread(vm);
    sc.CheckNonHeap(reinterpret_cast<JavaVMExt*>(vm), false, "i", &result);
    return result.i;
  }

  static jint GetEnv(JavaVM* vm, void** p_env, jint version) {
    ScopedCheck sc(kFlag_Invocation, __FUNCTION__);
    JniValueType args[3] = {{.v = vm}, {.p = p_env}, {.I = version}};
    sc.CheckNonHeap(reinterpret_cast<JavaVMExt*>(vm), true, "vpI", args);
    JniValueType result;
    result.i = BaseVm(vm)->GetEnv(vm, p_env, version);
    sc.CheckNonHeap(reinterpret_cast<JavaVMExt*>(vm), false, "i", &result);
    return result.i;
  }

 private:
  static const JNIInvokeInterface* BaseVm(JavaVM* vm) {
    return reinterpret_cast<JavaVMExt*>(vm)->GetUncheckedFunctions();
  }
};

const JNIInvokeInterface gCheckInvokeInterface = {
  nullptr,  // reserved0
  nullptr,  // reserved1
  nullptr,  // reserved2
  CheckJII::DestroyJavaVM,
  CheckJII::AttachCurrentThread,
  CheckJII::DetachCurrentThread,
  CheckJII::GetEnv,
  CheckJII::AttachCurrentThreadAsDaemon
};

}  // anonymous namespace

const JNINativeInterface* GetCheckJniNativeInterface() {
  return &gCheckNativeInterface;
}

const JNIInvokeInterface* GetCheckJniInvokeInterface() {
  return &gCheckInvokeInterface;
}

}  // namespace art
