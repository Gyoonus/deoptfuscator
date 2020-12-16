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

#ifndef ART_COMPILER_OPTIMIZING_ESCAPE_H_
#define ART_COMPILER_OPTIMIZING_ESCAPE_H_

namespace art {

class HInstruction;

/*
 * Methods related to escape analysis, i.e. determining whether an object
 * allocation is visible outside ('escapes') its immediate method context.
 */

/*
 * Performs escape analysis on the given instruction, typically a reference to an
 * allocation. The method assigns true to parameter 'is_singleton' if the reference
 * is the only name that can refer to its value during the lifetime of the method,
 * meaning that the reference is not aliased with something else, is not stored to
 * heap memory, and not passed to another method. In addition, the method assigns
 * true to parameter 'is_singleton_and_not_returned' if the reference is a singleton
 * and not returned to the caller and to parameter 'is_singleton_and_not_deopt_visible'
 * if the reference is a singleton and not used as an environment local of an
 * HDeoptimize instruction (clients of the final value must run after BCE to ensure
 * all such instructions have been introduced already).
 *
 * Note that being visible to a HDeoptimize instruction does not count for ordinary
 * escape analysis, since switching between compiled code and interpreted code keeps
 * non escaping references restricted to the lifetime of the method and the thread
 * executing it. This property only concerns optimizations that are interested in
 * escape analysis with respect to the *compiled* code (such as LSE).
 *
 * When set, the no_escape function is applied to any use of the allocation instruction
 * prior to any built-in escape analysis. This allows clients to define better escape
 * analysis in certain case-specific circumstances. If 'no_escape(reference, user)'
 * returns true, the user is assumed *not* to cause any escape right away. The return
 * value false means the client cannot provide a definite answer and built-in escape
 * analysis is applied to the user instead.
 */
void CalculateEscape(HInstruction* reference,
                     bool (*no_escape)(HInstruction*, HInstruction*),
                     /*out*/ bool* is_singleton,
                     /*out*/ bool* is_singleton_and_not_returned,
                     /*out*/ bool* is_singleton_and_not_deopt_visible);

/*
 * Convenience method for testing the singleton and not returned properties at once.
 * Callers should be aware that this method invokes the full analysis at each call.
 */
bool DoesNotEscape(HInstruction* reference, bool (*no_escape)(HInstruction*, HInstruction*));

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_ESCAPE_H_
