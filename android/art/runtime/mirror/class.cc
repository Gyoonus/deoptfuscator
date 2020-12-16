/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "class.h"

#include "android-base/stringprintf.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/logging.h"  // For VLOG.
#include "base/utils.h"
#include "class-inl.h"
#include "class_ext.h"
#include "class_linker-inl.h"
#include "class_loader.h"
#include "dex/descriptors_names.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_annotations.h"
#include "dex_cache.h"
#include "gc/accounting/card_table-inl.h"
#include "handle_scope-inl.h"
#include "subtype_check.h"
#include "method.h"
#include "object-inl.h"
#include "object-refvisitor-inl.h"
#include "object_array-inl.h"
#include "object_lock.h"
#include "runtime.h"
#include "thread.h"
#include "throwable.h"
#include "well_known_classes.h"

namespace art {

// TODO: move to own CC file?
constexpr size_t BitString::kBitSizeAtPosition[BitString::kCapacity];
constexpr size_t BitString::kCapacity;

namespace mirror {

using android::base::StringPrintf;

GcRoot<Class> Class::java_lang_Class_;

void Class::SetClassClass(ObjPtr<Class> java_lang_Class) {
  CHECK(java_lang_Class_.IsNull())
      << java_lang_Class_.Read()
      << " " << java_lang_Class;
  CHECK(java_lang_Class != nullptr);
  java_lang_Class->SetClassFlags(kClassFlagClass);
  java_lang_Class_ = GcRoot<Class>(java_lang_Class);
}

void Class::ResetClass() {
  CHECK(!java_lang_Class_.IsNull());
  java_lang_Class_ = GcRoot<Class>(nullptr);
}

void Class::VisitRoots(RootVisitor* visitor) {
  java_lang_Class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

ObjPtr<mirror::Class> Class::GetPrimitiveClass(ObjPtr<mirror::String> name) {
  const char* expected_name = nullptr;
  ClassLinker::ClassRoot class_root = ClassLinker::kJavaLangObject;  // Invalid.
  if (name != nullptr && name->GetLength() >= 2) {
    // Perfect hash for the expected values: from the second letters of the primitive types,
    // only 'y' has the bit 0x10 set, so use it to change 'b' to 'B'.
    char hash = name->CharAt(0) ^ ((name->CharAt(1) & 0x10) << 1);
    switch (hash) {
      case 'b': expected_name = "boolean"; class_root = ClassLinker::kPrimitiveBoolean; break;
      case 'B': expected_name = "byte";    class_root = ClassLinker::kPrimitiveByte;    break;
      case 'c': expected_name = "char";    class_root = ClassLinker::kPrimitiveChar;    break;
      case 'd': expected_name = "double";  class_root = ClassLinker::kPrimitiveDouble;  break;
      case 'f': expected_name = "float";   class_root = ClassLinker::kPrimitiveFloat;   break;
      case 'i': expected_name = "int";     class_root = ClassLinker::kPrimitiveInt;     break;
      case 'l': expected_name = "long";    class_root = ClassLinker::kPrimitiveLong;    break;
      case 's': expected_name = "short";   class_root = ClassLinker::kPrimitiveShort;   break;
      case 'v': expected_name = "void";    class_root = ClassLinker::kPrimitiveVoid;    break;
      default: break;
    }
  }
  if (expected_name != nullptr && name->Equals(expected_name)) {
    ObjPtr<mirror::Class> klass = Runtime::Current()->GetClassLinker()->GetClassRoot(class_root);
    DCHECK(klass != nullptr);
    return klass;
  } else {
    Thread* self = Thread::Current();
    if (name == nullptr) {
      // Note: ThrowNullPointerException() requires a message which we deliberately want to omit.
      self->ThrowNewException("Ljava/lang/NullPointerException;", /* msg */ nullptr);
    } else {
      self->ThrowNewException("Ljava/lang/ClassNotFoundException;", name->ToModifiedUtf8().c_str());
    }
    return nullptr;
  }
}

ClassExt* Class::EnsureExtDataPresent(Thread* self) {
  ObjPtr<ClassExt> existing(GetExtData());
  if (!existing.IsNull()) {
    return existing.Ptr();
  }
  StackHandleScope<3> hs(self);
  // Handlerize 'this' since we are allocating here.
  Handle<Class> h_this(hs.NewHandle(this));
  // Clear exception so we can allocate.
  Handle<Throwable> throwable(hs.NewHandle(self->GetException()));
  self->ClearException();
  // Allocate the ClassExt
  Handle<ClassExt> new_ext(hs.NewHandle(ClassExt::Alloc(self)));
  if (new_ext == nullptr) {
    // OOM allocating the classExt.
    // TODO Should we restore the suppressed exception?
    self->AssertPendingOOMException();
    return nullptr;
  } else {
    MemberOffset ext_offset(OFFSET_OF_OBJECT_MEMBER(Class, ext_data_));
    bool set;
    // Set the ext_data_ field using CAS semantics.
    if (Runtime::Current()->IsActiveTransaction()) {
      set = h_this->CasFieldStrongSequentiallyConsistentObject<true>(ext_offset,
                                                                     ObjPtr<ClassExt>(nullptr),
                                                                     new_ext.Get());
    } else {
      set = h_this->CasFieldStrongSequentiallyConsistentObject<false>(ext_offset,
                                                                      ObjPtr<ClassExt>(nullptr),
                                                                      new_ext.Get());
    }
    ObjPtr<ClassExt> ret(set ? new_ext.Get() : h_this->GetExtData());
    DCHECK(!set || h_this->GetExtData() == new_ext.Get());
    CHECK(!ret.IsNull());
    // Restore the exception if there was one.
    if (throwable != nullptr) {
      self->SetException(throwable.Get());
    }
    return ret.Ptr();
  }
}

void Class::SetStatus(Handle<Class> h_this, ClassStatus new_status, Thread* self) {
  ClassStatus old_status = h_this->GetStatus();
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  bool class_linker_initialized = class_linker != nullptr && class_linker->IsInitialized();
  if (LIKELY(class_linker_initialized)) {
    if (UNLIKELY(new_status <= old_status &&
                 new_status != ClassStatus::kErrorUnresolved &&
                 new_status != ClassStatus::kErrorResolved &&
                 new_status != ClassStatus::kRetired)) {
      LOG(FATAL) << "Unexpected change back of class status for " << h_this->PrettyClass()
                 << " " << old_status << " -> " << new_status;
    }
    if (new_status >= ClassStatus::kResolved || old_status >= ClassStatus::kResolved) {
      // When classes are being resolved the resolution code should hold the lock.
      CHECK_EQ(h_this->GetLockOwnerThreadId(), self->GetThreadId())
            << "Attempt to change status of class while not holding its lock: "
            << h_this->PrettyClass() << " " << old_status << " -> " << new_status;
    }
  }
  if (UNLIKELY(IsErroneous(new_status))) {
    CHECK(!h_this->IsErroneous())
        << "Attempt to set as erroneous an already erroneous class "
        << h_this->PrettyClass()
        << " old_status: " << old_status << " new_status: " << new_status;
    CHECK_EQ(new_status == ClassStatus::kErrorResolved, old_status >= ClassStatus::kResolved);
    if (VLOG_IS_ON(class_linker)) {
      LOG(ERROR) << "Setting " << h_this->PrettyDescriptor() << " to erroneous.";
      if (self->IsExceptionPending()) {
        LOG(ERROR) << "Exception: " << self->GetException()->Dump();
      }
    }

    ObjPtr<ClassExt> ext(h_this->EnsureExtDataPresent(self));
    if (!ext.IsNull()) {
      self->AssertPendingException();
      ext->SetVerifyError(self->GetException());
    } else {
      self->AssertPendingOOMException();
    }
    self->AssertPendingException();
  }

  if (kBitstringSubtypeCheckEnabled) {
    // FIXME: This looks broken with respect to aborted transactions.
    ObjPtr<mirror::Class> h_this_ptr = h_this.Get();
    SubtypeCheck<ObjPtr<mirror::Class>>::WriteStatus(h_this_ptr, new_status);
  } else {
    // The ClassStatus is always in the 4 most-significant bits of status_.
    static_assert(sizeof(status_) == sizeof(uint32_t), "Size of status_ not equal to uint32");
    uint32_t new_status_value = static_cast<uint32_t>(new_status) << (32 - kClassStatusBitSize);
    if (Runtime::Current()->IsActiveTransaction()) {
      h_this->SetField32Volatile<true>(StatusOffset(), new_status_value);
    } else {
      h_this->SetField32Volatile<false>(StatusOffset(), new_status_value);
    }
  }

  // Setting the object size alloc fast path needs to be after the status write so that if the
  // alloc path sees a valid object size, we would know that it's initialized as long as it has a
  // load-acquire/fake dependency.
  if (new_status == ClassStatus::kInitialized && !h_this->IsVariableSize()) {
    DCHECK_EQ(h_this->GetObjectSizeAllocFastPath(), std::numeric_limits<uint32_t>::max());
    // Finalizable objects must always go slow path.
    if (!h_this->IsFinalizable()) {
      h_this->SetObjectSizeAllocFastPath(RoundUp(h_this->GetObjectSize(), kObjectAlignment));
    }
  }

  if (!class_linker_initialized) {
    // When the class linker is being initialized its single threaded and by definition there can be
    // no waiters. During initialization classes may appear temporary but won't be retired as their
    // size was statically computed.
  } else {
    // Classes that are being resolved or initialized need to notify waiters that the class status
    // changed. See ClassLinker::EnsureResolved and ClassLinker::WaitForInitializeClass.
    if (h_this->IsTemp()) {
      // Class is a temporary one, ensure that waiters for resolution get notified of retirement
      // so that they can grab the new version of the class from the class linker's table.
      CHECK_LT(new_status, ClassStatus::kResolved) << h_this->PrettyDescriptor();
      if (new_status == ClassStatus::kRetired || new_status == ClassStatus::kErrorUnresolved) {
        h_this->NotifyAll(self);
      }
    } else {
      CHECK_NE(new_status, ClassStatus::kRetired);
      if (old_status >= ClassStatus::kResolved || new_status >= ClassStatus::kResolved) {
        h_this->NotifyAll(self);
      }
    }
  }
}

void Class::SetDexCache(ObjPtr<DexCache> new_dex_cache) {
  SetFieldObjectTransaction(OFFSET_OF_OBJECT_MEMBER(Class, dex_cache_), new_dex_cache);
}

void Class::SetClassSize(uint32_t new_class_size) {
  if (kIsDebugBuild && new_class_size < GetClassSize()) {
    DumpClass(LOG_STREAM(FATAL_WITHOUT_ABORT), kDumpClassFullDetail);
    LOG(FATAL_WITHOUT_ABORT) << new_class_size << " vs " << GetClassSize();
    LOG(FATAL) << "class=" << PrettyTypeOf();
  }
  SetField32Transaction(OFFSET_OF_OBJECT_MEMBER(Class, class_size_), new_class_size);
}

// Return the class' name. The exact format is bizarre, but it's the specified behavior for
// Class.getName: keywords for primitive types, regular "[I" form for primitive arrays (so "int"
// but "[I"), and arrays of reference types written between "L" and ";" but with dots rather than
// slashes (so "java.lang.String" but "[Ljava.lang.String;"). Madness.
String* Class::ComputeName(Handle<Class> h_this) {
  String* name = h_this->GetName();
  if (name != nullptr) {
    return name;
  }
  std::string temp;
  const char* descriptor = h_this->GetDescriptor(&temp);
  Thread* self = Thread::Current();
  if ((descriptor[0] != 'L') && (descriptor[0] != '[')) {
    // The descriptor indicates that this is the class for
    // a primitive type; special-case the return value.
    const char* c_name = nullptr;
    switch (descriptor[0]) {
    case 'Z': c_name = "boolean"; break;
    case 'B': c_name = "byte";    break;
    case 'C': c_name = "char";    break;
    case 'S': c_name = "short";   break;
    case 'I': c_name = "int";     break;
    case 'J': c_name = "long";    break;
    case 'F': c_name = "float";   break;
    case 'D': c_name = "double";  break;
    case 'V': c_name = "void";    break;
    default:
      LOG(FATAL) << "Unknown primitive type: " << PrintableChar(descriptor[0]);
    }
    name = String::AllocFromModifiedUtf8(self, c_name);
  } else {
    // Convert the UTF-8 name to a java.lang.String. The name must use '.' to separate package
    // components.
    name = String::AllocFromModifiedUtf8(self, DescriptorToDot(descriptor).c_str());
  }
  h_this->SetName(name);
  return name;
}

void Class::DumpClass(std::ostream& os, int flags) {
  if ((flags & kDumpClassFullDetail) == 0) {
    os << PrettyClass();
    if ((flags & kDumpClassClassLoader) != 0) {
      os << ' ' << GetClassLoader();
    }
    if ((flags & kDumpClassInitialized) != 0) {
      os << ' ' << GetStatus();
    }
    os << "\n";
    return;
  }

  Thread* const self = Thread::Current();
  StackHandleScope<2> hs(self);
  Handle<Class> h_this(hs.NewHandle(this));
  Handle<Class> h_super(hs.NewHandle(GetSuperClass()));
  auto image_pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();

  std::string temp;
  os << "----- " << (IsInterface() ? "interface" : "class") << " "
     << "'" << GetDescriptor(&temp) << "' cl=" << GetClassLoader() << " -----\n",
  os << "  objectSize=" << SizeOf() << " "
     << "(" << (h_super != nullptr ? h_super->SizeOf() : -1) << " from super)\n",
  os << StringPrintf("  access=0x%04x.%04x\n",
      GetAccessFlags() >> 16, GetAccessFlags() & kAccJavaFlagsMask);
  if (h_super != nullptr) {
    os << "  super='" << h_super->PrettyClass() << "' (cl=" << h_super->GetClassLoader()
       << ")\n";
  }
  if (IsArrayClass()) {
    os << "  componentType=" << PrettyClass(GetComponentType()) << "\n";
  }
  const size_t num_direct_interfaces = NumDirectInterfaces();
  if (num_direct_interfaces > 0) {
    os << "  interfaces (" << num_direct_interfaces << "):\n";
    for (size_t i = 0; i < num_direct_interfaces; ++i) {
      ObjPtr<Class> interface = GetDirectInterface(self, h_this.Get(), i);
      if (interface == nullptr) {
        os << StringPrintf("    %2zd: nullptr!\n", i);
      } else {
        ObjPtr<ClassLoader> cl = interface->GetClassLoader();
        os << StringPrintf("    %2zd: %s (cl=%p)\n", i, PrettyClass(interface).c_str(), cl.Ptr());
      }
    }
  }
  if (!IsLoaded()) {
    os << "  class not yet loaded";
  } else {
    // After this point, this may have moved due to GetDirectInterface.
    os << "  vtable (" << h_this->NumVirtualMethods() << " entries, "
        << (h_super != nullptr ? h_super->NumVirtualMethods() : 0) << " in super):\n";
    for (size_t i = 0; i < NumVirtualMethods(); ++i) {
      os << StringPrintf("    %2zd: %s\n", i, ArtMethod::PrettyMethod(
          h_this->GetVirtualMethodDuringLinking(i, image_pointer_size)).c_str());
    }
    os << "  direct methods (" << h_this->NumDirectMethods() << " entries):\n";
    for (size_t i = 0; i < h_this->NumDirectMethods(); ++i) {
      os << StringPrintf("    %2zd: %s\n", i, ArtMethod::PrettyMethod(
          h_this->GetDirectMethod(i, image_pointer_size)).c_str());
    }
    if (h_this->NumStaticFields() > 0) {
      os << "  static fields (" << h_this->NumStaticFields() << " entries):\n";
      if (h_this->IsResolved()) {
        for (size_t i = 0; i < h_this->NumStaticFields(); ++i) {
          os << StringPrintf("    %2zd: %s\n", i,
                             ArtField::PrettyField(h_this->GetStaticField(i)).c_str());
        }
      } else {
        os << "    <not yet available>";
      }
    }
    if (h_this->NumInstanceFields() > 0) {
      os << "  instance fields (" << h_this->NumInstanceFields() << " entries):\n";
      if (h_this->IsResolved()) {
        for (size_t i = 0; i < h_this->NumInstanceFields(); ++i) {
          os << StringPrintf("    %2zd: %s\n", i,
                             ArtField::PrettyField(h_this->GetInstanceField(i)).c_str());
        }
      } else {
        os << "    <not yet available>";
      }
    }
  }
}

void Class::SetReferenceInstanceOffsets(uint32_t new_reference_offsets) {
  if (kIsDebugBuild && new_reference_offsets != kClassWalkSuper) {
    // Sanity check that the number of bits set in the reference offset bitmap
    // agrees with the number of references
    uint32_t count = 0;
    for (ObjPtr<Class> c = this; c != nullptr; c = c->GetSuperClass()) {
      count += c->NumReferenceInstanceFieldsDuringLinking();
    }
    // +1 for the Class in Object.
    CHECK_EQ(static_cast<uint32_t>(POPCOUNT(new_reference_offsets)) + 1, count);
  }
  // Not called within a transaction.
  SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, reference_instance_offsets_),
                    new_reference_offsets);
}

bool Class::IsInSamePackage(const StringPiece& descriptor1, const StringPiece& descriptor2) {
  size_t i = 0;
  size_t min_length = std::min(descriptor1.size(), descriptor2.size());
  while (i < min_length && descriptor1[i] == descriptor2[i]) {
    ++i;
  }
  if (descriptor1.find('/', i) != StringPiece::npos ||
      descriptor2.find('/', i) != StringPiece::npos) {
    return false;
  } else {
    return true;
  }
}

bool Class::IsInSamePackage(ObjPtr<Class> that) {
  ObjPtr<Class> klass1 = this;
  ObjPtr<Class> klass2 = that;
  if (klass1 == klass2) {
    return true;
  }
  // Class loaders must match.
  if (klass1->GetClassLoader() != klass2->GetClassLoader()) {
    return false;
  }
  // Arrays are in the same package when their element classes are.
  while (klass1->IsArrayClass()) {
    klass1 = klass1->GetComponentType();
  }
  while (klass2->IsArrayClass()) {
    klass2 = klass2->GetComponentType();
  }
  // trivial check again for array types
  if (klass1 == klass2) {
    return true;
  }
  // Compare the package part of the descriptor string.
  std::string temp1, temp2;
  return IsInSamePackage(klass1->GetDescriptor(&temp1), klass2->GetDescriptor(&temp2));
}

bool Class::IsThrowableClass() {
  return WellKnownClasses::ToClass(WellKnownClasses::java_lang_Throwable)->IsAssignableFrom(this);
}

void Class::SetClassLoader(ObjPtr<ClassLoader> new_class_loader) {
  if (Runtime::Current()->IsActiveTransaction()) {
    SetFieldObject<true>(OFFSET_OF_OBJECT_MEMBER(Class, class_loader_), new_class_loader);
  } else {
    SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(Class, class_loader_), new_class_loader);
  }
}

