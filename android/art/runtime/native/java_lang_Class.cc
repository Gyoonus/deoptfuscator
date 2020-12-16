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

#include "java_lang_Class.h"

#include <iostream>

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "class_linker-inl.h"
#include "common_throws.h"
#include "dex/descriptors_names.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_annotations.h"
#include "dex/utf.h"
#include "hidden_api.h"
#include "jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/field-inl.h"
#include "mirror/method.h"
#include "mirror/method_handles_lookup.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/string-inl.h"
#include "native_util.h"
#include "nativehelper/jni_macros.h"
#include "nativehelper/scoped_local_ref.h"
#include "nativehelper/scoped_utf_chars.h"
#include "nth_caller_visitor.h"
#include "obj_ptr-inl.h"
#include "reflection.h"
#include "scoped_fast_native_object_access-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "well_known_classes.h"

namespace art {

// Returns true if the first caller outside of the Class class or java.lang.invoke package
// is in a platform DEX file.
static bool IsCallerTrusted(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_) {
  // Walk the stack and find the first frame not from java.lang.Class and not from java.lang.invoke.
  // This is very expensive. Save this till the last.
  struct FirstExternalCallerVisitor : public StackVisitor {
    explicit FirstExternalCallerVisitor(Thread* thread)
        : StackVisitor(thread, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
          caller(nullptr) {
    }

    bool VisitFrame() REQUIRES_SHARED(Locks::mutator_lock_) {
      ArtMethod *m = GetMethod();
      if (m == nullptr) {
        // Attached native thread. Assume this is *not* boot class path.
        caller = nullptr;
        return false;
      } else if (m->IsRuntimeMethod()) {
        // Internal runtime method, continue walking the stack.
        return true;
      }

      ObjPtr<mirror::Class> declaring_class = m->GetDeclaringClass();
      if (declaring_class->IsBootStrapClassLoaded()) {
        if (declaring_class->IsClassClass()) {
          return true;
        }
        // Check classes in the java.lang.invoke package. At the time of writing, the
        // classes of interest are MethodHandles and MethodHandles.Lookup, but this
        // is subject to change so conservatively cover the entire package.
        // NB Static initializers within java.lang.invoke are permitted and do not
        // need further stack inspection.
        ObjPtr<mirror::Class> lookup_class = mirror::MethodHandlesLookup::StaticClass();
        if ((declaring_class == lookup_class || declaring_class->IsInSamePackage(lookup_class))
            && !m->IsClassInitializer()) {
          return true;
        }
      }

      caller = m;
      return false;
    }

    ArtMethod* caller;
  };

  FirstExternalCallerVisitor visitor(self);
  visitor.WalkStack();
  return visitor.caller != nullptr &&
         hiddenapi::IsCallerTrusted(visitor.caller->GetDeclaringClass());
}

// Returns true if the first non-ClassClass caller up the stack is not allowed to
// access hidden APIs. This can be *very* expensive. Never call this in a loop.
ALWAYS_INLINE static bool ShouldEnforceHiddenApi(Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  hiddenapi::EnforcementPolicy policy = Runtime::Current()->GetHiddenApiEnforcementPolicy();
  return policy != hiddenapi::EnforcementPolicy::kNoChecks && !IsCallerTrusted(self);
}

// Returns true if the first non-ClassClass caller up the stack should not be
// allowed access to `member`.
template<typename T>
ALWAYS_INLINE static bool ShouldBlockAccessToMember(T* member, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  hiddenapi::Action action = hiddenapi::GetMemberAction(
      member, self, IsCallerTrusted, hiddenapi::kReflection);
  if (action != hiddenapi::kAllow) {
    hiddenapi::NotifyHiddenApiListener(member);
  }

  return action == hiddenapi::kDeny;
}

// Returns true if a class member should be discoverable with reflection given
// the criteria. Some reflection calls only return public members
// (public_only == true), some members should be hidden from non-boot class path
// callers (enforce_hidden_api == true).
template<typename T>
ALWAYS_INLINE static bool IsDiscoverable(bool public_only,
                                         bool enforce_hidden_api,
                                         T* member)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (public_only && ((member->GetAccessFlags() & kAccPublic) == 0)) {
    return false;
  }

  return hiddenapi::GetMemberAction(member,
                                    nullptr,
                                    [enforce_hidden_api] (Thread*) { return !enforce_hidden_api; },
                                    hiddenapi::kNone)
      != hiddenapi::kDeny;
}

ALWAYS_INLINE static inline ObjPtr<mirror::Class> DecodeClass(
    const ScopedFastNativeObjectAccess& soa, jobject java_class)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> c = soa.Decode<mirror::Class>(java_class);
  DCHECK(c != nullptr);
  DCHECK(c->IsClass());
  // TODO: we could EnsureInitialized here, rather than on every reflective get/set or invoke .
  // For now, we conservatively preserve the old dalvik behavior. A quick "IsInitialized" check
  // every time probably doesn't make much difference to reflection performance anyway.
  return c;
}

