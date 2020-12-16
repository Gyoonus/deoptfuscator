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

interface Itf {
}

class Foo {
}

class Main extends Foo implements Itf {
  public static void main(String[] args) {
    Itf parent = getParent();
    if (!(parent instanceof Foo)) {
      throw new Error("Instanceof should have succeeded");
    }
  }

  static Itf getParent() { return new Main(); }
}
