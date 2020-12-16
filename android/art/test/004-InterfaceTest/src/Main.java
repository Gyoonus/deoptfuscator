/*
 * Copyright (C) 2011 The Android Open Source Project
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

import java.util.Map;
import java.util.HashMap;

public class Main {

  public static long test_virtual(HashMap map) {
    Integer intobj = new Integer(0);
    String s = "asdf";
    long start = System.currentTimeMillis();
    for (int i = 0; i < 10000; i++) {
        map.put(intobj, s);
    }
    long end = System.currentTimeMillis();
    return (end - start);
  }

  public static long test_interface(Map map) {
    Integer intobj = new Integer(0);
    String s = "asdf";
    long start = System.currentTimeMillis();
    for (int i = 0; i < 10000; i++) {
        map.put(intobj, s);
    }
    long end = System.currentTimeMillis();
    return (end - start);
  }

  public static void main(String[] args) {
    HashMap hashmap = new HashMap();
    long elapsed = test_virtual(hashmap);
    System.out.println("test_virtual done");
    hashmap.clear();

    elapsed = test_interface(hashmap);
    System.out.println("test_interface done");
  }
}