// "name" is in "binary name" format, e.g. "dalvik.system.Debug$1".
static jclass Class_classForName(JNIEnv* env, jclass, jstring javaName, jboolean initialize,
                                 jobject javaLoader) {
  ScopedFastNativeObjectAccess soa(env);
  ScopedUtfChars name(env, javaName);
  if (name.c_str() == nullptr) {
    return nullptr;
  }

  // We need to validate and convert the name (from x.y.z to x/y/z).  This
  // is especially handy for array types, since we want to avoid
  // auto-generating bogus array classes.
  if (!IsValidBinaryClassName(name.c_str())) {
    soa.Self()->ThrowNewExceptionF("Ljava/lang/ClassNotFoundException;",
                                   "Invalid name: %s", name.c_str());
    return nullptr;
  }

  std::string descriptor(DotToDescriptor(name.c_str()));
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader(
      hs.NewHandle(soa.Decode<mirror::ClassLoader>(javaLoader)));
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::Class> c(
      hs.NewHandle(class_linker->FindClass(soa.Self(), descriptor.c_str(), class_loader)));
  if (c == nullptr) {
    ScopedLocalRef<jthrowable> cause(env, env->ExceptionOccurred());
    env->ExceptionClear();
    jthrowable cnfe = reinterpret_cast<jthrowable>(
        env->NewObject(WellKnownClasses::java_lang_ClassNotFoundException,
                       WellKnownClasses::java_lang_ClassNotFoundException_init,
                       javaName,
                       cause.get()));
    if (cnfe != nullptr) {
      // Make sure allocation didn't fail with an OOME.
      env->Throw(cnfe);
    }
    return nullptr;
  }
  if (initialize) {
    class_linker->EnsureInitialized(soa.Self(), c, true, true);
  }
  return soa.AddLocalReference<jclass>(c.Get());
}

static jclass Class_getPrimitiveClass(JNIEnv* env, jclass, jstring name) {
  ScopedFastNativeObjectAccess soa(env);
  ObjPtr<mirror::Class> klass = mirror::Class::GetPrimitiveClass(soa.Decode<mirror::String>(name));
  return soa.AddLocalReference<jclass>(klass);
}

static jstring Class_getNameNative(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  ObjPtr<mirror::Class> c = DecodeClass(soa, javaThis);
  return soa.AddLocalReference<jstring>(mirror::Class::ComputeName(hs.NewHandle(c)));
}

// TODO: Move this to mirror::Class ? Other mirror types that commonly appear
// as arrays have a GetArrayClass() method.
static ObjPtr<mirror::Class> GetClassArrayClass(Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> class_class = mirror::Class::GetJavaLangClass();
  return Runtime::Current()->GetClassLinker()->FindArrayClass(self, &class_class);
}

static jobjectArray Class_getInterfacesInternal(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<4> hs(soa.Self());
  Handle<mirror::Class> klass = hs.NewHandle(DecodeClass(soa, javaThis));

  if (klass->IsProxyClass()) {
    return soa.AddLocalReference<jobjectArray>(klass->GetProxyInterfaces()->Clone(soa.Self()));
  }

  const DexFile::TypeList* iface_list = klass->GetInterfaceTypeList();
  if (iface_list == nullptr) {
    return nullptr;
  }

  const uint32_t num_ifaces = iface_list->Size();
  Handle<mirror::Class> class_array_class = hs.NewHandle(GetClassArrayClass(soa.Self()));
  Handle<mirror::ObjectArray<mirror::Class>> ifaces = hs.NewHandle(
      mirror::ObjectArray<mirror::Class>::Alloc(soa.Self(), class_array_class.Get(), num_ifaces));
  if (ifaces.IsNull()) {
    DCHECK(soa.Self()->IsExceptionPending());
    return nullptr;
  }

  // Check that we aren't in an active transaction, we call SetWithoutChecks
  // with kActiveTransaction == false.
  DCHECK(!Runtime::Current()->IsActiveTransaction());

  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  MutableHandle<mirror::Class> interface(hs.NewHandle<mirror::Class>(nullptr));
  for (uint32_t i = 0; i < num_ifaces; ++i) {
    const dex::TypeIndex type_idx = iface_list->GetTypeItem(i).type_idx_;
    interface.Assign(linker->LookupResolvedType(type_idx, klass.Get()));
    ifaces->SetWithoutChecks<false>(i, interface.Get());
  }

  return soa.AddLocalReference<jobjectArray>(ifaces.Get());
}

