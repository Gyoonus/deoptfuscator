/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "cha.h"

#include "art_method-inl.h"
#include "base/logging.h"  // For VLOG
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "linear_alloc.h"
#include "mirror/class_loader.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread.h"
#include "thread_list.h"
#include "thread_pool.h"

namespace art {

void ClassHierarchyAnalysis::AddDependency(ArtMethod* method,
                                           ArtMethod* dependent_method,
                                           OatQuickMethodHeader* dependent_header) {
  const auto it = cha_dependency_map_.insert(
      decltype(cha_dependency_map_)::value_type(method, ListOfDependentPairs())).first;
  it->second.push_back({dependent_method, dependent_header});
}

static const ClassHierarchyAnalysis::ListOfDependentPairs s_empty_vector;

const ClassHierarchyAnalysis::ListOfDependentPairs& ClassHierarchyAnalysis::GetDependents(
    ArtMethod* method) {
  auto it = cha_dependency_map_.find(method);
  if (it != cha_dependency_map_.end()) {
    return it->second;
  }
  return s_empty_vector;
}

void ClassHierarchyAnalysis::RemoveAllDependenciesFor(ArtMethod* method) {
  cha_dependency_map_.erase(method);
}

void ClassHierarchyAnalysis::RemoveDependentsWithMethodHeaders(
    const std::unordered_set<OatQuickMethodHeader*>& method_headers) {
  // Iterate through all entries in the dependency map and remove any entry that
  // contains one of those in method_headers.
  for (auto map_it = cha_dependency_map_.begin(); map_it != cha_dependency_map_.end(); ) {
    ListOfDependentPairs& dependents = map_it->second;
    dependents.erase(
        std::remove_if(
            dependents.begin(),
            dependents.end(),
            [&method_headers](MethodAndMethodHeaderPair& dependent) {
              return method_headers.find(dependent.second) != method_headers.end();
            }),
        dependents.end());

    // Remove the map entry if there are no more dependents.
    if (dependents.empty()) {
      map_it = cha_dependency_map_.erase(map_it);
    } else {
      map_it++;
    }
  }
}

void ClassHierarchyAnalysis::ResetSingleImplementationInHierarchy(ObjPtr<mirror::Class> klass,
                                                                  const LinearAlloc* alloc,
                                                                  const PointerSize pointer_size)
                                                                  const {
  // Presumably called from some sort of class visitor, no null pointers expected.
  DCHECK(klass != nullptr);
  DCHECK(alloc != nullptr);

  // Skip interfaces since they cannot provide SingleImplementations to work with.
  if (klass->IsInterface()) {
    return;
  }

  // This method is called while visiting classes in the class table of a class loader.
  // That means, some 'klass'es can belong to other classloaders. Argument 'alloc'
  // allows to explicitly indicate a classloader, which is going to be deleted.
  // Filter out classes, that do not belong to it.
  if (!alloc->ContainsUnsafe(klass->GetMethodsPtr())) {
    return;
  }

  // CHA analysis is only applied to resolved classes.
  if (!klass->IsResolved()) {
    return;
  }

  ObjPtr<mirror::Class> super = klass->GetSuperClass<kDefaultVerifyFlags, kWithoutReadBarrier>();

  // Skip Object class and primitive classes.
  if (super == nullptr) {
    return;
  }

  // The class is going to be deleted. Iterate over the virtual methods of its superclasses to see
  // if they have SingleImplementations methods defined by 'klass'.
  // Skip all virtual methods that do not override methods from super class since they cannot be
  // SingleImplementations for anything.
  int32_t vtbl_size = super->GetVTableLength<kDefaultVerifyFlags, kWithoutReadBarrier>();
  ObjPtr<mirror::ClassLoader> loader =
      klass->GetClassLoader<kDefaultVerifyFlags, kWithoutReadBarrier>();
  for (int vtbl_index = 0; vtbl_index < vtbl_size; ++vtbl_index) {
    ArtMethod* method =
        klass->GetVTableEntry<kDefaultVerifyFlags, kWithoutReadBarrier>(vtbl_index, pointer_size);
    if (!alloc->ContainsUnsafe(method)) {
      continue;
    }

    // Find all occurrences of virtual methods in parents' SingleImplementations fields
    // and reset them.
    // No need to reset SingleImplementations for the method itself (it will be cleared anyways),
    // so start with a superclass and move up looking into a corresponding vtbl slot.
    for (ObjPtr<mirror::Class> super_it = super;
         super_it != nullptr &&
             super_it->GetVTableLength<kDefaultVerifyFlags, kWithoutReadBarrier>() > vtbl_index;
         super_it = super_it->GetSuperClass<kDefaultVerifyFlags, kWithoutReadBarrier>()) {
      // Skip superclasses that are also going to be unloaded.
      ObjPtr<mirror::ClassLoader> super_loader = super_it->
          GetClassLoader<kDefaultVerifyFlags, kWithoutReadBarrier>();
      if (super_loader == loader) {
        continue;
      }

      ArtMethod* super_method = super_it->
          GetVTableEntry<kDefaultVerifyFlags, kWithoutReadBarrier>(vtbl_index, pointer_size);
      if (super_method->IsAbstract<kWithoutReadBarrier>() &&
          super_method->HasSingleImplementation<kWithoutReadBarrier>() &&
          super_method->GetSingleImplementation<kWithoutReadBarrier>(pointer_size) == method) {
        // Do like there was no single implementation defined previously
        // for this method of the superclass.
        super_method->SetSingleImplementation<kWithoutReadBarrier>(nullptr, pointer_size);
      } else {
        // No related SingleImplementations could possibly be found any further.
        DCHECK(!super_method->HasSingleImplementation<kWithoutReadBarrier>());
        break;
      }
    }
  }

  // Check all possible interface methods too.
  ObjPtr<mirror::IfTable> iftable = klass->GetIfTable<kDefaultVerifyFlags, kWithoutReadBarrier>();
  const size_t ifcount = klass->GetIfTableCount<kDefaultVerifyFlags, kWithoutReadBarrier>();
  for (size_t i = 0; i < ifcount; ++i) {
    ObjPtr<mirror::Class> interface =
        iftable->GetInterface<kDefaultVerifyFlags, kWithoutReadBarrier>(i);
    for (size_t j = 0,
         count = iftable->GetMethodArrayCount<kDefaultVerifyFlags, kWithoutReadBarrier>(i);
         j < count;
         ++j) {
      ArtMethod* method = interface->GetVirtualMethod(j, pointer_size);
      if (method->HasSingleImplementation<kWithoutReadBarrier>() &&
          alloc->ContainsUnsafe(
              method->GetSingleImplementation<kWithoutReadBarrier>(pointer_size)) &&
          !method->IsDefault<kWithoutReadBarrier>()) {
        // Do like there was no single implementation defined previously for this method.
        method->SetSingleImplementation<kWithoutReadBarrier>(nullptr, pointer_size);
      }
    }
  }
}

// This stack visitor walks the stack and for compiled code with certain method
// headers, sets the should_deoptimize flag on stack to 1.
// TODO: also set the register value to 1 when should_deoptimize is allocated in
// a register.
class CHAStackVisitor FINAL  : public StackVisitor {
 public:
  CHAStackVisitor(Thread* thread_in,
                  Context* context,
                  const std::unordered_set<OatQuickMethodHeader*>& method_headers)
      : StackVisitor(thread_in, context, StackVisitor::StackWalkKind::kSkipInlinedFrames),
        method_headers_(method_headers) {
  }

