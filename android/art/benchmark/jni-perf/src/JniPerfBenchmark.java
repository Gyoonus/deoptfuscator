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

public class JniPerfBenchmark {
  private static final String MSG = "ABCDE";

  native void perfJniEmptyCall();
  native void perfSOACall();
  native void perfSOAUncheckedCall();

  public void timeFastJNI(int N) {
    // TODO: This might be an intrinsic.
    for (long i = 0; i < N; i++) {
      char c = MSG.charAt(2);
    }
  }

  public void timeEmptyCall(int N) {
    for (long i = 0; i < N; i++) {
      perfJniEmptyCall();
    }
  }

  public void timeSOACall(int N) {
    for (long i = 0; i < N; i++) {
      perfSOACall();
    }
  }

  public void timeSOAUncheckedCall(int N) {
    for (long i = 0; i < N; i++) {
      perfSOAUncheckedCall();
    }
  }

  {
    System.loadLibrary("artbenchmark");
  }
}
