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

public class Main {
  public static void main(String[] args) {
    Class klass = Other.class;
    Object[] data = new Object[100000];
    try {
        System.out.println("Filling heap");
        int size = 256 * 1024 * 1024;
        int index = 0;
        while (true) {
            try {
                data[index] = new byte[size];
                index++;
            } catch (OutOfMemoryError e) {
                size /= 2;
                if (size == 0) {
                    break;
                }
            }
        }
        // Initialize now that the heap is full.
        Other.print();
    } catch (OutOfMemoryError e) {
    } catch (Exception e) {
        System.out.println(e);
    }
  }
}