  bool VisitFrame() OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* method = GetMethod();
    // Avoid types of methods that do not have an oat quick method header.
    if (method == nullptr ||
        method->IsRuntimeMethod() ||
        method->IsNative() ||
        method->IsProxyMethod()) {
      return true;
    }
    if (GetCurrentQuickFrame() == nullptr) {
      // Not compiled code.
      return true;
    }
    // Method may have multiple versions of compiled code. Check
    // the method header to see if it has should_deoptimize flag.
    const OatQuickMethodHeader* method_header = GetCurrentOatQuickMethodHeader();
    DCHECK(method_header != nullptr);
    if (!method_header->HasShouldDeoptimizeFlag()) {
      // This compiled version doesn't have should_deoptimize flag. Skip.
      return true;
    }
    auto it = std::find(method_headers_.begin(), method_headers_.end(), method_header);
    if (it == method_headers_.end()) {
      // Not in the list of method headers that should be deoptimized.
      return true;
    }

    // The compiled code on stack is not valid anymore. Need to deoptimize.
    SetShouldDeoptimizeFlag();

    return true;
  }

 private:
  void SetShouldDeoptimizeFlag() REQUIRES_SHARED(Locks::mutator_lock_) {
    QuickMethodFrameInfo frame_info = GetCurrentQuickFrameInfo();
    size_t frame_size = frame_info.FrameSizeInBytes();
    uint8_t* sp = reinterpret_cast<uint8_t*>(GetCurrentQuickFrame());
    size_t core_spill_size = POPCOUNT(frame_info.CoreSpillMask()) *
        GetBytesPerGprSpillLocation(kRuntimeISA);
    size_t fpu_spill_size = POPCOUNT(frame_info.FpSpillMask()) *
        GetBytesPerFprSpillLocation(kRuntimeISA);
    size_t offset = frame_size - core_spill_size - fpu_spill_size - kShouldDeoptimizeFlagSize;
    uint8_t* should_deoptimize_addr = sp + offset;
    // Set deoptimization flag to 1.
    DCHECK(*should_deoptimize_addr == 0 || *should_deoptimize_addr == 1);
    *should_deoptimize_addr = 1;
  }

