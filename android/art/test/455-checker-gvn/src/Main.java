/*
 * Copyright (C) 2014 The Android Open Source Project
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
import java.lang.reflect.Method;

public class Main {

  private static int mX = 2;
  private static int mY = -3;

  public static void main(String[] args) {
    System.out.println(foo(3, 4));
    System.out.println(mulAndIntrinsic());
    System.out.println(directIntrinsic(-5));
  }

  public static int foo(int x, int y) {
   try {
      Class<?> c = Class.forName("Smali");
      Method m = c.getMethod("foo", int.class, int.class);
      return (Integer) m.invoke(null, x, y);
    } catch (Throwable t) {
      throw new RuntimeException(t);
    }
  }

  /// CHECK-START: int Main.mulAndIntrinsic() GVN (before)
  /// CHECK: StaticFieldGet
  /// CHECK: StaticFieldGet
  /// CHECK: Mul
  /// CHECK: InvokeStaticOrDirect
  /// CHECK: StaticFieldGet
  /// CHECK: StaticFieldGet
  /// CHECK: Mul
  /// CHECK: Add

  /// CHECK-START: int Main.mulAndIntrinsic() GVN (after)
  /// CHECK: StaticFieldGet
  /// CHECK: StaticFieldGet
  /// CHECK: Mul
  /// CHECK: InvokeStaticOrDirect
  /// CHECK-NOT: StaticFieldGet
  /// CHECK-NOT: StaticFieldGet
  /// CHECK-NOT: Mul
  /// CHECK: Add

  public static int mulAndIntrinsic() {
    // The intermediate call to abs() does not kill
    // the common subexpression on the multiplication.
    int mul1 = mX * mY;
    int abs  = Math.abs(mul1);
    int mul2 = mY * mX;
    return abs + mul2;
  }

  /// CHECK-START: int Main.directIntrinsic(int) GVN (before)
  /// CHECK: InvokeStaticOrDirect
  /// CHECK: InvokeStaticOrDirect
  /// CHECK: Add

  /// CHECK-START: int Main.directIntrinsic(int) GVN (after)
  /// CHECK: InvokeStaticOrDirect
  /// CHECK-NOT: InvokeStaticOrDirect
  /// CHECK: Add

  public static int directIntrinsic(int x) {
    // Here, the two calls to abs() themselves can be replaced with just one.
    int abs1 = Math.abs(x);
    int abs2 = Math.abs(x);
    return abs1 + abs2;
  }

}