template <typename SignatureType>
static inline ArtMethod* FindInterfaceMethodWithSignature(ObjPtr<Class> klass,
                                                          const StringPiece& name,
                                                          const SignatureType& signature,
                                                          PointerSize pointer_size)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // If the current class is not an interface, skip the search of its declared methods;
  // such lookup is used only to distinguish between IncompatibleClassChangeError and
  // NoSuchMethodError and the caller has already tried to search methods in the class.
  if (LIKELY(klass->IsInterface())) {
    // Search declared methods, both direct and virtual.
    // (This lookup is used also for invoke-static on interface classes.)
    for (ArtMethod& method : klass->GetDeclaredMethodsSlice(pointer_size)) {
      if (method.GetName() == name && method.GetSignature() == signature) {
        return &method;
      }
    }
  }

  // TODO: If there is a unique maximally-specific non-abstract superinterface method,
  // we should return it, otherwise an arbitrary one can be returned.
  ObjPtr<IfTable> iftable = klass->GetIfTable();
  for (int32_t i = 0, iftable_count = iftable->Count(); i < iftable_count; ++i) {
    ObjPtr<Class> iface = iftable->GetInterface(i);
    for (ArtMethod& method : iface->GetVirtualMethodsSlice(pointer_size)) {
      if (method.GetName() == name && method.GetSignature() == signature) {
        return &method;
      }
    }
  }

  // Then search for public non-static methods in the java.lang.Object.
  if (LIKELY(klass->IsInterface())) {
    ObjPtr<Class> object_class = klass->GetSuperClass();
    DCHECK(object_class->IsObjectClass());
    for (ArtMethod& method : object_class->GetDeclaredMethodsSlice(pointer_size)) {
      if (method.IsPublic() && !method.IsStatic() &&
          method.GetName() == name && method.GetSignature() == signature) {
        return &method;
      }
    }
  }
  return nullptr;
}

