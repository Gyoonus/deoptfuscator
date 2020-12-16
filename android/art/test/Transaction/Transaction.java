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

public class Transaction {
    static class EmptyStatic {
    }

    static class ResolveString {
      static String s = "ResolvedString";
    }

    static class StaticFieldClass {
      public static int intField;
      static {
        intField = 5;
      }
    }

    static class FinalizableAbortClass {
      public static AbortHelperClass finalizableObject;
      static {
        finalizableObject = new AbortHelperClass();
      }
    }

    static class NativeCallAbortClass {
      static {
        AbortHelperClass.nativeMethod();
      }
    }

    static class SynchronizedNativeCallAbortClass {
      static {
        synchronized (SynchronizedNativeCallAbortClass.class) {
          AbortHelperClass.nativeMethod();
        }
      }
    }

    static class CatchNativeCallAbortClass {
      static {
        try {
          AbortHelperClass.nativeMethod();
        } catch (Throwable e) {
          // ignore exception.
        }
      }
    }

    static class MultipleNativeCallAbortClass {
      static {
        // Call native method but catch the transaction exception.
        try {
          AbortHelperClass.nativeMethod();
        } catch (Throwable e) {
          // ignore exception.
        }

        // Call another native method.
        AbortHelperClass.nativeMethod2();
      }
    }

    // Helper class to abort transaction: finalizable class with natve methods.
    static class AbortHelperClass {
      public void finalize() throws Throwable {
        super.finalize();
      }
      public static native void nativeMethod();
      public static native void nativeMethod2();
    }
}