static mirror::ObjectArray<mirror::Field>* GetDeclaredFields(
    Thread* self, ObjPtr<mirror::Class> klass, bool public_only, bool force_resolve)
      REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<1> hs(self);
  IterationRange<StrideIterator<ArtField>> ifields = klass->GetIFields();
  IterationRange<StrideIterator<ArtField>> sfields = klass->GetSFields();
  size_t array_size = klass->NumInstanceFields() + klass->NumStaticFields();
  bool enforce_hidden_api = ShouldEnforceHiddenApi(self);
  // Lets go subtract all the non discoverable fields.
  for (ArtField& field : ifields) {
    if (!IsDiscoverable(public_only, enforce_hidden_api, &field)) {
      --array_size;
    }
  }
  for (ArtField& field : sfields) {
    if (!IsDiscoverable(public_only, enforce_hidden_api, &field)) {
      --array_size;
    }
  }
  size_t array_idx = 0;
  auto object_array = hs.NewHandle(mirror::ObjectArray<mirror::Field>::Alloc(
      self, mirror::Field::ArrayClass(), array_size));
  if (object_array == nullptr) {
    return nullptr;
  }
  for (ArtField& field : ifields) {
    if (IsDiscoverable(public_only, enforce_hidden_api, &field)) {
      auto* reflect_field = mirror::Field::CreateFromArtField<kRuntimePointerSize>(self,
                                                                                   &field,
                                                                                   force_resolve);
      if (reflect_field == nullptr) {
        if (kIsDebugBuild) {
          self->AssertPendingException();
        }
        // Maybe null due to OOME or type resolving exception.
        return nullptr;
      }
      object_array->SetWithoutChecks<false>(array_idx++, reflect_field);
    }
  }
  for (ArtField& field : sfields) {
    if (IsDiscoverable(public_only, enforce_hidden_api, &field)) {
      auto* reflect_field = mirror::Field::CreateFromArtField<kRuntimePointerSize>(self,
                                                                                   &field,
                                                                                   force_resolve);
      if (reflect_field == nullptr) {
        if (kIsDebugBuild) {
          self->AssertPendingException();
        }
        return nullptr;
      }
      object_array->SetWithoutChecks<false>(array_idx++, reflect_field);
    }
  }
  DCHECK_EQ(array_idx, array_size);
  return object_array.Get();
}

static jobjectArray Class_getDeclaredFieldsUnchecked(JNIEnv* env, jobject javaThis,
                                                     jboolean publicOnly) {
  ScopedFastNativeObjectAccess soa(env);
  return soa.AddLocalReference<jobjectArray>(
      GetDeclaredFields(soa.Self(), DecodeClass(soa, javaThis), publicOnly != JNI_FALSE, false));
}

static jobjectArray Class_getDeclaredFields(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  return soa.AddLocalReference<jobjectArray>(
      GetDeclaredFields(soa.Self(), DecodeClass(soa, javaThis), false, true));
}

static jobjectArray Class_getPublicDeclaredFields(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  return soa.AddLocalReference<jobjectArray>(
      GetDeclaredFields(soa.Self(), DecodeClass(soa, javaThis), true, true));
}

// Performs a binary search through an array of fields, TODO: Is this fast enough if we don't use
// the dex cache for lookups? I think CompareModifiedUtf8ToUtf16AsCodePointValues should be fairly
// fast.
ALWAYS_INLINE static inline ArtField* FindFieldByName(ObjPtr<mirror::String> name,
                                                      LengthPrefixedArray<ArtField>* fields)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (fields == nullptr) {
    return nullptr;
  }
  size_t low = 0;
  size_t high = fields->size();
  const bool is_name_compressed = name->IsCompressed();
  const uint16_t* const data = (is_name_compressed) ? nullptr : name->GetValue();
  const uint8_t* const data_compressed = (is_name_compressed) ? name->GetValueCompressed()
                                                              : nullptr;
  const size_t length = name->GetLength();
  while (low < high) {
    auto mid = (low + high) / 2;
    ArtField& field = fields->At(mid);
    int result = 0;
    if (is_name_compressed) {
      size_t field_length = strlen(field.GetName());
      size_t min_size = (length < field_length) ? length : field_length;
      result = memcmp(field.GetName(), data_compressed, min_size);
      if (result == 0) {
        result = field_length - length;
      }
    } else {
      result = CompareModifiedUtf8ToUtf16AsCodePointValues(field.GetName(), data, length);
    }
    // Alternate approach, only a few % faster at the cost of more allocations.
    // int result = field->GetStringName(self, true)->CompareTo(name);
    if (result < 0) {
      low = mid + 1;
    } else if (result > 0) {
      high = mid;
    } else {
      return &field;
    }
  }
  if (kIsDebugBuild) {
    for (ArtField& field : MakeIterationRangeFromLengthPrefixedArray(fields)) {
      CHECK_NE(field.GetName(), name->ToModifiedUtf8());
    }
  }
  return nullptr;
}

