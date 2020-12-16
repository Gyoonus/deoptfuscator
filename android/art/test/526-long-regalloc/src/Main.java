/*
 * Copyright (C) 2015 The Android Open Source Project
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

class Main {
  public static void main(String[] args) {
    foo();
  }

  public static void foo() {
    int a = myField1; // esi
    int b = myField2; // edi
    $noinline$bar(); // makes allocation of a and b to be callee-save registers
    int c = myField3; // ecx
    int e = myField4; // ebx
    int f = myField5; // edx
    long d = a == 42 ? myLongField1 : 42L; // Will call AllocateBlockedReg -> edx/ebx

    // At this point, the register allocator used to be in a bogus state, where the low
    // part of the interval was in the active set, but not the high part.

    long i = myLongField1; // Will call TrySplitNonPairOrUnalignedPairIntervalAt -> Failing DCHECK

    // Use esi and edi first to not have d allocated to them.
    myField2 = a;
    myField3 = b;

    // The following sequence of instructions are making the AllocateBlockedReg call
    // for allocating the d variable misbehave: allocation of the low interval would split
    // both low and high interval at the fixed use; therefore the allocation of the high interval
    // would not see the register use, and think the interval can just be spilled and not be
    // put in the active set, even though it is holding a register.
    myField1 = (int)d; // stack use
    myLongField3 = (long) myField2; // edx fixed use
    myLongField2 = d; // register use

    // Ensure the HInstruction mapping to i, c, e, and f have a live range.
    myLongField1 = i;
    myField4 = c;
    myField5 = e;
    myField6 = f;
  }

  public static long $noinline$bar() {
    if (doThrow) throw new Error();
    return 42;
  }

  public static boolean doThrow = false;

  public static int myField1 = 0;
  public static int myField2 = 0;
  public static int myField3 = 0;
  public static int myField4 = 0;
  public static int myField5 = 0;
  public static int myField6 = 0;
  public static long myLongField1 = 0L;
  public static long myLongField2 = 0L;
  public static long myLongField3 = 0L;
}
