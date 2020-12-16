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

#include "art_method-inl.h"
#include "callee_save_frame.h"
#include "common_throws.h"
#include "mirror/object-inl.h"
#include "thread.h"
#include "well_known_classes.h"

namespace art {

// Deliver an exception that's pending on thread helping set up a callee save frame on the way.
extern "C" NO_RETURN void artDeliverPendingExceptionFromCode(Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  self->QuickDeliverException();
}

extern "C" NO_RETURN uint64_t artInvokeObsoleteMethod(ArtMethod* method, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(method->IsObsolete());
  ScopedQuickEntrypointChecks sqec(self);
  ThrowInternalError("Attempting to invoke obsolete version of '%s'.",
                     method->PrettyMethod().c_str());
  self->QuickDeliverException();
}

// Called by generated code to throw an exception.
extern "C" NO_RETURN void artDeliverExceptionFromCode(mirror::Throwable* exception, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  /*
   * exception may be null, in which case this routine should
   * throw NPE.  NOTE: this is a convenience for generated code,
   * which previously did the null check inline and constructed
   * and threw a NPE if null.  This routine responsible for setting
   * exception_ in thread and delivering the exception.
   */
  ScopedQuickEntrypointChecks sqec(self);
  if (exception == nullptr) {
    self->ThrowNewException("Ljava/lang/NullPointerException;", "throw with null exception");
  } else {
    self->SetException(exception);
  }
  self->QuickDeliverException();
}

// Called by generated code to throw a NPE exception.
extern "C" NO_RETURN void artThrowNullPointerExceptionFromCode(Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  // We come from an explicit check in the generated code. This path is triggered
  // only if the object is indeed null.
  ThrowNullPointerExceptionFromDexPC(/* check_address */ false, 0U);
  self->QuickDeliverException();
}

// Installed by a signal handler to throw a NPE exception.
extern "C" NO_RETURN void artThrowNullPointerExceptionFromSignal(uintptr_t addr, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  ThrowNullPointerExceptionFromDexPC(/* check_address */ true, addr);
  self->QuickDeliverException();
}

// Called by generated code to throw an arithmetic divide by zero exception.
extern "C" NO_RETURN void artThrowDivZeroFromCode(Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  ThrowArithmeticExceptionDivideByZero();
  self->QuickDeliverException();
}

// Called by generated code to throw an array index out of bounds exception.
extern "C" NO_RETURN void artThrowArrayBoundsFromCode(int index, int length, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  ThrowArrayIndexOutOfBoundsException(index, length);
  self->QuickDeliverException();
}

// Called by generated code to throw a string index out of bounds exception.
extern "C" NO_RETURN void artThrowStringBoundsFromCode(int index, int length, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  ThrowStringIndexOutOfBoundsException(index, length);
  self->QuickDeliverException();
}

extern "C" NO_RETURN void artThrowStackOverflowFromCode(Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  ThrowStackOverflowError(self);
  self->QuickDeliverException();
}

extern "C" NO_RETURN void artThrowClassCastException(mirror::Class* dest_type,
                                                     mirror::Class* src_type,
                                                     Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  DCHECK(!dest_type->IsAssignableFrom(src_type));
  ThrowClassCastException(dest_type, src_type);
  self->QuickDeliverException();
}

extern "C" NO_RETURN void artThrowClassCastExceptionForObject(mirror::Object* obj,
                                                              mirror::Class* dest_type,
                                                              Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(obj != nullptr);
  artThrowClassCastException(dest_type, obj->GetClass(), self);
}

extern "C" NO_RETURN void artThrowArrayStoreException(mirror::Object* array, mirror::Object* value,
                                                      Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  ThrowArrayStoreException(value->GetClass(), array->GetClass());
  self->QuickDeliverException();
}

}  // namespace art
