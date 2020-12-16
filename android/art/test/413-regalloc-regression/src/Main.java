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
  private Object[] data;
  private int size;

  public Main() {
    data = new Object[4];
    size = 0;
  }

  public void removeElementAt(int index) {
    for (int i = index; i < size - 1; i++) {
      data[i] = data[i + 1];
    }
    data[--size] = null;
  }

  public static void main(String[] args) {
    Main main = new Main();
    main.size++;
    main.removeElementAt(0);
    if (main.size != 0) {
      throw new Error("Unexpected size");
    }
  }
}
