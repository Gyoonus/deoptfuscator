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

public class Main {
  static boolean doThrow = false;

  // Note: We're not doing checker tests as we cannot do them specifically for a non-PIC
  // configuration. The check here would be "prepare_for_register_allocation (before)"
  //     CHECK:         LoadClass
  //     CHECK-NEXT:    ClinitCheck
  //     CHECK-NEXT:    LoadString load_kind:BootImageAddress
  //     CHECK-NEXT:    NewInstance
  // and "prepare_for_register_allocation (after)"
  //     CHECK:         LoadString
  //     CHECK-NEXT:    NewInstance
  // but the order of instructions for non-PIC mode is different.
  public static int $noinline$test() {
    if (doThrow) { throw new Error(); }

    int r = 0x12345678;
    do {
      // LICM pulls the LoadClass and ClinitCheck out of the loop, leaves NewInstance in the loop.
      Helper h = new Helper();
      // For non-PIC mode, LICM pulls the boot image LoadString out of the loop.
      // (For PIC mode, the LoadString can throw and will not be moved out of the loop.)
      String s = "";  // Empty string is known to be in the boot image.
      r = r ^ (r >> 5);
      h.$noinline$printString(s);
      // During DCE after inlining, the loop back-edge disappears and the pre-header is
      // merged with the body, leaving consecutive LoadClass, ClinitCheck, LoadString
      // and NewInstance in non-PIC mode. The prepare_for_register_allocation pass
      // merges the LoadClass and ClinitCheck with the NewInstance and checks that
      // there are no instructions with side effects in between. This check used to
      // fail because LoadString was always listing SideEffects::CanTriggerGC() even
      // when it doesn't really have any side effects, i.e. for direct references to
      // boot image Strings or for Strings known to be in the dex cache.
    } while ($inline$shouldContinue());
    return r;
  }

  static boolean $inline$shouldContinue() {
    return false;
  }

  public static void main(String[] args) {
    assertIntEquals(0x12345678 ^ (0x12345678 >> 5), $noinline$test());
  }

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}

class Helper {
  static boolean doThrow = false;

  public void $noinline$printString(String s) {
    if (doThrow) { throw new Error(); }

    System.out.println("String: \"" + s + "\"");
  }
}
