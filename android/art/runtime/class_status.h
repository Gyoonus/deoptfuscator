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

#ifndef ART_RUNTIME_CLASS_STATUS_H_
#define ART_RUNTIME_CLASS_STATUS_H_

#include <iosfwd>
#include <stdint.h>

namespace art {

// Class Status
//
// kRetired: Class that's temporarily used till class linking time
// has its (vtable) size figured out and has been cloned to one with the
// right size which will be the one used later. The old one is retired and
// will be gc'ed once all refs to the class point to the newly
// cloned version.
//
// kErrorUnresolved, kErrorResolved: Class is erroneous. We need
// to distinguish between classes that have been resolved and classes that
// have not. This is important because the const-class instruction needs to
// return a previously resolved class even if its subsequent initialization
// failed. We also need this to decide whether to wrap a previous
// initialization failure in ClassDefNotFound error or not.
//
// kNotReady: If a Class cannot be found in the class table by
// FindClass, it allocates an new one with AllocClass in the
// kNotReady and calls LoadClass. Note if it does find a
// class, it may not be kResolved and it will try to push it
// forward toward kResolved.
//
// kIdx: LoadClass populates with Class with information from
// the DexFile, moving the status to kIdx, indicating that the
// Class value in super_class_ has not been populated. The new Class
// can then be inserted into the classes table.
//
// kLoaded: After taking a lock on Class, the ClassLinker will
// attempt to move a kIdx class forward to kLoaded by
// using ResolveClass to initialize the super_class_ and ensuring the
// interfaces are resolved.
//
// kResolving: Class is just cloned with the right size from
// temporary class that's acting as a placeholder for linking. The old
// class will be retired. New class is set to this status first before
// moving on to being resolved.
//
// kResolved: Still holding the lock on Class, the ClassLinker
// shows linking is complete and fields of the Class populated by making
// it kResolved. Java allows circularities of the form where a super
// class has a field that is of the type of the sub class. We need to be able
// to fully resolve super classes while resolving types for fields.
//
// kRetryVerificationAtRuntime: The verifier sets a class to
// this state if it encounters a soft failure at compile time. This
// often happens when there are unresolved classes in other dex
// files, and this status marks a class as needing to be verified
// again at runtime.
//
// TODO: Explain the other states
enum class ClassStatus : uint8_t {
  kNotReady = 0,  // Zero-initialized Class object starts in this state.
  kRetired = 1,  // Retired, should not be used. Use the newly cloned one instead.
  kErrorResolved = 2,
  kErrorUnresolved = 3,
  kIdx = 4,  // Loaded, DEX idx in super_class_type_idx_ and interfaces_type_idx_.
  kLoaded = 5,  // DEX idx values resolved.
  kResolving = 6,  // Just cloned from temporary class object.
  kResolved = 7,  // Part of linking.
  kVerifying = 8,  // In the process of being verified.
  kRetryVerificationAtRuntime = 9,  // Compile time verification failed, retry at runtime.
  kVerifyingAtRuntime = 10,  // Retrying verification at runtime.
  kVerified = 11,  // Logically part of linking; done pre-init.
  kSuperclassValidated = 12,  // Superclass validation part of init done.
  kInitializing = 13,  // Class init in progress.
  kInitialized = 14,  // Ready to go.
  kLast = kInitialized
};

std::ostream& operator<<(std::ostream& os, const ClassStatus& rhs);

}  // namespace art

#endif  // ART_RUNTIME_CLASS_STATUS_H_
