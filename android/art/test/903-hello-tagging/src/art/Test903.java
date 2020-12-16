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

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;

public class Test903 {
  public static void run() {
    doTest();
    testGetTaggedObjects();
    testTags();
  }

  public static void doTest() {
    WeakReference<Object> weak = test();

    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    if (weak.get() != null) {
      throw new RuntimeException("WeakReference not cleared");
    }
  }

  public static void testTags() {
    Object o = new Object();
    long[] res = testTagsInDifferentEnvs(o, 100, 10);
    System.out.println(Arrays.toString(res));
  }

  private static WeakReference<Object> test() {
    Object o1 = new Object();
    Main.setTag(o1, 1);

    Object o2 = new Object();
    Main.setTag(o2, 2);

    checkTag(o1, 1);
    checkTag(o2, 2);

    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    checkTag(o1, 1);
    checkTag(o2, 2);

    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    Main.setTag(o1, 10);
    Main.setTag(o2, 20);

    checkTag(o1, 10);
    checkTag(o2, 20);

    return new WeakReference<Object>(o1);
  }

  private static void checkTag(Object o, long expectedTag) {
    long tag = Main.getTag(o);
    if (expectedTag != tag) {
      throw new RuntimeException("Unexpected tag " + tag + ", expected " + expectedTag);
    }
  }

  private static void testGetTaggedObjects() {
    // Use an array list to ensure that the objects stay live for a bit. Also gives us a source
    // to compare to. We use index % 10 as the tag.
    ArrayList<Object> l = new ArrayList<>();

    for (int i = 0; i < 20; i++) {
      Integer o = new Integer(i);
      l.add(o);
      if (i % 10 != 0) {
        Main.setTag(o, i % 10);
      }
    }

    testGetTaggedObjectsRun(l, null, false, false);
    testGetTaggedObjectsRun(l, null, true, true);
    testGetTaggedObjectsRun(l, new long[] { 2, 5 }, true, true);
    testGetTaggedObjectsRun(l, null, false, true);
    testGetTaggedObjectsRun(l, null, true, false);
  }

  private static void testGetTaggedObjectsRun(ArrayList<Object> l, long[] searchTags,
      boolean returnObjects, boolean returnTags) {
    Object[] result = getTaggedObjects(searchTags, returnObjects, returnTags);

    Object[] objects = (Object[])result[0];
    long[] tags = (long[])result[1];
    int count = (int)result[2];

    System.out.println(count);
    printArraysSorted(objects, tags);
  }

  private static void printArraysSorted(Object[] objects, long[] tags) {
    if (objects == null && tags == null) {
      System.out.println("<nothing>");
      return;
    }

    int l1 = objects == null ? 0 : objects.length;
    int l2 = tags == null ? 0 : tags.length;
    int l = Math.max(l1, l2);
    Pair[] tmp = new Pair[l];
    for (int i = 0; i < l; i++) {
      tmp[i] = new Pair(objects == null ? null : objects[i], tags == null ? 0 : tags[i]);
    }

    Arrays.sort(tmp);

    System.out.println(Arrays.toString(tmp));
  }

  private static class Pair implements Comparable<Pair> {
    Object obj;
    long tag;
    public Pair(Object o, long t) {
      obj = o;
      tag = t;
    }

    public int compareTo(Pair p) {
      if (tag != p.tag) {
        return Long.compare(tag, p.tag);
      }

      if ((obj instanceof Comparable) && (p.obj instanceof Comparable)) {
        // It's not really correct, but w/e, best effort.
        int result = ((Comparable<Object>)obj).compareTo(p.obj);
        if (result != 0) {
          return result;
        }
      }

      if (obj != null && p.obj != null) {
        return obj.hashCode() - p.obj.hashCode();
      }

      if (obj != null) {
        return 1;
      }

      if (p.obj != null) {
        return -1;
      }

      return hashCode() - p.hashCode();
    }

    public String toString() {
      return "<" + obj + ";" + tag + ">";
    }
  }

  private static native Object[] getTaggedObjects(long[] searchTags, boolean returnObjects,
      boolean returnTags);
  private static native long[] testTagsInDifferentEnvs(Object o, long baseTag, int n);
}
