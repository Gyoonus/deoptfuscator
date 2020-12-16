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

class SuperClass {
}

class ChildClass extends SuperClass {
}

public class Main {

  public static void main(String[] args) {
    test1();
    test2();
  }

  /// CHECK-START:    void Main.test1() builder (after)
  /// CHECK:          BoundType  klass:SuperClass can_be_null:false exact:false

  /// CHECK-START:    void Main.test1() builder (after)
  /// CHECK-NOT:      BoundType  klass:SuperClass can_be_null:false exact:true
  public static void test1() {
    Object obj = new ChildClass();

    // We need a fixed point iteration to hit the bogus type update
    // of 'obj' below, so create a loop that updates the type of 'obj'.
    for (int i = 1; i < 1; i++) {
      obj = new Object();
    }

    if (obj instanceof SuperClass) {
      // We used to wrongly type obj as an exact SuperClass from this point,
      // meaning we were statically determining that the following instanceof
      // would always fail.
      if (!(obj instanceof ChildClass)) {
        throw new Error("Expected a ChildClass, got " + obj.getClass());
      }
    }
  }

  /// CHECK-START-X86: boolean Main.$noinline$instanceOfString(java.lang.Object) disassembly (after)
  /// CHECK:          InstanceOf check_kind:exact_check
  /// CHECK-NOT:      {{.*fs:.*}}

  /// CHECK-START-X86_64: boolean Main.$noinline$instanceOfString(java.lang.Object) disassembly (after)
  /// CHECK:          InstanceOf check_kind:exact_check
  /// CHECK-NOT:      {{.*gs:.*}}

  /// CHECK-START-{ARM,ARM64,MIPS,MIPS64}: boolean Main.$noinline$instanceOfString(java.lang.Object) disassembly (after)
  /// CHECK:          InstanceOf check_kind:exact_check
  // For ARM and ARM64, the marking register (r8 and x20, respectively) can be used in
  // non-CC configs for any other purpose, so we'd need a config-specific checker test.
  // TODO: Add the checks when we support config-specific tests.
  public static boolean $noinline$instanceOfString(Object o) {
    // String is a final class, so `instanceof String` should use exact check.
    // String is in the boot image, so we should avoid read barriers. The presence
    // of the read barrier can be checked in the architecture-specific disassembly.
    return o instanceof String;
  }

  public static void test2() {
    if ($noinline$instanceOfString(new Object())) {
      throw new Error();
    }
    if (!$noinline$instanceOfString(new String())) {
      throw new Error();
    }
  }
}