ArtMethod* Class::FindInterfaceMethod(const StringPiece& name,
                                      const StringPiece& signature,
                                      PointerSize pointer_size) {
  return FindInterfaceMethodWithSignature(this, name, signature, pointer_size);
}

ArtMethod* Class::FindInterfaceMethod(const StringPiece& name,
                                      const Signature& signature,
                                      PointerSize pointer_size) {
  return FindInterfaceMethodWithSignature(this, name, signature, pointer_size);
}

ArtMethod* Class::FindInterfaceMethod(ObjPtr<DexCache> dex_cache,
                                      uint32_t dex_method_idx,
                                      PointerSize pointer_size) {
  // We always search by name and signature, ignoring the type index in the MethodId.
  const DexFile& dex_file = *dex_cache->GetDexFile();
  const DexFile::MethodId& method_id = dex_file.GetMethodId(dex_method_idx);
  StringPiece name = dex_file.StringDataByIdx(method_id.name_idx_);
  const Signature signature = dex_file.GetMethodSignature(method_id);
  return FindInterfaceMethod(name, signature, pointer_size);
}

static inline bool IsValidInheritanceCheck(ObjPtr<mirror::Class> klass,
                                           ObjPtr<mirror::Class> declaring_class)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (klass->IsArrayClass()) {
    return declaring_class->IsObjectClass();
  } else if (klass->IsInterface()) {
    return declaring_class->IsObjectClass() || declaring_class == klass;
  } else {
    return klass->IsSubClass(declaring_class);
  }
}

