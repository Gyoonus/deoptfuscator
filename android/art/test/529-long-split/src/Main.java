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

public class Main  {
  public static void main(String[] args) throws Exception {
    if (testOddLow1(5L)) {
      throw new Error();
    }

    if (testNonFollowingHigh(5)) {
      throw new Error();
    }

    if (testOddLow2()) {
      throw new Error();
    }
  }

  public static boolean testOddLow1(long a /* ECX-EDX */) {
    // class instance is in EBP
    long b = myLongField1; // ESI-EDI
    int f = myField1; // EBX
    int e = myField2; // EAX
    int g = myField3; // ESI (by spilling ESI-EDI, see below)
    int h = myField4; // EDI
    myLongField2 = a; // Make sure ESI-EDI gets spilled and not ECX-EDX
    myField2 = f; // use of EBX
    myField1 = e; // use of EAX
    myField3 = h; // use of ESI
    myField4 = g; // use if EDI

    // At this point `b` has been spilled and needs to have a pair. The ordering
    // in the register allocator triggers the allocation of `res` before `b`.
    // `res` being used after the `doCall`, we want a callee saved register.
    //
    // EBP is taken by the class instance and EDI is taken by `g` (both used in the `myField4`
    // assignment below). So we end up allocating ESI for `res`.
    //
    // When we try to allocate a pair for `b` we're in the following situation:
    // EAX is free
    // ECX is taken
    // EDX is taken
    // EBX is free
    // ESP is blocked
    // EBP could be spilled
    // ESI is taken
    // EDI could be spilled
    //
    // So there is no consecutive registers available to please the register allocator.
    // The compiler used to trip then because of a bogus implementation of trying to split
    // an unaligned register pair (here ECX and EDX). The implementation would not find
    // a register and the register allocator would then complain about not having
    // enough registers for the operation.
    boolean res = a == b;
    $noinline$doCall();
    myField4 = g;
    return res;
  }

  public static boolean testNonFollowingHigh(int i) {
    // class instance is in EBP
    long b = myLongField1; // ESI-EDI
    long a = (long)i; // EAX-EDX
    int f = myField1; // EBX
    int e = myField2; // ECX
    int g = myField3; // ESI (by spilling ESI-EDI, see below)
    int h = myField4; // EDI
    myLongField2 = a; // Make sure ESI-EDI gets spilled and not ECX-EDX
    myField2 = f; // use of EBX
    myField1 = e; // use of ECX
    myField3 = h; // use of EDI
    myField4 = g; // use of ESI

    // At this point `b` has been spilled and needs to have a pair. The ordering
    // in the register allocator triggers the allocation of `res` before `b`.
    // `res` being used after the `doCall`, we want a callee saved register.
    //
    // EBP is taken by the class instance and ESI is taken by `g` (both used in the `myField4`
    // assignment below). So we end up allocating EDI for `res`.
    //
    // When we try to allocate a pair for `b` we're in the following situation:
    // EAX is taken
    // ECX is free
    // EDX is taken
    // EBX is free
    // ESP is blocked
    // EBP could be spilled
    // ESI is taken
    // EDI could be spilled
    //
    // So there is no consecutive registers available to please the register allocator.
    // The compiler used to be in a bad state because of a bogus implementation of trying
    // to split an unaligned register pair (here EAX and EDX).
    boolean res = a == b;
    $noinline$doCall();
    myField4 = g;
    return res;
  }

  public static boolean testOddLow2() {
    // class instance is in EBP
    long b = myLongField1; // ECX-EDX (hint due to call below).
    long a = myLongField2; // ESI-EDI
    int f = myField1; // EBX
    int e = myField2; // EAX
    int g = myField3; // ECX
    int h = myField4; // EDX
    int i = myField5; // ESI - callee saved due to assignment after call to $noinline$doCall.
    myField2 = f; // use of EBX
    myField1 = e; // use of EAX
    myField3 = h; // use of EDX
    myField4 = i; // use of ESI
    myField5 = g; // use of ECX

    // At this point `a` and `b` have been spilled and need to have a pairs. The ordering
    // in the register allocator triggers the allocation of `res` before `a` and `b`.
    // `res` being used after the `doCall`, we want a callee saved register.
    //
    // EBP is taken by the class instance and ESI is taken by `i` (both used in the `myField4`
    // assignment below). So we end up allocating EDI for `res`.
    //
    // We first try to allocator a pair for `b`. We're in the following situation:
    // EAX is free
    // ECX is free
    // EDX is free
    // EBX is free
    // ESP is blocked
    // EBP could be spilled
    // ESI could be spilled
    // EDI is taken
    //
    // Because `b` is used as a first argument to a call, we take its hint and allocate
    // ECX-EDX to it.
    //
    // We then try to allocate a pair for `a`. We're in the following situation:
    // EAX is free
    // ECX could be spilled
    // EDX could be spilled
    // EBX is free
    // ESP is blocked
    // EBP could be spilled
    // ESI could be spilled
    // EDI is taken
    //
    // So no consecutive two free registers are available. When trying to find a slot, we pick
    // the first unaligned or non-pair interval. In this case, this is the unaligned ECX-EDX.
    // The compiler used to then trip because it forgot to remove the high interval containing
    // the pair from the active list.

    boolean res = a == b;
    $noinline$doCall(b);
    myField4 = i; // use of ESI
    return res;
  }

  public static void $noinline$doCall() {
    if (doThrow) throw new Error();
  }

  public static void $noinline$doCall(long e) {
    if (doThrow) throw new Error();
  }

  public static boolean doThrow;
  public static int myField1;
  public static int myField2;
  public static int myField3;
  public static int myField4;
  public static int myField5;
  public static long myLongField1;
  public static long myLongField2;
}
