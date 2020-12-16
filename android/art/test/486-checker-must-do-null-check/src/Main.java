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

public class Main {
  /// CHECK-START: void Main.InstanceOfPreChecked(java.lang.Object) instruction_simplifier (after)
  /// CHECK:       InstanceOf must_do_null_check:false
  public void InstanceOfPreChecked(Object o) throws Exception {
    o.toString();
    if (o instanceof Main) {
      throw new Exception();
    }
  }

  /// CHECK-START: void Main.InstanceOf(java.lang.Object) instruction_simplifier (after)
  /// CHECK:       InstanceOf must_do_null_check:true
  public void InstanceOf(Object o) throws Exception {
    if (o instanceof Main) {
      throw new Exception();
    }
  }

  /// CHECK-START: void Main.CheckCastPreChecked(java.lang.Object) instruction_simplifier (after)
  /// CHECK:       CheckCast must_do_null_check:false
  public void CheckCastPreChecked(Object o) {
    o.toString();
    ((Main)o).$noinline$Bar();
  }

  /// CHECK-START: void Main.CheckCast(java.lang.Object) instruction_simplifier (after)
  /// CHECK:       CheckCast must_do_null_check:true
  public void CheckCast(Object o) {
    ((Main)o).$noinline$Bar();
  }

  void $noinline$Bar() {throw new RuntimeException();}

  public static void main(String[] sa) {
    Main t = new Main();
  }
}