static inline bool IsInheritedMethod(ObjPtr<mirror::Class> klass,
                                     ObjPtr<mirror::Class> declaring_class,
                                     ArtMethod& method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK_EQ(declaring_class, method.GetDeclaringClass());
  DCHECK_NE(klass, declaring_class);
  DCHECK(IsValidInheritanceCheck(klass, declaring_class));
  uint32_t access_flags = method.GetAccessFlags();
  if ((access_flags & (kAccPublic | kAccProtected)) != 0) {
    return true;
  }
  if ((access_flags & kAccPrivate) != 0) {
    return false;
  }
  for (; klass != declaring_class; klass = klass->GetSuperClass()) {
    if (!klass->IsInSamePackage(declaring_class)) {
      return false;
    }
  }
  return true;
}

template <typename SignatureType>
static inline ArtMethod* FindClassMethodWithSignature(ObjPtr<Class> this_klass,
                                                      const StringPiece& name,
                                                      const SignatureType& signature,
                                                      PointerSize pointer_size)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Search declared methods first.
  for (ArtMethod& method : this_klass->GetDeclaredMethodsSlice(pointer_size)) {
    ArtMethod* np_method = method.GetInterfaceMethodIfProxy(pointer_size);
    if (np_method->GetName() == name && np_method->GetSignature() == signature) {
      return &method;
    }
  }

  // Then search the superclass chain. If we find an inherited method, return it.
  // If we find a method that's not inherited because of access restrictions,
  // try to find a method inherited from an interface in copied methods.
  ObjPtr<Class> klass = this_klass->GetSuperClass();
  ArtMethod* uninherited_method = nullptr;
  for (; klass != nullptr; klass = klass->GetSuperClass()) {
    DCHECK(!klass->IsProxyClass());
    for (ArtMethod& method : klass->GetDeclaredMethodsSlice(pointer_size)) {
      if (method.GetName() == name && method.GetSignature() == signature) {
        if (IsInheritedMethod(this_klass, klass, method)) {
          return &method;
        }
        uninherited_method = &method;
        break;
      }
    }
    if (uninherited_method != nullptr) {
      break;
    }
  }

  // Then search copied methods.
  // If we found a method that's not inherited, stop the search in its declaring class.
  ObjPtr<Class> end_klass = klass;
  DCHECK_EQ(uninherited_method != nullptr, end_klass != nullptr);
  klass = this_klass;
  if (UNLIKELY(klass->IsProxyClass())) {
    DCHECK(klass->GetCopiedMethodsSlice(pointer_size).empty());
    klass = klass->GetSuperClass();
  }
  for (; klass != end_klass; klass = klass->GetSuperClass()) {
    DCHECK(!klass->IsProxyClass());
    for (ArtMethod& method : klass->GetCopiedMethodsSlice(pointer_size)) {
      if (method.GetName() == name && method.GetSignature() == signature) {
        return &method;  // No further check needed, copied methods are inherited by definition.
      }
    }
  }
  return uninherited_method;  // Return the `uninherited_method` if any.
}


ArtMethod* Class::FindClassMethod(const StringPiece& name,
                                  const StringPiece& signature,
                                  PointerSize pointer_size) {
  return FindClassMethodWithSignature(this, name, signature, pointer_size);
}

ArtMethod* Class::FindClassMethod(const StringPiece& name,
                                  const Signature& signature,
                                  PointerSize pointer_size) {
  return FindClassMethodWithSignature(this, name, signature, pointer_size);
}

ArtMethod* Class::FindClassMethod(ObjPtr<DexCache> dex_cache,
                                  uint32_t dex_method_idx,
                                  PointerSize pointer_size) {
  // FIXME: Hijacking a proxy class by a custom class loader can break this assumption.
  DCHECK(!IsProxyClass());

  // First try to find a declared method by dex_method_idx if we have a dex_cache match.
  ObjPtr<DexCache> this_dex_cache = GetDexCache();
  if (this_dex_cache == dex_cache) {
    // Lookup is always performed in the class referenced by the MethodId.
    DCHECK_EQ(dex_type_idx_, GetDexFile().GetMethodId(dex_method_idx).class_idx_.index_);
    for (ArtMethod& method : GetDeclaredMethodsSlice(pointer_size)) {
      if (method.GetDexMethodIndex() == dex_method_idx) {
        return &method;
      }
    }
  }
  // If not found, we need to search by name and signature.
  const DexFile& dex_file = *dex_cache->GetDexFile();
  const DexFile::MethodId& method_id = dex_file.GetMethodId(dex_method_idx);
  const Signature signature = dex_file.GetMethodSignature(method_id);
  StringPiece name;  // Delay strlen() until actually needed.
  // If we do not have a dex_cache match, try to find the declared method in this class now.
  if (this_dex_cache != dex_cache && !GetDeclaredMethodsSlice(pointer_size).empty()) {
    DCHECK(name.empty());
    name = dex_file.StringDataByIdx(method_id.name_idx_);
    for (ArtMethod& method : GetDeclaredMethodsSlice(pointer_size)) {
      if (method.GetName() == name && method.GetSignature() == signature) {
        return &method;
      }
    }
  }

  // Then search the superclass chain. If we find an inherited method, return it.
  // If we find a method that's not inherited because of access restrictions,
  // try to find a method inherited from an interface in copied methods.
  ArtMethod* uninherited_method = nullptr;
  ObjPtr<Class> klass = GetSuperClass();
  for (; klass != nullptr; klass = klass->GetSuperClass()) {
    ArtMethod* candidate_method = nullptr;
    ArraySlice<ArtMethod> declared_methods = klass->GetDeclaredMethodsSlice(pointer_size);
    if (klass->GetDexCache() == dex_cache) {
      // Matching dex_cache. We cannot compare the `dex_method_idx` anymore because
      // the type index differs, so compare the name index and proto index.
      for (ArtMethod& method : declared_methods) {
        const DexFile::MethodId& cmp_method_id = dex_file.GetMethodId(method.GetDexMethodIndex());
        if (cmp_method_id.name_idx_ == method_id.name_idx_ &&
            cmp_method_id.proto_idx_ == method_id.proto_idx_) {
          candidate_method = &method;
          break;
        }
      }
    } else {
      if (!declared_methods.empty() && name.empty()) {
        name = dex_file.StringDataByIdx(method_id.name_idx_);
      }
      for (ArtMethod& method : declared_methods) {
        if (method.GetName() == name && method.GetSignature() == signature) {
          candidate_method = &method;
          break;
        }
      }
    }
    if (candidate_method != nullptr) {
      if (IsInheritedMethod(this, klass, *candidate_method)) {
        return candidate_method;
      } else {
        uninherited_method = candidate_method;
        break;
      }
    }
  }

  // Then search copied methods.
  // If we found a method that's not inherited, stop the search in its declaring class.
  ObjPtr<Class> end_klass = klass;
  DCHECK_EQ(uninherited_method != nullptr, end_klass != nullptr);
  // After we have searched the declared methods of the super-class chain,
  // search copied methods which can contain methods from interfaces.
  for (klass = this; klass != end_klass; klass = klass->GetSuperClass()) {
    ArraySlice<ArtMethod> copied_methods = klass->GetCopiedMethodsSlice(pointer_size);
    if (!copied_methods.empty() && name.empty()) {
      name = dex_file.StringDataByIdx(method_id.name_idx_);
    }
    for (ArtMethod& method : copied_methods) {
      if (method.GetName() == name && method.GetSignature() == signature) {
        return &method;  // No further check needed, copied methods are inherited by definition.
      }
    }
  }
  return uninherited_method;  // Return the `uninherited_method` if any.
}

