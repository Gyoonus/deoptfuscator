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
  public static void main(String[] args) {
    Exception exception = null;
    try {
      $opt$Throw(new int[1]);
    } catch (ArrayIndexOutOfBoundsException e) {
      exception = e;
    }

    String exceptionMessage = exception.getMessage();

    // Note that it's ART specific to emit the length.
    if (exceptionMessage.contains("length")) {
      if (!exceptionMessage.contains("length=1")) {
        throw new Error("Wrong length in exception message");
      }
    }

    // Note that it's ART specific to emit the index.
    if (exceptionMessage.contains("index")) {
      if (!exceptionMessage.contains("index=2")) {
        throw new Error("Wrong index in exception message");
      }
    }
  }

  static void $opt$Throw(int[] array) {
    // We fetch the length first, to ensure it is in EAX (on x86).
    // The pThrowArrayBounds entrypoint expects the index in EAX and the
    // length in ECX, and the optimizing compiler used to write to EAX
    // before putting the length in ECX.
    int length = array.length;
    array[2] = 42;
  }
}
