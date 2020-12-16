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

  /// CHECK-START: java.lang.Object Main.boundTypeForIf(java.lang.Object) builder (after)
  /// CHECK:     BoundType
  public static Object boundTypeForIf(Object a) {
    if (a != null) {
      return a.toString();
    } else {
      return null;
    }
  }

  /// CHECK-START: java.lang.Object Main.boundTypeForInstanceOf(java.lang.Object) builder (after)
  /// CHECK:     BoundType
  public static Object boundTypeForInstanceOf(Object a) {
    if (a instanceof Main) {
      return (Main)a;
    } else {
      return null;
    }
  }

  /// CHECK-START: java.lang.Object Main.noBoundTypeForIf(java.lang.Object) builder (after)
  /// CHECK-NOT: BoundType
  public static Object noBoundTypeForIf(Object a) {
    if (a == null) {
      return new Object();
    } else {
      return null;
    }
  }

  /// CHECK-START: java.lang.Object Main.noBoundTypeForInstanceOf(java.lang.Object) builder (after)
  /// CHECK-NOT: BoundType
  public static Object noBoundTypeForInstanceOf(Object a) {
    if (a instanceof Main) {
      return new Object();
    } else {
      return null;
    }
  }

  public static void main(String[] args) {  }
}