ArtMethod* Class::FindConstructor(const StringPiece& signature, PointerSize pointer_size) {
  // Internal helper, never called on proxy classes. We can skip GetInterfaceMethodIfProxy().
  DCHECK(!IsProxyClass());
  StringPiece name("<init>");
  for (ArtMethod& method : GetDirectMethodsSliceUnchecked(pointer_size)) {
    if (method.GetName() == name && method.GetSignature() == signature) {
      return &method;
    }
  }
  return nullptr;
}

ArtMethod* Class::FindDeclaredDirectMethodByName(const StringPiece& name,
                                                 PointerSize pointer_size) {
  for (auto& method : GetDirectMethods(pointer_size)) {
    ArtMethod* const np_method = method.GetInterfaceMethodIfProxy(pointer_size);
    if (name == np_method->GetName()) {
      return &method;
    }
  }
  return nullptr;
}

ArtMethod* Class::FindDeclaredVirtualMethodByName(const StringPiece& name,
                                                  PointerSize pointer_size) {
  for (auto& method : GetVirtualMethods(pointer_size)) {
    ArtMethod* const np_method = method.GetInterfaceMethodIfProxy(pointer_size);
    if (name == np_method->GetName()) {
      return &method;
    }
  }
  return nullptr;
}

ArtMethod* Class::FindVirtualMethodForInterfaceSuper(ArtMethod* method, PointerSize pointer_size) {
  DCHECK(method->GetDeclaringClass()->IsInterface());
  DCHECK(IsInterface()) << "Should only be called on a interface class";
  // Check if we have one defined on this interface first. This includes searching copied ones to
  // get any conflict methods. Conflict methods are copied into each subtype from the supertype. We
  // don't do any indirect method checks here.
  for (ArtMethod& iface_method : GetVirtualMethods(pointer_size)) {
    if (method->HasSameNameAndSignature(&iface_method)) {
      return &iface_method;
    }
  }

  std::vector<ArtMethod*> abstract_methods;
  // Search through the IFTable for a working version. We don't need to check for conflicts
  // because if there was one it would appear in this classes virtual_methods_ above.

  Thread* self = Thread::Current();
  StackHandleScope<2> hs(self);
  MutableHandle<IfTable> iftable(hs.NewHandle(GetIfTable()));
  MutableHandle<Class> iface(hs.NewHandle<Class>(nullptr));
  size_t iftable_count = GetIfTableCount();
  // Find the method. We don't need to check for conflicts because they would have been in the
  // copied virtuals of this interface.  Order matters, traverse in reverse topological order; most
  // subtypiest interfaces get visited first.
  for (size_t k = iftable_count; k != 0;) {
    k--;
    DCHECK_LT(k, iftable->Count());
    iface.Assign(iftable->GetInterface(k));
    // Iterate through every declared method on this interface. Each direct method's name/signature
    // is unique so the order of the inner loop doesn't matter.
    for (auto& method_iter : iface->GetDeclaredVirtualMethods(pointer_size)) {
      ArtMethod* current_method = &method_iter;
      if (current_method->HasSameNameAndSignature(method)) {
        if (current_method->IsDefault()) {
          // Handle JLS soft errors, a default method from another superinterface tree can
          // "override" an abstract method(s) from another superinterface tree(s).  To do this,
          // ignore any [default] method which are dominated by the abstract methods we've seen so
          // far. Check if overridden by any in abstract_methods. We do not need to check for
          // default_conflicts because we would hit those before we get to this loop.
          bool overridden = false;
          for (ArtMethod* possible_override : abstract_methods) {
            DCHECK(possible_override->HasSameNameAndSignature(current_method));
            if (iface->IsAssignableFrom(possible_override->GetDeclaringClass())) {
              overridden = true;
              break;
            }
          }
          if (!overridden) {
            return current_method;
          }
        } else {
          // Is not default.
          // This might override another default method. Just stash it for now.
          abstract_methods.push_back(current_method);
        }
      }
    }
  }
  // If we reach here we either never found any declaration of the method (in which case
  // 'abstract_methods' is empty or we found no non-overriden default methods in which case
  // 'abstract_methods' contains a number of abstract implementations of the methods. We choose one
  // of these arbitrarily.
  return abstract_methods.empty() ? nullptr : abstract_methods[0];
}

ArtMethod* Class::FindClassInitializer(PointerSize pointer_size) {
  for (ArtMethod& method : GetDirectMethods(pointer_size)) {
    if (method.IsClassInitializer()) {
      DCHECK_STREQ(method.GetName(), "<clinit>");
      DCHECK_STREQ(method.GetSignature().ToString().c_str(), "()V");
      return &method;
    }
  }
  return nullptr;
}

// Custom binary search to avoid double comparisons from std::binary_search.
static ArtField* FindFieldByNameAndType(LengthPrefixedArray<ArtField>* fields,
                                        const StringPiece& name,
                                        const StringPiece& type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (fields == nullptr) {
    return nullptr;
  }
  size_t low = 0;
  size_t high = fields->size();
  ArtField* ret = nullptr;
  while (low < high) {
    size_t mid = (low + high) / 2;
    ArtField& field = fields->At(mid);
    // Fields are sorted by class, then name, then type descriptor. This is verified in dex file
    // verifier. There can be multiple fields with the same in the same class name due to proguard.
    int result = StringPiece(field.GetName()).Compare(name);
    if (result == 0) {
      result = StringPiece(field.GetTypeDescriptor()).Compare(type);
    }
    if (result < 0) {
      low = mid + 1;
    } else if (result > 0) {
      high = mid;
    } else {
      ret = &field;
      break;
    }
  }
  if (kIsDebugBuild) {
    ArtField* found = nullptr;
    for (ArtField& field : MakeIterationRangeFromLengthPrefixedArray(fields)) {
      if (name == field.GetName() && type == field.GetTypeDescriptor()) {
        found = &field;
        break;
      }
    }
    CHECK_EQ(found, ret) << "Found " << found->PrettyField() << " vs  " << ret->PrettyField();
  }
  return ret;
}

ArtField* Class::FindDeclaredInstanceField(const StringPiece& name, const StringPiece& type) {
  // Binary search by name. Interfaces are not relevant because they can't contain instance fields.
  return FindFieldByNameAndType(GetIFieldsPtr(), name, type);
}

ArtField* Class::FindDeclaredInstanceField(ObjPtr<DexCache> dex_cache, uint32_t dex_field_idx) {
  if (GetDexCache() == dex_cache) {
    for (ArtField& field : GetIFields()) {
      if (field.GetDexFieldIndex() == dex_field_idx) {
        return &field;
      }
    }
  }
  return nullptr;
}

ArtField* Class::FindInstanceField(const StringPiece& name, const StringPiece& type) {
  // Is the field in this class, or any of its superclasses?
  // Interfaces are not relevant because they can't contain instance fields.
  for (ObjPtr<Class> c = this; c != nullptr; c = c->GetSuperClass()) {
    ArtField* f = c->FindDeclaredInstanceField(name, type);
    if (f != nullptr) {
      return f;
    }
  }
  return nullptr;
}

ArtField* Class::FindInstanceField(ObjPtr<DexCache> dex_cache, uint32_t dex_field_idx) {
  // Is the field in this class, or any of its superclasses?
  // Interfaces are not relevant because they can't contain instance fields.
  for (ObjPtr<Class> c = this; c != nullptr; c = c->GetSuperClass()) {
    ArtField* f = c->FindDeclaredInstanceField(dex_cache, dex_field_idx);
    if (f != nullptr) {
      return f;
    }
  }
  return nullptr;
}

ArtField* Class::FindDeclaredStaticField(const StringPiece& name, const StringPiece& type) {
  DCHECK(type != nullptr);
  return FindFieldByNameAndType(GetSFieldsPtr(), name, type);
}

