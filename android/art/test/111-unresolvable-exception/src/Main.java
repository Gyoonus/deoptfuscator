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

public class Main {
    static public void main(String[] args) throws Exception {
      try {
        check(false);
      } catch (Throwable t) {          // Should catch the NoClassDefFoundError
        System.out.println("Caught " + t.getClass());
      }
    }

    private static void check(boolean b) {
      try {
        if (b) {                   // Need this to not be dead code, but also not be invoked.
          throwsTestException();   // TestException is checked, so we need something potentially
                                   // throwing it.
        }
        throw new RuntimeException();  // Trigger exception handling.
      } catch (TestException e) {      // This handler will have an unresolvable class.
      } catch (Exception e) {          // General-purpose handler
        System.out.println("Got expected exception.");
      }
    }

    // This avoids having to construct one explicitly, which won't work.
    private static native void throwsTestException() throws TestException;
}
