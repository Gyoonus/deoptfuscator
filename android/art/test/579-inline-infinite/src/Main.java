/*
 * Copyright (C) 2016 The Android Open Source Project
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

class Infinite implements Runnable {
  public int field;

  private final void $noinline$infinite() {
    while(true) {
      field++;
    }
  }

  public void run() {
    $noinline$infinite();
  }
}

public class Main {
  public static void main(String[] args) {
    Thread thr = new Thread(new Infinite());
    thr.setDaemon(true);
    thr.start();
    // This is a compiler test, so just finish.
  }
}