ALWAYS_INLINE static inline mirror::Field* GetDeclaredField(Thread* self,
                                                            ObjPtr<mirror::Class> c,
                                                            ObjPtr<mirror::String> name)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtField* art_field = FindFieldByName(name, c->GetIFieldsPtr());
  if (art_field != nullptr) {
    return mirror::Field::CreateFromArtField<kRuntimePointerSize>(self, art_field, true);
  }
  art_field = FindFieldByName(name, c->GetSFieldsPtr());
  if (art_field != nullptr) {
    return mirror::Field::CreateFromArtField<kRuntimePointerSize>(self, art_field, true);
  }
  return nullptr;
}

static mirror::Field* GetPublicFieldRecursive(
    Thread* self, ObjPtr<mirror::Class> clazz, ObjPtr<mirror::String> name)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(clazz != nullptr);
  DCHECK(name != nullptr);
  DCHECK(self != nullptr);

  StackHandleScope<2> hs(self);
  MutableHandle<mirror::Class> h_clazz(hs.NewHandle(clazz));
  Handle<mirror::String> h_name(hs.NewHandle(name));

  // We search the current class, its direct interfaces then its superclass.
  while (h_clazz != nullptr) {
    mirror::Field* result = GetDeclaredField(self, h_clazz.Get(), h_name.Get());
    if ((result != nullptr) && (result->GetAccessFlags() & kAccPublic)) {
      return result;
    } else if (UNLIKELY(self->IsExceptionPending())) {
      // Something went wrong. Bail out.
      return nullptr;
    }

    uint32_t num_direct_interfaces = h_clazz->NumDirectInterfaces();
    for (uint32_t i = 0; i < num_direct_interfaces; i++) {
      ObjPtr<mirror::Class> iface = mirror::Class::ResolveDirectInterface(self, h_clazz, i);
      if (UNLIKELY(iface == nullptr)) {
        self->AssertPendingException();
        return nullptr;
      }
      result = GetPublicFieldRecursive(self, iface, h_name.Get());
      if (result != nullptr) {
        DCHECK(result->GetAccessFlags() & kAccPublic);
        return result;
      } else if (UNLIKELY(self->IsExceptionPending())) {
        // Something went wrong. Bail out.
        return nullptr;
      }
    }

    // We don't try the superclass if we are an interface.
    if (h_clazz->IsInterface()) {
      break;
    }

    // Get the next class.
    h_clazz.Assign(h_clazz->GetSuperClass());
  }
  return nullptr;
}

static jobject Class_getPublicFieldRecursive(JNIEnv* env, jobject javaThis, jstring name) {
  ScopedFastNativeObjectAccess soa(env);
  auto name_string = soa.Decode<mirror::String>(name);
  if (UNLIKELY(name_string == nullptr)) {
    ThrowNullPointerException("name == null");
    return nullptr;
  }

  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Field> field = hs.NewHandle(GetPublicFieldRecursive(
      soa.Self(), DecodeClass(soa, javaThis), name_string));
  if (field.Get() == nullptr ||
      ShouldBlockAccessToMember(field->GetArtField(), soa.Self())) {
    return nullptr;
  }
  return soa.AddLocalReference<jobject>(field.Get());
}

static jobject Class_getDeclaredField(JNIEnv* env, jobject javaThis, jstring name) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<3> hs(soa.Self());
  Handle<mirror::String> h_string = hs.NewHandle(soa.Decode<mirror::String>(name));
  if (h_string == nullptr) {
    ThrowNullPointerException("name == null");
    return nullptr;
  }
  Handle<mirror::Class> h_klass = hs.NewHandle(DecodeClass(soa, javaThis));
  Handle<mirror::Field> result =
      hs.NewHandle(GetDeclaredField(soa.Self(), h_klass.Get(), h_string.Get()));
  if (result == nullptr || ShouldBlockAccessToMember(result->GetArtField(), soa.Self())) {
    std::string name_str = h_string->ToModifiedUtf8();
    if (name_str == "value" && h_klass->IsStringClass()) {
      // We log the error for this specific case, as the user might just swallow the exception.
      // This helps diagnose crashes when applications rely on the String#value field being
      // there.
      // Also print on the error stream to test it through run-test.
      std::string message("The String#value field is not present on Android versions >= 6.0");
      LOG(ERROR) << message;
      std::cerr << message << std::endl;
    }
    // We may have a pending exception if we failed to resolve.
    if (!soa.Self()->IsExceptionPending()) {
      ThrowNoSuchFieldException(h_klass.Get(), name_str.c_str());
    }
    return nullptr;
  }
  return soa.AddLocalReference<jobject>(result.Get());
}