ArtField* Class::FindDeclaredStaticField(ObjPtr<DexCache> dex_cache, uint32_t dex_field_idx) {
  if (dex_cache == GetDexCache()) {
    for (ArtField& field : GetSFields()) {
      if (field.GetDexFieldIndex() == dex_field_idx) {
        return &field;
      }
    }
  }
  return nullptr;
}

ArtField* Class::FindStaticField(Thread* self,
                                 ObjPtr<Class> klass,
                                 const StringPiece& name,
                                 const StringPiece& type) {
  // Is the field in this class (or its interfaces), or any of its
  // superclasses (or their interfaces)?
  for (ObjPtr<Class> k = klass; k != nullptr; k = k->GetSuperClass()) {
    // Is the field in this class?
    ArtField* f = k->FindDeclaredStaticField(name, type);
    if (f != nullptr) {
      return f;
    }
    // Is this field in any of this class' interfaces?
    for (uint32_t i = 0, num_interfaces = k->NumDirectInterfaces(); i != num_interfaces; ++i) {
      ObjPtr<Class> interface = GetDirectInterface(self, k, i);
      DCHECK(interface != nullptr);
      f = FindStaticField(self, interface, name, type);
      if (f != nullptr) {
        return f;
      }
    }
  }
  return nullptr;
}

ArtField* Class::FindStaticField(Thread* self,
                                 ObjPtr<Class> klass,
                                 ObjPtr<DexCache> dex_cache,
                                 uint32_t dex_field_idx) {
  for (ObjPtr<Class> k = klass; k != nullptr; k = k->GetSuperClass()) {
    // Is the field in this class?
    ArtField* f = k->FindDeclaredStaticField(dex_cache, dex_field_idx);
    if (f != nullptr) {
      return f;
    }
    // Though GetDirectInterface() should not cause thread suspension when called
    // from here, it takes a Handle as an argument, so we need to wrap `k`.
    ScopedAssertNoThreadSuspension ants(__FUNCTION__);
    // Is this field in any of this class' interfaces?
    for (uint32_t i = 0, num_interfaces = k->NumDirectInterfaces(); i != num_interfaces; ++i) {
      ObjPtr<Class> interface = GetDirectInterface(self, k, i);
      DCHECK(interface != nullptr);
      f = FindStaticField(self, interface, dex_cache, dex_field_idx);
      if (f != nullptr) {
        return f;
      }
    }
  }
  return nullptr;
}

ArtField* Class::FindField(Thread* self,
                           ObjPtr<Class> klass,
                           const StringPiece& name,
                           const StringPiece& type) {
  // Find a field using the JLS field resolution order
  for (ObjPtr<Class> k = klass; k != nullptr; k = k->GetSuperClass()) {
    // Is the field in this class?
    ArtField* f = k->FindDeclaredInstanceField(name, type);
    if (f != nullptr) {
      return f;
    }
    f = k->FindDeclaredStaticField(name, type);
    if (f != nullptr) {
      return f;
    }
    // Is this field in any of this class' interfaces?
    for (uint32_t i = 0, num_interfaces = k->NumDirectInterfaces(); i != num_interfaces; ++i) {
      ObjPtr<Class> interface = GetDirectInterface(self, k, i);
      DCHECK(interface != nullptr);
      f = FindStaticField(self, interface, name, type);
      if (f != nullptr) {
        return f;
      }
    }
  }
  return nullptr;
}

void Class::SetSkipAccessChecksFlagOnAllMethods(PointerSize pointer_size) {
  DCHECK(IsVerified());
  for (auto& m : GetMethods(pointer_size)) {
    if (!m.IsNative() && m.IsInvokable()) {
      m.SetSkipAccessChecks();
    }
  }
}

const char* Class::GetDescriptor(std::string* storage) {
  if (IsPrimitive()) {
    return Primitive::Descriptor(GetPrimitiveType());
  } else if (IsArrayClass()) {
    return GetArrayDescriptor(storage);
  } else if (IsProxyClass()) {
    *storage = Runtime::Current()->GetClassLinker()->GetDescriptorForProxy(this);
    return storage->c_str();
  } else {
    const DexFile& dex_file = GetDexFile();
    const DexFile::TypeId& type_id = dex_file.GetTypeId(GetClassDef()->class_idx_);
    return dex_file.GetTypeDescriptor(type_id);
  }
}

const char* Class::GetArrayDescriptor(std::string* storage) {
  std::string temp;
  const char* elem_desc = GetComponentType()->GetDescriptor(&temp);
  *storage = "[";
  *storage += elem_desc;
  return storage->c_str();
}

const DexFile::ClassDef* Class::GetClassDef() {
  uint16_t class_def_idx = GetDexClassDefIndex();
  if (class_def_idx == DexFile::kDexNoIndex16) {
    return nullptr;
  }
  return &GetDexFile().GetClassDef(class_def_idx);
}

dex::TypeIndex Class::GetDirectInterfaceTypeIdx(uint32_t idx) {
  DCHECK(!IsPrimitive());
  DCHECK(!IsArrayClass());
  return GetInterfaceTypeList()->GetTypeItem(idx).type_idx_;
}

ObjPtr<Class> Class::GetDirectInterface(Thread* self, ObjPtr<Class> klass, uint32_t idx) {
  DCHECK(klass != nullptr);
  DCHECK(!klass->IsPrimitive());
  if (klass->IsArrayClass()) {
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    // Use ClassLinker::LookupClass(); avoid poisoning ObjPtr<>s by ClassLinker::FindSystemClass().
    ObjPtr<Class> interface;
    if (idx == 0) {
      interface = class_linker->LookupClass(self, "Ljava/lang/Cloneable;", nullptr);
    } else {
      DCHECK_EQ(1U, idx);
      interface = class_linker->LookupClass(self, "Ljava/io/Serializable;", nullptr);
    }
    DCHECK(interface != nullptr);
    return interface;
  } else if (klass->IsProxyClass()) {
    ObjPtr<ObjectArray<Class>> interfaces = klass->GetProxyInterfaces();
    DCHECK(interfaces != nullptr);
    return interfaces->Get(idx);
  } else {
    dex::TypeIndex type_idx = klass->GetDirectInterfaceTypeIdx(idx);
    ObjPtr<Class> interface = Runtime::Current()->GetClassLinker()->LookupResolvedType(
        type_idx, klass->GetDexCache(), klass->GetClassLoader());
    return interface;
  }
}

ObjPtr<Class> Class::ResolveDirectInterface(Thread* self, Handle<Class> klass, uint32_t idx) {
  ObjPtr<Class> interface = GetDirectInterface(self, klass.Get(), idx);
  if (interface == nullptr) {
    DCHECK(!klass->IsArrayClass());
    DCHECK(!klass->IsProxyClass());
    dex::TypeIndex type_idx = klass->GetDirectInterfaceTypeIdx(idx);
    interface = Runtime::Current()->GetClassLinker()->ResolveType(type_idx, klass.Get());
    CHECK(interface != nullptr || self->IsExceptionPending());
  }
  return interface;
}

ObjPtr<Class> Class::GetCommonSuperClass(Handle<Class> klass) {
  DCHECK(klass != nullptr);
  DCHECK(!klass->IsInterface());
  DCHECK(!IsInterface());
  ObjPtr<Class> common_super_class = this;
  while (!common_super_class->IsAssignableFrom(klass.Get())) {
    ObjPtr<Class> old_common = common_super_class;
    common_super_class = old_common->GetSuperClass();
    DCHECK(common_super_class != nullptr) << old_common->PrettyClass();
  }
  return common_super_class;
}