  // Set of method headers for compiled code that should be deoptimized.
  const std::unordered_set<OatQuickMethodHeader*>& method_headers_;

  DISALLOW_COPY_AND_ASSIGN(CHAStackVisitor);
};

class CHACheckpoint FINAL : public Closure {
 public:
  explicit CHACheckpoint(const std::unordered_set<OatQuickMethodHeader*>& method_headers)
      : barrier_(0),
        method_headers_(method_headers) {}

  void Run(Thread* thread) OVERRIDE {
    // Note thread and self may not be equal if thread was already suspended at
    // the point of the request.
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    CHAStackVisitor visitor(thread, nullptr, method_headers_);
    visitor.WalkStack();
    barrier_.Pass(self);
  }

  void WaitForThreadsToRunThroughCheckpoint(size_t threads_running_checkpoint) {
    Thread* self = Thread::Current();
    ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
    barrier_.Increment(self, threads_running_checkpoint);
  }

 private:
  // The barrier to be passed through and for the requestor to wait upon.
  Barrier barrier_;
  // List of method headers for invalidated compiled code.
  const std::unordered_set<OatQuickMethodHeader*>& method_headers_;

  DISALLOW_COPY_AND_ASSIGN(CHACheckpoint);
};


static void VerifyNonSingleImplementation(mirror::Class* verify_class,
                                          uint16_t verify_index,
                                          ArtMethod* excluded_method)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (!kIsDebugBuild) {
    return;
  }

  // Grab cha_lock_ to make sure all single-implementation updates are seen.
  MutexLock cha_mu(Thread::Current(), *Locks::cha_lock_);

  PointerSize image_pointer_size =
      Runtime::Current()->GetClassLinker()->GetImagePointerSize();

  mirror::Class* input_verify_class = verify_class;

  while (verify_class != nullptr) {
    if (verify_index >= verify_class->GetVTableLength()) {
      return;
    }
    ArtMethod* verify_method = verify_class->GetVTableEntry(verify_index, image_pointer_size);
    if (verify_method != excluded_method) {
      auto construct_parent_chain = [](mirror::Class* failed, mirror::Class* in)
          REQUIRES_SHARED(Locks::mutator_lock_) {
        std::string tmp = in->PrettyClass();
        while (in != failed) {
          in = in->GetSuperClass();
          tmp = tmp + "->" + in->PrettyClass();
        }
        return tmp;
      };
      DCHECK(!verify_method->HasSingleImplementation())
          << "class: " << verify_class->PrettyClass()
          << " verify_method: " << verify_method->PrettyMethod(true)
          << " (" << construct_parent_chain(verify_class, input_verify_class) << ")"
          << " excluded_method: " << ArtMethod::PrettyMethod(excluded_method);
      if (verify_method->IsAbstract()) {
        DCHECK(verify_method->GetSingleImplementation(image_pointer_size) == nullptr);
      }
    }
    verify_class = verify_class->GetSuperClass();
  }
}