static jobject Class_getDeclaredConstructorInternal(
    JNIEnv* env, jobject javaThis, jobjectArray args) {
  ScopedFastNativeObjectAccess soa(env);
  DCHECK_EQ(Runtime::Current()->GetClassLinker()->GetImagePointerSize(), kRuntimePointerSize);
  DCHECK(!Runtime::Current()->IsActiveTransaction());

  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Constructor> result = hs.NewHandle(
      mirror::Class::GetDeclaredConstructorInternal<kRuntimePointerSize, false>(
      soa.Self(),
      DecodeClass(soa, javaThis),
      soa.Decode<mirror::ObjectArray<mirror::Class>>(args)));
  if (result == nullptr || ShouldBlockAccessToMember(result->GetArtMethod(), soa.Self())) {
    return nullptr;
  }
  return soa.AddLocalReference<jobject>(result.Get());
}

static ALWAYS_INLINE inline bool MethodMatchesConstructor(
    ArtMethod* m, bool public_only, bool enforce_hidden_api)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(m != nullptr);
  return m->IsConstructor() &&
         !m->IsStatic() &&
         IsDiscoverable(public_only, enforce_hidden_api, m);
}

static jobjectArray Class_getDeclaredConstructorsInternal(
    JNIEnv* env, jobject javaThis, jboolean publicOnly) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<2> hs(soa.Self());
  bool public_only = (publicOnly != JNI_FALSE);
  bool enforce_hidden_api = ShouldEnforceHiddenApi(soa.Self());
  Handle<mirror::Class> h_klass = hs.NewHandle(DecodeClass(soa, javaThis));
  size_t constructor_count = 0;
  // Two pass approach for speed.
  for (auto& m : h_klass->GetDirectMethods(kRuntimePointerSize)) {
    constructor_count += MethodMatchesConstructor(&m, public_only, enforce_hidden_api) ? 1u : 0u;
  }
  auto h_constructors = hs.NewHandle(mirror::ObjectArray<mirror::Constructor>::Alloc(
      soa.Self(), mirror::Constructor::ArrayClass(), constructor_count));
  if (UNLIKELY(h_constructors == nullptr)) {
    soa.Self()->AssertPendingException();
    return nullptr;
  }
  constructor_count = 0;
  for (auto& m : h_klass->GetDirectMethods(kRuntimePointerSize)) {
    if (MethodMatchesConstructor(&m, public_only, enforce_hidden_api)) {
      DCHECK_EQ(Runtime::Current()->GetClassLinker()->GetImagePointerSize(), kRuntimePointerSize);
      DCHECK(!Runtime::Current()->IsActiveTransaction());
      auto* constructor = mirror::Constructor::CreateFromArtMethod<kRuntimePointerSize, false>(
          soa.Self(), &m);
      if (UNLIKELY(constructor == nullptr)) {
        soa.Self()->AssertPendingOOMException();
        return nullptr;
      }
      h_constructors->SetWithoutChecks<false>(constructor_count++, constructor);
    }
  }
  return soa.AddLocalReference<jobjectArray>(h_constructors.Get());
}

static jobject Class_getDeclaredMethodInternal(JNIEnv* env, jobject javaThis,
                                               jstring name, jobjectArray args) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  DCHECK_EQ(Runtime::Current()->GetClassLinker()->GetImagePointerSize(), kRuntimePointerSize);
  DCHECK(!Runtime::Current()->IsActiveTransaction());
  Handle<mirror::Method> result = hs.NewHandle(
      mirror::Class::GetDeclaredMethodInternal<kRuntimePointerSize, false>(
          soa.Self(),
          DecodeClass(soa, javaThis),
          soa.Decode<mirror::String>(name),
          soa.Decode<mirror::ObjectArray<mirror::Class>>(args)));
  if (result == nullptr || ShouldBlockAccessToMember(result->GetArtMethod(), soa.Self())) {
    return nullptr;
  }
  return soa.AddLocalReference<jobject>(result.Get());
}