const char* Class::GetSourceFile() {
  const DexFile& dex_file = GetDexFile();
  const DexFile::ClassDef* dex_class_def = GetClassDef();
  if (dex_class_def == nullptr) {
    // Generated classes have no class def.
    return nullptr;
  }
  return dex_file.GetSourceFile(*dex_class_def);
}

std::string Class::GetLocation() {
  ObjPtr<DexCache> dex_cache = GetDexCache();
  if (dex_cache != nullptr && !IsProxyClass()) {
    return dex_cache->GetLocation()->ToModifiedUtf8();
  }
  // Arrays and proxies are generated and have no corresponding dex file location.
  return "generated class";
}

const DexFile::TypeList* Class::GetInterfaceTypeList() {
  const DexFile::ClassDef* class_def = GetClassDef();
  if (class_def == nullptr) {
    return nullptr;
  }
  return GetDexFile().GetInterfacesList(*class_def);
}

void Class::PopulateEmbeddedVTable(PointerSize pointer_size) {
  PointerArray* table = GetVTableDuringLinking();
  CHECK(table != nullptr) << PrettyClass();
  const size_t table_length = table->GetLength();
  SetEmbeddedVTableLength(table_length);
  for (size_t i = 0; i < table_length; i++) {
    SetEmbeddedVTableEntry(i, table->GetElementPtrSize<ArtMethod*>(i, pointer_size), pointer_size);
  }
  // Keep java.lang.Object class's vtable around for since it's easier
  // to be reused by array classes during their linking.
  if (!IsObjectClass()) {
    SetVTable(nullptr);
  }
}

class ReadBarrierOnNativeRootsVisitor {
 public:
  void operator()(ObjPtr<Object> obj ATTRIBUTE_UNUSED,
                  MemberOffset offset ATTRIBUTE_UNUSED,
                  bool is_static ATTRIBUTE_UNUSED) const {}

  void VisitRootIfNonNull(CompressedReference<Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(CompressedReference<Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ObjPtr<Object> old_ref = root->AsMirrorPtr();
    ObjPtr<Object> new_ref = ReadBarrier::BarrierForRoot(root);
    if (old_ref != new_ref) {
      // Update the field atomically. This may fail if mutator updates before us, but it's ok.
      auto* atomic_root =
          reinterpret_cast<Atomic<CompressedReference<Object>>*>(root);
      atomic_root->CompareAndSetStrongSequentiallyConsistent(
          CompressedReference<Object>::FromMirrorPtr(old_ref.Ptr()),
          CompressedReference<Object>::FromMirrorPtr(new_ref.Ptr()));
    }
  }
};

// The pre-fence visitor for Class::CopyOf().
class CopyClassVisitor {
 public:
  CopyClassVisitor(Thread* self,
                   Handle<Class>* orig,
                   size_t new_length,
                   size_t copy_bytes,
                   ImTable* imt,
                   PointerSize pointer_size)
      : self_(self), orig_(orig), new_length_(new_length),
        copy_bytes_(copy_bytes), imt_(imt), pointer_size_(pointer_size) {
  }

  void operator()(ObjPtr<Object> obj, size_t usable_size ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    StackHandleScope<1> hs(self_);
    Handle<mirror::Class> h_new_class_obj(hs.NewHandle(obj->AsClass()));
    Object::CopyObject(h_new_class_obj.Get(), orig_->Get(), copy_bytes_);
    Class::SetStatus(h_new_class_obj, ClassStatus::kResolving, self_);
    h_new_class_obj->PopulateEmbeddedVTable(pointer_size_);
    h_new_class_obj->SetImt(imt_, pointer_size_);
    h_new_class_obj->SetClassSize(new_length_);
    // Visit all of the references to make sure there is no from space references in the native
    // roots.
    ObjPtr<Object>(h_new_class_obj.Get())->VisitReferences(
        ReadBarrierOnNativeRootsVisitor(), VoidFunctor());
  }

