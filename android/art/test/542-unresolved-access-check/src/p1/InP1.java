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

package p1;

public class InP1 {
    public static Object $inline$AllocateOtherInP1(int i) {
      // Let this method execute a while to make sure the JIT sees it hot.
      if (i <= 10000) {
        return null;
      }
      // Set the flag that we have entered InP1 code to get OtherInP1 loaded.
      PlaceHolder.entered = true;
      return new OtherInP1();
    }

    public static Object $inline$AllocateArrayOtherInP1(int i) {
      if (i <= 10000) {
        return null;
      }
      return new OtherInP1[10];
    }

    public static Object $inline$UseStaticFieldOtherInP1(int i) {
      if (i <= 10000) {
        return null;
      }
      return OtherInP1.staticField;
    }

    public static void $inline$SetStaticFieldOtherInP1(int i) {
      if (i <= 10000) {
        return;
      }
      OtherInP1.staticField = new Object();
    }

    public static Object $inline$UseInstanceFieldOtherInP1(int i) {
      if (i <= 10000) {
        return null;
      }
      return $noinline$AllocateOtherInP1().instanceField;
    }

    public static void $inline$SetInstanceFieldOtherInP1(int i) {
      if (i <= 10000) {
        return;
      }
      $noinline$AllocateOtherInP1().instanceField = new Object();
    }

    public static OtherInP1 $noinline$AllocateOtherInP1() {
      try {
        return new OtherInP1();
      } catch (Exception e) {
        throw new Error(e);
      }
    }

    public static Object $inline$LoadOtherInP1(int i) {
      if (i <= 10000) {
        return null;
      }
      return OtherInP1.class;
    }

    public static Object $inline$StaticCallOtherInP1(int i) {
      if (i <= 10000) {
        return null;
      }
      return OtherInP1.doTheStaticCall();
    }

    public static Object $inline$InstanceCallOtherInP1(int i) {
      if (i <= 10000) {
        return null;
      }
      return $noinline$AllocateOtherInP1().doTheInstanceCall();
    }
}