static jobjectArray Class_getDeclaredMethodsUnchecked(JNIEnv* env, jobject javaThis,
                                                      jboolean publicOnly) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<2> hs(soa.Self());

  bool enforce_hidden_api = ShouldEnforceHiddenApi(soa.Self());
  bool public_only = (publicOnly != JNI_FALSE);

  Handle<mirror::Class> klass = hs.NewHandle(DecodeClass(soa, javaThis));
  size_t num_methods = 0;
  for (ArtMethod& m : klass->GetDeclaredMethods(kRuntimePointerSize)) {
    uint32_t modifiers = m.GetAccessFlags();
    // Add non-constructor declared methods.
    if ((modifiers & kAccConstructor) == 0 &&
        IsDiscoverable(public_only, enforce_hidden_api, &m)) {
      ++num_methods;
    }
  }
  auto ret = hs.NewHandle(mirror::ObjectArray<mirror::Method>::Alloc(
      soa.Self(), mirror::Method::ArrayClass(), num_methods));
  if (ret == nullptr) {
    soa.Self()->AssertPendingOOMException();
    return nullptr;
  }
  num_methods = 0;
  for (ArtMethod& m : klass->GetDeclaredMethods(kRuntimePointerSize)) {
    uint32_t modifiers = m.GetAccessFlags();
    if ((modifiers & kAccConstructor) == 0 &&
        IsDiscoverable(public_only, enforce_hidden_api, &m)) {
      DCHECK_EQ(Runtime::Current()->GetClassLinker()->GetImagePointerSize(), kRuntimePointerSize);
      DCHECK(!Runtime::Current()->IsActiveTransaction());
      auto* method =
          mirror::Method::CreateFromArtMethod<kRuntimePointerSize, false>(soa.Self(), &m);
      if (method == nullptr) {
        soa.Self()->AssertPendingException();
        return nullptr;
      }
      ret->SetWithoutChecks<false>(num_methods++, method);
    }
  }
  return soa.AddLocalReference<jobjectArray>(ret.Get());
}

static jobject Class_getDeclaredAnnotation(JNIEnv* env, jobject javaThis, jclass annotationClass) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));

  // Handle public contract to throw NPE if the "annotationClass" argument was null.
  if (UNLIKELY(annotationClass == nullptr)) {
    ThrowNullPointerException("annotationClass");
    return nullptr;
  }

  if (klass->IsProxyClass() || klass->GetDexCache() == nullptr) {
    return nullptr;
  }
  Handle<mirror::Class> annotation_class(hs.NewHandle(soa.Decode<mirror::Class>(annotationClass)));
  return soa.AddLocalReference<jobject>(
      annotations::GetAnnotationForClass(klass, annotation_class));
}

static jobjectArray Class_getDeclaredAnnotations(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));
  if (klass->IsProxyClass() || klass->GetDexCache() == nullptr) {
    // Return an empty array instead of a null pointer.
    ObjPtr<mirror::Class>  annotation_array_class =
        soa.Decode<mirror::Class>(WellKnownClasses::java_lang_annotation_Annotation__array);
    mirror::ObjectArray<mirror::Object>* empty_array =
        mirror::ObjectArray<mirror::Object>::Alloc(soa.Self(),
                                                   annotation_array_class.Ptr(),
                                                   0);
    return soa.AddLocalReference<jobjectArray>(empty_array);
  }
  return soa.AddLocalReference<jobjectArray>(annotations::GetAnnotationsForClass(klass));
}

static jobjectArray Class_getDeclaredClasses(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));
  mirror::ObjectArray<mirror::Class>* classes = nullptr;
  if (!klass->IsProxyClass() && klass->GetDexCache() != nullptr) {
    classes = annotations::GetDeclaredClasses(klass);
  }
  if (classes == nullptr) {
    // Return an empty array instead of a null pointer.
    if (soa.Self()->IsExceptionPending()) {
      // Pending exception from GetDeclaredClasses.
      return nullptr;
    }
    ObjPtr<mirror::Class> class_array_class = GetClassArrayClass(soa.Self());
    if (class_array_class == nullptr) {
      return nullptr;
    }
    ObjPtr<mirror::ObjectArray<mirror::Class>> empty_array =
        mirror::ObjectArray<mirror::Class>::Alloc(soa.Self(), class_array_class, 0);
    return soa.AddLocalReference<jobjectArray>(empty_array);
  }
  return soa.AddLocalReference<jobjectArray>(classes);
}

static jclass Class_getEnclosingClass(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));
  if (klass->IsProxyClass() || klass->GetDexCache() == nullptr) {
    return nullptr;
  }
  return soa.AddLocalReference<jclass>(annotations::GetEnclosingClass(klass));
}

static jobject Class_getEnclosingConstructorNative(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));
  if (klass->IsProxyClass() || klass->GetDexCache() == nullptr) {
    return nullptr;
  }
  ObjPtr<mirror::Object> method = annotations::GetEnclosingMethod(klass);
  if (method != nullptr) {
    if (soa.Decode<mirror::Class>(WellKnownClasses::java_lang_reflect_Constructor) ==
        method->GetClass()) {
      return soa.AddLocalReference<jobject>(method);
    }
  }
  return nullptr;
}