void ClassHierarchyAnalysis::CheckVirtualMethodSingleImplementationInfo(
    Handle<mirror::Class> klass,
    ArtMethod* virtual_method,
    ArtMethod* method_in_super,
    std::unordered_set<ArtMethod*>& invalidated_single_impl_methods,
    PointerSize pointer_size) {
  // TODO: if klass is not instantiable, virtual_method isn't invocable yet so
  // even if it overrides, it doesn't invalidate single-implementation
  // assumption.

  DCHECK((virtual_method != method_in_super) || virtual_method->IsAbstract());
  DCHECK(method_in_super->GetDeclaringClass()->IsResolved()) << "class isn't resolved";
  // If virtual_method doesn't come from a default interface method, it should
  // be supplied by klass.
  DCHECK(virtual_method == method_in_super ||
         virtual_method->IsCopied() ||
         virtual_method->GetDeclaringClass() == klass.Get());

  // To make updating single-implementation flags simple, we always maintain the following
  // invariant:
  // Say all virtual methods in the same vtable slot, starting from the bottom child class
  // to super classes, is a sequence of unique methods m3, m2, m1, ... (after removing duplicate
  // methods for inherited methods).
  // For example for the following class hierarchy,
  //   class A { void m() { ... } }
  //   class B extends A { void m() { ... } }
  //   class C extends B {}
  //   class D extends C { void m() { ... } }
  // the sequence is D.m(), B.m(), A.m().
  // The single-implementation status for that sequence of methods begin with one or two true's,
  // then become all falses. The only case where two true's are possible is for one abstract
  // method m and one non-abstract method mImpl that overrides method m.
  // With the invariant, when linking in a new class, we only need to at most update one or
  // two methods in the sequence for their single-implementation status, in order to maintain
  // the invariant.

  if (!method_in_super->HasSingleImplementation()) {
    // method_in_super already has multiple implementations. All methods in the
    // same vtable slots in its super classes should have
    // non-single-implementation already.
    VerifyNonSingleImplementation(klass->GetSuperClass()->GetSuperClass(),
                                  method_in_super->GetMethodIndex(),
                                  nullptr /* excluded_method */);
    return;
  }

  uint16_t method_index = method_in_super->GetMethodIndex();
  if (method_in_super->IsAbstract()) {
    // An abstract method should have made all methods in the same vtable
    // slot above it in the class hierarchy having non-single-implementation.
    VerifyNonSingleImplementation(klass->GetSuperClass()->GetSuperClass(),
                                  method_index,
                                  method_in_super);

    if (virtual_method->IsAbstract()) {
      // SUPER: abstract, VIRTUAL: abstract.
      if (method_in_super == virtual_method) {
        DCHECK(klass->IsInstantiable());
        // An instantiable subclass hasn't provided a concrete implementation of
        // the abstract method. Invoking method_in_super may throw AbstractMethodError.
        // This is an uncommon case, so we simply treat method_in_super as not
        // having single-implementation.
        invalidated_single_impl_methods.insert(method_in_super);
        return;
      } else {
        // One abstract method overrides another abstract method. This is an uncommon
        // case. We simply treat method_in_super as not having single-implementation.
        invalidated_single_impl_methods.insert(method_in_super);
        return;
      }
    } else {
      // SUPER: abstract, VIRTUAL: non-abstract.
      // A non-abstract method overrides an abstract method.
      if (method_in_super->GetSingleImplementation(pointer_size) == nullptr) {
        // Abstract method_in_super has no implementation yet.
        // We need to grab cha_lock_ since there may be multiple class linking
        // going on that can check/modify the single-implementation flag/method
        // of method_in_super.
        MutexLock cha_mu(Thread::Current(), *Locks::cha_lock_);
        if (!method_in_super->HasSingleImplementation()) {
          return;
        }
        if (method_in_super->GetSingleImplementation(pointer_size) == nullptr) {
          // virtual_method becomes the first implementation for method_in_super.
          method_in_super->SetSingleImplementation(virtual_method, pointer_size);
          // Keep method_in_super's single-implementation status.
          return;
        }
        // Fall through to invalidate method_in_super's single-implementation status.
      }
      // Abstract method_in_super already got one implementation.
      // Invalidate method_in_super's single-implementation status.
      invalidated_single_impl_methods.insert(method_in_super);
      return;
    }
  } else {
    if (virtual_method->IsAbstract()) {
      // SUPER: non-abstract, VIRTUAL: abstract.
      // An abstract method overrides a non-abstract method. This is an uncommon
      // case, we simply treat both methods as not having single-implementation.
      invalidated_single_impl_methods.insert(virtual_method);
      // Fall-through to handle invalidating method_in_super of its
      // single-implementation status.
    }

    // SUPER: non-abstract, VIRTUAL: non-abstract/abstract(fall-through from previous if).
    // Invalidate method_in_super's single-implementation status.
    invalidated_single_impl_methods.insert(method_in_super);

    // method_in_super might be the single-implementation of another abstract method,
    // which should be also invalidated of its single-implementation status.
    mirror::Class* super_super = klass->GetSuperClass()->GetSuperClass();
    while (super_super != nullptr &&
           method_index < super_super->GetVTableLength()) {
      ArtMethod* method_in_super_super = super_super->GetVTableEntry(method_index, pointer_size);
      if (method_in_super_super != method_in_super) {
        if (method_in_super_super->IsAbstract()) {
          if (method_in_super_super->HasSingleImplementation()) {
            // Invalidate method_in_super's single-implementation status.
            invalidated_single_impl_methods.insert(method_in_super_super);
            // No need to further traverse up the class hierarchy since if there
            // are cases that one abstract method overrides another method, we
            // should have made that method having non-single-implementation already.
          } else {
            // method_in_super_super is already non-single-implementation.
            // No need to further traverse up the class hierarchy.
          }
        } else {
          DCHECK(!method_in_super_super->HasSingleImplementation());
          // No need to further traverse up the class hierarchy since two non-abstract
          // methods (method_in_super and method_in_super_super) should have set all
          // other methods (abstract or not) in the vtable slot to be non-single-implementation.
        }

        VerifyNonSingleImplementation(super_super->GetSuperClass(),
                                      method_index,
                                      method_in_super_super);
        // No need to go any further.
        return;
      } else {
        super_super = super_super->GetSuperClass();
      }
    }
  }
}

