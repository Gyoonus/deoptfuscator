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

package art;

import java.lang.reflect.Field;
import java.util.Arrays;

public class Test920 {
  public static void run() throws Exception {
    doTest();
  }

  public static void doTest() throws Exception {
    testObjectSize(new Object());
    testObjectSize(new Object());

    testObjectSize(new int[0]);
    testObjectSize(new int[1]);
    testObjectSize(new int[2]);

    testObjectSize(new double[0]);
    testObjectSize(new double[1]);
    testObjectSize(new double[2]);

    testObjectSize(new String("abc"));
    testObjectSize(new String("wxyz"));

    testObjectHash();
  }

  private static void testObjectSize(Object o) {
    System.out.println(o.getClass() + " " + getObjectSize(o));
  }

  private static void testObjectHash() {
    Object[] objects = new Object[] {
        new Object(),
        new Object(),

        new MyHash(1),
        new MyHash(1),
        new MyHash(2)
    };

    int hashes[] = new int[objects.length];

    for (int i = 0; i < objects.length; i++) {
      hashes[i] = getObjectHashCode(objects[i]);
    }

    // Implementation detail: we use the identity hashcode, for simplicity.
    for (int i = 0; i < objects.length; i++) {
      int ihash = System.identityHashCode(objects[i]);
      if (hashes[i] != ihash) {
        throw new RuntimeException(objects[i] + ": " + hashes[i] + " vs " + ihash);
      }
    }

    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    for (int i = 0; i < objects.length; i++) {
      int newhash = getObjectHashCode(objects[i]);
      if (hashes[i] != newhash) {
        throw new RuntimeException(objects[i] + ": " + hashes[i] + " vs " + newhash);
      }
    }
  }

  private static native long getObjectSize(Object o);
  private static native int getObjectHashCode(Object o);

  private static class MyHash {
    private int hash;

    public MyHash(int h) {
      hash = h;
    }

    public int hashCode() {
      return hash;
    }
  }
}