static jobject Class_getEnclosingMethodNative(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));
  if (klass->IsProxyClass() || klass->GetDexCache() == nullptr) {
    return nullptr;
  }
  ObjPtr<mirror::Object> method = annotations::GetEnclosingMethod(klass);
  if (method != nullptr) {
    if (soa.Decode<mirror::Class>(WellKnownClasses::java_lang_reflect_Method) ==
        method->GetClass()) {
      return soa.AddLocalReference<jobject>(method);
    }
  }
  return nullptr;
}

static jint Class_getInnerClassFlags(JNIEnv* env, jobject javaThis, jint defaultValue) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));
  return mirror::Class::GetInnerClassFlags(klass, defaultValue);
}

static jstring Class_getInnerClassName(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));
  if (klass->IsProxyClass() || klass->GetDexCache() == nullptr) {
    return nullptr;
  }
  mirror::String* class_name = nullptr;
  if (!annotations::GetInnerClass(klass, &class_name)) {
    return nullptr;
  }
  return soa.AddLocalReference<jstring>(class_name);
}

static jobjectArray Class_getSignatureAnnotation(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));
  if (klass->IsProxyClass() || klass->GetDexCache() == nullptr) {
    return nullptr;
  }
  return soa.AddLocalReference<jobjectArray>(
      annotations::GetSignatureAnnotationForClass(klass));
}

static jboolean Class_isAnonymousClass(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));
  if (klass->IsProxyClass() || klass->GetDexCache() == nullptr) {
    return false;
  }
  mirror::String* class_name = nullptr;
  if (!annotations::GetInnerClass(klass, &class_name)) {
    return false;
  }
  return class_name == nullptr;
}

static jboolean Class_isDeclaredAnnotationPresent(JNIEnv* env, jobject javaThis,
                                                  jclass annotationType) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));
  if (klass->IsProxyClass() || klass->GetDexCache() == nullptr) {
    return false;
  }
  Handle<mirror::Class> annotation_class(hs.NewHandle(soa.Decode<mirror::Class>(annotationType)));
  return annotations::IsClassAnnotationPresent(klass, annotation_class);
}

static jclass Class_getDeclaringClass(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::Class> klass(hs.NewHandle(DecodeClass(soa, javaThis)));
  if (klass->IsProxyClass() || klass->GetDexCache() == nullptr) {
    return nullptr;
  }
  // Return null for anonymous classes.
  if (Class_isAnonymousClass(env, javaThis)) {
    return nullptr;
  }
  return soa.AddLocalReference<jclass>(annotations::GetDeclaringClass(klass));
}

static jobject Class_newInstance(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<4> hs(soa.Self());
  Handle<mirror::Class> klass = hs.NewHandle(DecodeClass(soa, javaThis));
  if (UNLIKELY(klass->GetPrimitiveType() != 0 || klass->IsInterface() || klass->IsArrayClass() ||
               klass->IsAbstract())) {
    soa.Self()->ThrowNewExceptionF("Ljava/lang/InstantiationException;",
                                   "%s cannot be instantiated",
                                   klass->PrettyClass().c_str());
    return nullptr;
  }
  auto caller = hs.NewHandle<mirror::Class>(nullptr);
  // Verify that we can access the class.
  if (!klass->IsPublic()) {
    caller.Assign(GetCallingClass(soa.Self(), 1));
    if (caller != nullptr && !caller->CanAccess(klass.Get())) {
      soa.Self()->ThrowNewExceptionF(
          "Ljava/lang/IllegalAccessException;", "%s is not accessible from %s",
          klass->PrettyClass().c_str(), caller->PrettyClass().c_str());
      return nullptr;
    }
  }
  ArtMethod* constructor = klass->GetDeclaredConstructor(
      soa.Self(),
      ScopedNullHandle<mirror::ObjectArray<mirror::Class>>(),
      kRuntimePointerSize);
  if (UNLIKELY(constructor == nullptr) || ShouldBlockAccessToMember(constructor, soa.Self())) {
    soa.Self()->ThrowNewExceptionF("Ljava/lang/InstantiationException;",
                                   "%s has no zero argument constructor",
                                   klass->PrettyClass().c_str());
    return nullptr;
  }
  // Invoke the string allocator to return an empty string for the string class.
  if (klass->IsStringClass()) {
    gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
    ObjPtr<mirror::Object> obj = mirror::String::AllocEmptyString<true>(soa.Self(), allocator_type);
    if (UNLIKELY(soa.Self()->IsExceptionPending())) {
      return nullptr;
    } else {
      return soa.AddLocalReference<jobject>(obj);
    }
  }
  auto receiver = hs.NewHandle(klass->AllocObject(soa.Self()));
  if (UNLIKELY(receiver == nullptr)) {
    soa.Self()->AssertPendingOOMException();
    return nullptr;
  }
  // Verify that we can access the constructor.
  auto* declaring_class = constructor->GetDeclaringClass();
  if (!constructor->IsPublic()) {
    if (caller == nullptr) {
      caller.Assign(GetCallingClass(soa.Self(), 1));
    }
    if (UNLIKELY(caller != nullptr && !VerifyAccess(receiver.Get(),
                                                          declaring_class,
                                                          constructor->GetAccessFlags(),
                                                          caller.Get()))) {
      soa.Self()->ThrowNewExceptionF(
          "Ljava/lang/IllegalAccessException;", "%s is not accessible from %s",
          constructor->PrettyMethod().c_str(), caller->PrettyClass().c_str());
      return nullptr;
    }
  }
  // Ensure that we are initialized.
  if (UNLIKELY(!declaring_class->IsInitialized())) {
    if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(
        soa.Self(), hs.NewHandle(declaring_class), true, true)) {
      soa.Self()->AssertPendingException();
      return nullptr;
    }
  }
  // Invoke the constructor.
  JValue result;
  uint32_t args[1] = { static_cast<uint32_t>(reinterpret_cast<uintptr_t>(receiver.Get())) };
  constructor->Invoke(soa.Self(), args, sizeof(args), &result, "V");
  if (UNLIKELY(soa.Self()->IsExceptionPending())) {
    return nullptr;
  }
  // Constructors are ()V methods, so we shouldn't touch the result of InvokeMethod.
  return soa.AddLocalReference<jobject>(receiver.Get());
}