void ClassHierarchyAnalysis::CheckInterfaceMethodSingleImplementationInfo(
    Handle<mirror::Class> klass,
    ArtMethod* interface_method,
    ArtMethod* implementation_method,
    std::unordered_set<ArtMethod*>& invalidated_single_impl_methods,
    PointerSize pointer_size) {
  DCHECK(klass->IsInstantiable());
  DCHECK(interface_method->IsAbstract() || interface_method->IsDefault());

  if (!interface_method->HasSingleImplementation()) {
    return;
  }

  if (implementation_method->IsAbstract()) {
    // An instantiable class doesn't supply an implementation for
    // interface_method. Invoking the interface method on the class will throw
    // AbstractMethodError. This is an uncommon case, so we simply treat
    // interface_method as not having single-implementation.
    invalidated_single_impl_methods.insert(interface_method);
    return;
  }

  // We need to grab cha_lock_ since there may be multiple class linking going
  // on that can check/modify the single-implementation flag/method of
  // interface_method.
  MutexLock cha_mu(Thread::Current(), *Locks::cha_lock_);
  // Do this check again after we grab cha_lock_.
  if (!interface_method->HasSingleImplementation()) {
    return;
  }

  ArtMethod* single_impl = interface_method->GetSingleImplementation(pointer_size);
  if (single_impl == nullptr) {
    // implementation_method becomes the first implementation for
    // interface_method.
    interface_method->SetSingleImplementation(implementation_method, pointer_size);
    // Keep interface_method's single-implementation status.
    return;
  }
  DCHECK(!single_impl->IsAbstract());
  if (single_impl->GetDeclaringClass() == implementation_method->GetDeclaringClass()) {
    // Same implementation. Since implementation_method may be a copy of a default
    // method, we need to check the declaring class for equality.
    return;
  }
  // Another implementation for interface_method.
  invalidated_single_impl_methods.insert(interface_method);
}

