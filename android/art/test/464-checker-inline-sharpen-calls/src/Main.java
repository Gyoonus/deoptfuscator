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

public final class Main {

  public final static class Helper {
    private int foo = 3;

    public int getFoo() {
        return foo;
    }
  }

  public void invokeVirtual() {
  }

  /// CHECK-START: void Main.inlineSharpenInvokeVirtual(Main) builder (after)
  /// CHECK-DAG:     <<Invoke:v\d+>>  InvokeVirtual
  /// CHECK-DAG:                      ReturnVoid

  /// CHECK-START: void Main.inlineSharpenInvokeVirtual(Main) inliner (after)
  /// CHECK-NOT:                      InvokeVirtual
  /// CHECK-NOT:                      InvokeStaticOrDirect

  public static void inlineSharpenInvokeVirtual(Main m) {
    m.invokeVirtual();
  }

  /// CHECK-START: int Main.inlineSharpenHelperInvoke() builder (after)
  /// CHECK-DAG:     <<Invoke:i\d+>>  InvokeVirtual {{.*\.getFoo.*}}
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: int Main.inlineSharpenHelperInvoke() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect {{.*\.getFoo.*}}
  /// CHECK-NOT:                      InvokeVirtual {{.*\.getFoo.*}}

  /// CHECK-START: int Main.inlineSharpenHelperInvoke() inliner (after)
  /// CHECK-DAG:     <<Field:i\d+>>   InstanceFieldGet
  /// CHECK-DAG:                      Return [<<Field>>]

  public static int inlineSharpenHelperInvoke() {
    return new Helper().getFoo();
  }

  public static void main(String[] args) {
    inlineSharpenInvokeVirtual(new Main());
    if (inlineSharpenHelperInvoke() != 3) {
      throw new Error("Expected 3");
    }
  }
}
