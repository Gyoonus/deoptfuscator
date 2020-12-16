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

public class JObjectBenchmark {
  public JObjectBenchmark() {
    // Make sure to link methods before benchmark starts.
    System.loadLibrary("artbenchmark");
    timeAddRemoveLocal(1);
    timeDecodeLocal(1);
    timeAddRemoveGlobal(1);
    timeDecodeGlobal(1);
    timeAddRemoveWeakGlobal(1);
    timeDecodeWeakGlobal(1);
    timeDecodeHandleScopeRef(1);
  }

  public native void timeAddRemoveLocal(int reps);
  public native void timeDecodeLocal(int reps);
  public native void timeAddRemoveGlobal(int reps);
  public native void timeDecodeGlobal(int reps);
  public native void timeAddRemoveWeakGlobal(int reps);
  public native void timeDecodeWeakGlobal(int reps);
  public native void timeDecodeHandleScopeRef(int reps);
}