void ClassHierarchyAnalysis::InitSingleImplementationFlag(Handle<mirror::Class> klass,
                                                          ArtMethod* method,
                                                          PointerSize pointer_size) {
  DCHECK(method->IsCopied() || method->GetDeclaringClass() == klass.Get());
  if (klass->IsFinal() || method->IsFinal()) {
    // Final classes or methods do not need CHA for devirtualization.
    // This frees up modifier bits for intrinsics which currently are only
    // used for static methods or methods of final classes.
    return;
  }
  if (method->IsAbstract()) {
    // single-implementation of abstract method shares the same field
    // that's used for JNI function of native method. It's fine since a method
    // cannot be both abstract and native.
    DCHECK(!method->IsNative()) << "Abstract method cannot be native";

    if (method->GetDeclaringClass()->IsInstantiable()) {
      // Rare case, but we do accept it (such as 800-smali/smali/b_26143249.smali).
      // Do not attempt to devirtualize it.
      method->SetHasSingleImplementation(false);
      DCHECK(method->GetSingleImplementation(pointer_size) == nullptr);
    } else {
      // Abstract method starts with single-implementation flag set and null
      // implementation method.
      method->SetHasSingleImplementation(true);
      DCHECK(method->GetSingleImplementation(pointer_size) == nullptr);
    }
  } else {
    method->SetHasSingleImplementation(true);
    // Single implementation of non-abstract method is itself.
    DCHECK_EQ(method->GetSingleImplementation(pointer_size), method);
  }
}

void ClassHierarchyAnalysis::UpdateAfterLoadingOf(Handle<mirror::Class> klass) {
  PointerSize image_pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();
  if (klass->IsInterface()) {
    for (ArtMethod& method : klass->GetDeclaredVirtualMethods(image_pointer_size)) {
      DCHECK(method.IsAbstract() || method.IsDefault());
      InitSingleImplementationFlag(klass, &method, image_pointer_size);
    }
    return;
  }

  mirror::Class* super_class = klass->GetSuperClass();
  if (super_class == nullptr) {
    return;
  }

  // Keeps track of all methods whose single-implementation assumption
  // is invalidated by linking `klass`.
  std::unordered_set<ArtMethod*> invalidated_single_impl_methods;

  // Do an entry-by-entry comparison of vtable contents with super's vtable.
  for (int32_t i = 0; i < super_class->GetVTableLength(); ++i) {
    ArtMethod* method = klass->GetVTableEntry(i, image_pointer_size);
    ArtMethod* method_in_super = super_class->GetVTableEntry(i, image_pointer_size);
    if (method == method_in_super) {
      // vtable slot entry is inherited from super class.
      if (method->IsAbstract() && klass->IsInstantiable()) {
        // An instantiable class that inherits an abstract method is treated as
        // supplying an implementation that throws AbstractMethodError.
        CheckVirtualMethodSingleImplementationInfo(klass,
                                                   method,
                                                   method_in_super,
                                                   invalidated_single_impl_methods,
                                                   image_pointer_size);
      }
      continue;
    }
    InitSingleImplementationFlag(klass, method, image_pointer_size);
    CheckVirtualMethodSingleImplementationInfo(klass,
                                               method,
                                               method_in_super,
                                               invalidated_single_impl_methods,
                                               image_pointer_size);
  }
  // For new virtual methods that don't override.
  for (int32_t i = super_class->GetVTableLength(); i < klass->GetVTableLength(); ++i) {
    ArtMethod* method = klass->GetVTableEntry(i, image_pointer_size);
    InitSingleImplementationFlag(klass, method, image_pointer_size);
  }

  if (klass->IsInstantiable()) {
    auto* iftable = klass->GetIfTable();
    const size_t ifcount = klass->GetIfTableCount();
    for (size_t i = 0; i < ifcount; ++i) {
      mirror::Class* interface = iftable->GetInterface(i);
      for (size_t j = 0, count = iftable->GetMethodArrayCount(i); j < count; ++j) {
        ArtMethod* interface_method = interface->GetVirtualMethod(j, image_pointer_size);
        mirror::PointerArray* method_array = iftable->GetMethodArray(i);
        ArtMethod* implementation_method =
            method_array->GetElementPtrSize<ArtMethod*>(j, image_pointer_size);
        DCHECK(implementation_method != nullptr) << klass->PrettyClass();
        CheckInterfaceMethodSingleImplementationInfo(klass,
                                                     interface_method,
                                                     implementation_method,
                                                     invalidated_single_impl_methods,
                                                     image_pointer_size);
      }
    }
  }

  InvalidateSingleImplementationMethods(invalidated_single_impl_methods);
}