 private:
  Thread* const self_;
  Handle<Class>* const orig_;
  const size_t new_length_;
  const size_t copy_bytes_;
  ImTable* imt_;
  const PointerSize pointer_size_;
  DISALLOW_COPY_AND_ASSIGN(CopyClassVisitor);
};

Class* Class::CopyOf(Thread* self, int32_t new_length, ImTable* imt, PointerSize pointer_size) {
  DCHECK_GE(new_length, static_cast<int32_t>(sizeof(Class)));
  // We may get copied by a compacting GC.
  StackHandleScope<1> hs(self);
  Handle<Class> h_this(hs.NewHandle(this));
  gc::Heap* heap = Runtime::Current()->GetHeap();
  // The num_bytes (3rd param) is sizeof(Class) as opposed to SizeOf()
  // to skip copying the tail part that we will overwrite here.
  CopyClassVisitor visitor(self, &h_this, new_length, sizeof(Class), imt, pointer_size);
  ObjPtr<Object> new_class = kMovingClasses ?
      heap->AllocObject<true>(self, java_lang_Class_.Read(), new_length, visitor) :
      heap->AllocNonMovableObject<true>(self, java_lang_Class_.Read(), new_length, visitor);
  if (UNLIKELY(new_class == nullptr)) {
    self->AssertPendingOOMException();
    return nullptr;
  }
  return new_class->AsClass();
}

bool Class::ProxyDescriptorEquals(const char* match) {
  DCHECK(IsProxyClass());
  return Runtime::Current()->GetClassLinker()->GetDescriptorForProxy(this) == match;
}

// TODO: Move this to java_lang_Class.cc?
ArtMethod* Class::GetDeclaredConstructor(
    Thread* self, Handle<ObjectArray<Class>> args, PointerSize pointer_size) {
  for (auto& m : GetDirectMethods(pointer_size)) {
    // Skip <clinit> which is a static constructor, as well as non constructors.
    if (m.IsStatic() || !m.IsConstructor()) {
      continue;
    }
    // May cause thread suspension and exceptions.
    if (m.GetInterfaceMethodIfProxy(kRuntimePointerSize)->EqualParameters(args)) {
      return &m;
    }
    if (UNLIKELY(self->IsExceptionPending())) {
      return nullptr;
    }
  }
  return nullptr;
}

uint32_t Class::Depth() {
  uint32_t depth = 0;
  for (ObjPtr<Class> klass = this; klass->GetSuperClass() != nullptr; klass = klass->GetSuperClass()) {
    depth++;
  }
  return depth;
}

dex::TypeIndex Class::FindTypeIndexInOtherDexFile(const DexFile& dex_file) {
  std::string temp;
  const DexFile::TypeId* type_id = dex_file.FindTypeId(GetDescriptor(&temp));
  return (type_id == nullptr) ? dex::TypeIndex() : dex_file.GetIndexForTypeId(*type_id);
}

template <PointerSize kPointerSize, bool kTransactionActive>
ObjPtr<Method> Class::GetDeclaredMethodInternal(
    Thread* self,
    ObjPtr<Class> klass,
    ObjPtr<String> name,
    ObjPtr<ObjectArray<Class>> args) {
  // Covariant return types permit the class to define multiple
  // methods with the same name and parameter types. Prefer to
  // return a non-synthetic method in such situations. We may
  // still return a synthetic method to handle situations like
  // escalated visibility. We never return miranda methods that
  // were synthesized by the runtime.
  StackHandleScope<3> hs(self);
  auto h_method_name = hs.NewHandle(name);
  if (UNLIKELY(h_method_name == nullptr)) {
    ThrowNullPointerException("name == null");
    return nullptr;
  }
  auto h_args = hs.NewHandle(args);
  Handle<Class> h_klass = hs.NewHandle(klass);
  ArtMethod* result = nullptr;
  for (auto& m : h_klass->GetDeclaredVirtualMethods(kPointerSize)) {
    auto* np_method = m.GetInterfaceMethodIfProxy(kPointerSize);
    // May cause thread suspension.
    ObjPtr<String> np_name = np_method->GetNameAsString(self);
    if (!np_name->Equals(h_method_name.Get()) || !np_method->EqualParameters(h_args)) {
      if (UNLIKELY(self->IsExceptionPending())) {
        return nullptr;
      }
      continue;
    }
    if (!m.IsMiranda()) {
      if (!m.IsSynthetic()) {
        return Method::CreateFromArtMethod<kPointerSize, kTransactionActive>(self, &m);
      }
      result = &m;  // Remember as potential result if it's not a miranda method.
    }
  }
  if (result == nullptr) {
    for (auto& m : h_klass->GetDirectMethods(kPointerSize)) {
      auto modifiers = m.GetAccessFlags();
      if ((modifiers & kAccConstructor) != 0) {
        continue;
      }
      auto* np_method = m.GetInterfaceMethodIfProxy(kPointerSize);
      // May cause thread suspension.
      ObjPtr<String> np_name = np_method->GetNameAsString(self);
      if (np_name == nullptr) {
        self->AssertPendingException();
        return nullptr;
      }
      if (!np_name->Equals(h_method_name.Get()) || !np_method->EqualParameters(h_args)) {
        if (UNLIKELY(self->IsExceptionPending())) {
          return nullptr;
        }
        continue;
      }
      DCHECK(!m.IsMiranda());  // Direct methods cannot be miranda methods.
      if ((modifiers & kAccSynthetic) == 0) {
        return Method::CreateFromArtMethod<kPointerSize, kTransactionActive>(self, &m);
      }
      result = &m;  // Remember as potential result.
    }
  }
  return result != nullptr
      ? Method::CreateFromArtMethod<kPointerSize, kTransactionActive>(self, result)
      : nullptr;
}

template
ObjPtr<Method> Class::GetDeclaredMethodInternal<PointerSize::k32, false>(
    Thread* self,
    ObjPtr<Class> klass,
    ObjPtr<String> name,
    ObjPtr<ObjectArray<Class>> args);
template
ObjPtr<Method> Class::GetDeclaredMethodInternal<PointerSize::k32, true>(
    Thread* self,
    ObjPtr<Class> klass,
    ObjPtr<String> name,
    ObjPtr<ObjectArray<Class>> args);
template
ObjPtr<Method> Class::GetDeclaredMethodInternal<PointerSize::k64, false>(
    Thread* self,
    ObjPtr<Class> klass,
    ObjPtr<String> name,
    ObjPtr<ObjectArray<Class>> args);
template
ObjPtr<Method> Class::GetDeclaredMethodInternal<PointerSize::k64, true>(
    Thread* self,
    ObjPtr<Class> klass,
    ObjPtr<String> name,
    ObjPtr<ObjectArray<Class>> args);

template <PointerSize kPointerSize, bool kTransactionActive>
ObjPtr<Constructor> Class::GetDeclaredConstructorInternal(
    Thread* self,
    ObjPtr<Class> klass,
    ObjPtr<ObjectArray<Class>> args) {
  StackHandleScope<1> hs(self);
  ArtMethod* result = klass->GetDeclaredConstructor(self, hs.NewHandle(args), kPointerSize);
  return result != nullptr
      ? Constructor::CreateFromArtMethod<kPointerSize, kTransactionActive>(self, result)
      : nullptr;
}

// Constructor::CreateFromArtMethod<kTransactionActive>(self, result)

template
ObjPtr<Constructor> Class::GetDeclaredConstructorInternal<PointerSize::k32, false>(
    Thread* self,
    ObjPtr<Class> klass,
    ObjPtr<ObjectArray<Class>> args);
template
ObjPtr<Constructor> Class::GetDeclaredConstructorInternal<PointerSize::k32, true>(
    Thread* self,
    ObjPtr<Class> klass,
    ObjPtr<ObjectArray<Class>> args);
template
ObjPtr<Constructor> Class::GetDeclaredConstructorInternal<PointerSize::k64, false>(
    Thread* self,
    ObjPtr<Class> klass,
    ObjPtr<ObjectArray<Class>> args);
template
ObjPtr<Constructor> Class::GetDeclaredConstructorInternal<PointerSize::k64, true>(
    Thread* self,
    ObjPtr<Class> klass,
    ObjPtr<ObjectArray<Class>> args);

int32_t Class::GetInnerClassFlags(Handle<Class> h_this, int32_t default_value) {
  if (h_this->IsProxyClass() || h_this->GetDexCache() == nullptr) {
    return default_value;
  }
  uint32_t flags;
  if (!annotations::GetInnerClassFlags(h_this, &flags)) {
    return default_value;
  }
  return flags;
}

void Class::SetObjectSizeAllocFastPath(uint32_t new_object_size) {
  if (Runtime::Current()->IsActiveTransaction()) {
    SetField32Volatile<true>(ObjectSizeAllocFastPathOffset(), new_object_size);
  } else {
    SetField32Volatile<false>(ObjectSizeAllocFastPathOffset(), new_object_size);
  }
}

std::string Class::PrettyDescriptor(ObjPtr<mirror::Class> klass) {
  if (klass == nullptr) {
    return "null";
  }
  return klass->PrettyDescriptor();
}

std::string Class::PrettyDescriptor() {
  std::string temp;
  return art::PrettyDescriptor(GetDescriptor(&temp));
}

std::string Class::PrettyClass(ObjPtr<mirror::Class> c) {
  if (c == nullptr) {
    return "null";
  }
  return c->PrettyClass();
}

std::string Class::PrettyClass() {
  std::string result;
  result += "java.lang.Class<";
  result += PrettyDescriptor();
  result += ">";
  return result;
}

std::string Class::PrettyClassAndClassLoader(ObjPtr<mirror::Class> c) {
  if (c == nullptr) {
    return "null";
  }
  return c->PrettyClassAndClassLoader();
}

std::string Class::PrettyClassAndClassLoader() {
  std::string result;
  result += "java.lang.Class<";
  result += PrettyDescriptor();
  result += ",";
  result += mirror::Object::PrettyTypeOf(GetClassLoader());
  // TODO: add an identifying hash value for the loader
  result += ">";
  return result;
}

template<VerifyObjectFlags kVerifyFlags> void Class::GetAccessFlagsDCheck() {
  // Check class is loaded/retired or this is java.lang.String that has a
  // circularity issue during loading the names of its members
  DCHECK(IsIdxLoaded<kVerifyFlags>() || IsRetired<kVerifyFlags>() ||
         IsErroneous<static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis)>() ||
         this == String::GetJavaLangString())
              << "IsIdxLoaded=" << IsIdxLoaded<kVerifyFlags>()
              << " IsRetired=" << IsRetired<kVerifyFlags>()
              << " IsErroneous=" <<
              IsErroneous<static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis)>()
              << " IsString=" << (this == String::GetJavaLangString())
              << " status= " << GetStatus<kVerifyFlags>()
              << " descriptor=" << PrettyDescriptor();
}
// Instantiate the common cases.
template void Class::GetAccessFlagsDCheck<kVerifyNone>();
template void Class::GetAccessFlagsDCheck<kVerifyThis>();
template void Class::GetAccessFlagsDCheck<kVerifyReads>();
template void Class::GetAccessFlagsDCheck<kVerifyWrites>();
template void Class::GetAccessFlagsDCheck<kVerifyAll>();

}  // namespace mirror
}  // namespace art