static JNINativeMethod gMethods[] = {
  FAST_NATIVE_METHOD(Class, classForName,
                "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;"),
  FAST_NATIVE_METHOD(Class, getDeclaredAnnotation,
                "(Ljava/lang/Class;)Ljava/lang/annotation/Annotation;"),
  FAST_NATIVE_METHOD(Class, getDeclaredAnnotations, "()[Ljava/lang/annotation/Annotation;"),
  FAST_NATIVE_METHOD(Class, getDeclaredClasses, "()[Ljava/lang/Class;"),
  FAST_NATIVE_METHOD(Class, getDeclaredConstructorInternal,
                "([Ljava/lang/Class;)Ljava/lang/reflect/Constructor;"),
  FAST_NATIVE_METHOD(Class, getDeclaredConstructorsInternal, "(Z)[Ljava/lang/reflect/Constructor;"),
  FAST_NATIVE_METHOD(Class, getDeclaredField, "(Ljava/lang/String;)Ljava/lang/reflect/Field;"),
  FAST_NATIVE_METHOD(Class, getPublicFieldRecursive, "(Ljava/lang/String;)Ljava/lang/reflect/Field;"),
  FAST_NATIVE_METHOD(Class, getDeclaredFields, "()[Ljava/lang/reflect/Field;"),
  FAST_NATIVE_METHOD(Class, getDeclaredFieldsUnchecked, "(Z)[Ljava/lang/reflect/Field;"),
  FAST_NATIVE_METHOD(Class, getDeclaredMethodInternal,
                "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;"),
  FAST_NATIVE_METHOD(Class, getDeclaredMethodsUnchecked,
                "(Z)[Ljava/lang/reflect/Method;"),
  FAST_NATIVE_METHOD(Class, getDeclaringClass, "()Ljava/lang/Class;"),
  FAST_NATIVE_METHOD(Class, getEnclosingClass, "()Ljava/lang/Class;"),
  FAST_NATIVE_METHOD(Class, getEnclosingConstructorNative, "()Ljava/lang/reflect/Constructor;"),
  FAST_NATIVE_METHOD(Class, getEnclosingMethodNative, "()Ljava/lang/reflect/Method;"),
  FAST_NATIVE_METHOD(Class, getInnerClassFlags, "(I)I"),
  FAST_NATIVE_METHOD(Class, getInnerClassName, "()Ljava/lang/String;"),
  FAST_NATIVE_METHOD(Class, getInterfacesInternal, "()[Ljava/lang/Class;"),
  FAST_NATIVE_METHOD(Class, getPrimitiveClass, "(Ljava/lang/String;)Ljava/lang/Class;"),
  FAST_NATIVE_METHOD(Class, getNameNative, "()Ljava/lang/String;"),
  FAST_NATIVE_METHOD(Class, getPublicDeclaredFields, "()[Ljava/lang/reflect/Field;"),
  FAST_NATIVE_METHOD(Class, getSignatureAnnotation, "()[Ljava/lang/String;"),
  FAST_NATIVE_METHOD(Class, isAnonymousClass, "()Z"),
  FAST_NATIVE_METHOD(Class, isDeclaredAnnotationPresent, "(Ljava/lang/Class;)Z"),
  FAST_NATIVE_METHOD(Class, newInstance, "()Ljava/lang/Object;"),
};

void register_java_lang_Class(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/Class");
}

}  // namespace art