void ClassHierarchyAnalysis::InvalidateSingleImplementationMethods(
    std::unordered_set<ArtMethod*>& invalidated_single_impl_methods) {
  if (!invalidated_single_impl_methods.empty()) {
    Runtime* const runtime = Runtime::Current();
    Thread *self = Thread::Current();
    // Method headers for compiled code to be invalidated.
    std::unordered_set<OatQuickMethodHeader*> dependent_method_headers;
    PointerSize image_pointer_size =
        Runtime::Current()->GetClassLinker()->GetImagePointerSize();

    {
      // We do this under cha_lock_. Committing code also grabs this lock to
      // make sure the code is only committed when all single-implementation
      // assumptions are still true.
      MutexLock cha_mu(self, *Locks::cha_lock_);
      // Invalidate compiled methods that assume some virtual calls have only
      // single implementations.
      for (ArtMethod* invalidated : invalidated_single_impl_methods) {
        if (!invalidated->HasSingleImplementation()) {
          // It might have been invalidated already when other class linking is
          // going on.
          continue;
        }
        invalidated->SetHasSingleImplementation(false);
        if (invalidated->IsAbstract()) {
          // Clear the single implementation method.
          invalidated->SetSingleImplementation(nullptr, image_pointer_size);
        }

        if (runtime->IsAotCompiler()) {
          // No need to invalidate any compiled code as the AotCompiler doesn't
          // run any code.
          continue;
        }

        // Invalidate all dependents.
        for (const auto& dependent : GetDependents(invalidated)) {
          ArtMethod* method = dependent.first;;
          OatQuickMethodHeader* method_header = dependent.second;
          VLOG(class_linker) << "CHA invalidated compiled code for " << method->PrettyMethod();
          DCHECK(runtime->UseJitCompilation());
          runtime->GetJit()->GetCodeCache()->InvalidateCompiledCodeFor(
              method, method_header);
          dependent_method_headers.insert(method_header);
        }
        RemoveAllDependenciesFor(invalidated);
      }
    }

    if (dependent_method_headers.empty()) {
      return;
    }
    // Deoptimze compiled code on stack that should have been invalidated.
    CHACheckpoint checkpoint(dependent_method_headers);
    size_t threads_running_checkpoint = runtime->GetThreadList()->RunCheckpoint(&checkpoint);
    if (threads_running_checkpoint != 0) {
      checkpoint.WaitForThreadsToRunThroughCheckpoint(threads_running_checkpoint);
    }
  }
}

void ClassHierarchyAnalysis::RemoveDependenciesForLinearAlloc(const LinearAlloc* linear_alloc) {
  MutexLock mu(Thread::Current(), *Locks::cha_lock_);
  for (auto it = cha_dependency_map_.begin(); it != cha_dependency_map_.end(); ) {
    // Use unsafe to avoid locking since the allocator is going to be deleted.
    if (linear_alloc->ContainsUnsafe(it->first)) {
      // About to delete the ArtMethod, erase the entry from the map.
      it = cha_dependency_map_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace art
