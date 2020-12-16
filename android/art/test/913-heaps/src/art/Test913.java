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

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.concurrent.CountDownLatch;

public class Test913 {
  public static void run() throws Exception {
    doTest();

    // Use a countdown latch for synchronization, as join() will introduce more roots.
    final CountDownLatch cdl1 = new CountDownLatch(1);

    // Run the follow-references tests on a dedicated thread so we know the specific Thread type.
    Thread t = new Thread() {
      @Override
      public void run() {
        try {
          Test913.runFollowReferences();
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
        cdl1.countDown();
      }
    };
    t.start();
    cdl1.await();

    doExtensionTests();
  }

  public static void runFollowReferences() throws Exception {
    new TestConfig().doFollowReferencesTest();

    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    new TestConfig(null, 0, 1, -1).doFollowReferencesTest();

    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    new TestConfig(null, 0, Integer.MAX_VALUE, 1).doFollowReferencesTest();

    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    doStringTest();

    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    doPrimitiveArrayTest();
    doPrimitiveFieldTest();

    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    // Test klass filter.
    System.out.println("--- klass ---");
    new TestConfig(A.class, 0).doFollowReferencesTest();

    // Test heap filter.
    System.out.println("--- heap_filter ---");
    System.out.println("---- tagged objects");
    new TestConfig(null, 0x4).doFollowReferencesTest();
    System.out.println("---- untagged objects");
    new TestConfig(null, 0x8).doFollowReferencesTest();
    System.out.println("---- tagged classes");
    new TestConfig(null, 0x10).doFollowReferencesTest();
    System.out.println("---- untagged classes");
    new TestConfig(null, 0x20).doFollowReferencesTest();
  }

  public static void doTest() throws Exception {
    setupGcCallback();

    enableGcTracking(true);
    runGc();
    enableGcTracking(false);
  }

  public static void doStringTest() throws Exception {
    final String str = new String("HelloWorld");
    final String str2 = new String("");
    Object o = new Object() {
      String s = str;
      String s2 = str2;
    };

    setTag(str, 1);
    setTag(str2, 2);
    System.out.println(Arrays.toString(followReferencesString(o)));
    System.out.println(getTag(str));
    System.out.println(getTag(str2));
  }

  public static void doPrimitiveArrayTest() throws Exception {
    final boolean[] zArray = new boolean[] { false, true };
    setTag(zArray, 1);

    final byte[] bArray = new byte[] { 1, 2, 3 };
    setTag(bArray, 2);

    final char[] cArray = new char[] { 'A', 'Z' };
    setTag(cArray, 3);

    final short[] sArray = new short[] { 1, 2, 3 };
    setTag(sArray, 4);

    final int[] iArray = new int[] { 1, 2, 3 };
    setTag(iArray, 5);

    final float[] fArray = new float[] { 0.0f, 1.0f };
    setTag(fArray, 6);

    final long[] lArray = new long[] { 1, 2, 3 };
    setTag(lArray, 7);

    final double[] dArray = new double[] { 0.0, 1.0 };
    setTag(dArray, 8);

    Object o = new Object() {
      Object z = zArray;
      Object b = bArray;
      Object c = cArray;
      Object s = sArray;
      Object i = iArray;
      Object f = fArray;
      Object l = lArray;
      Object d = dArray;
    };

    System.out.println(followReferencesPrimitiveArray(o));
    System.out.print(getTag(zArray));
    System.out.print(getTag(bArray));
    System.out.print(getTag(cArray));
    System.out.print(getTag(sArray));
    System.out.print(getTag(iArray));
    System.out.print(getTag(fArray));
    System.out.print(getTag(lArray));
    System.out.println(getTag(dArray));
  }

  public static void doPrimitiveFieldTest() throws Exception {
    // Force GCs to clean up dirt.
    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    doTestPrimitiveFieldsClasses();

    doTestPrimitiveFieldsIntegral();

    // Force GCs to clean up dirt.
    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    doTestPrimitiveFieldsFloat();

    // Force GCs to clean up dirt.
    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();
  }

  private static void doTestPrimitiveFieldsClasses() {
    setTag(IntObject.class, 10000);
    System.out.println(followReferencesPrimitiveFields(IntObject.class));
    System.out.println(getTag(IntObject.class));
    setTag(IntObject.class, 0);

    setTag(FloatObject.class, 10000);
    System.out.println(followReferencesPrimitiveFields(FloatObject.class));
    System.out.println(getTag(FloatObject.class));
    setTag(FloatObject.class, 0);

    boolean correctHeapValue = false;
    setTag(Inf1.class, 10000);
    String heapTrace = followReferencesPrimitiveFields(Inf1.class);

    if (!checkInitialized(Inf1.class)) {
      correctHeapValue = heapTrace.equals("10000@0 (static, int, index=0) 0000000000000000");
    } else {
      correctHeapValue = heapTrace.equals("10000@0 (static, int, index=0) 0000000000000001");
    }

    if (!correctHeapValue)
      System.out.println("Heap Trace for Inf1 is not as expected:\n" + heapTrace);

    System.out.println(getTag(Inf1.class));
    setTag(Inf1.class, 0);

    setTag(Inf2.class, 10000);
    heapTrace = followReferencesPrimitiveFields(Inf2.class);

    if (!checkInitialized(Inf2.class)) {
      correctHeapValue = heapTrace.equals("10000@0 (static, int, index=1) 0000000000000000");
    } else {
      correctHeapValue = heapTrace.equals("10000@0 (static, int, index=1) 0000000000000001");
    }

    if (!correctHeapValue)
      System.out.println("Heap Trace for Inf2 is not as expected:\n" + heapTrace);
    System.out.println(getTag(Inf2.class));
    setTag(Inf2.class, 0);
  }

  private static void doTestPrimitiveFieldsIntegral() {
    IntObject intObject = new IntObject();
    setTag(intObject, 10000);
    System.out.println(followReferencesPrimitiveFields(intObject));
    System.out.println(getTag(intObject));
  }

  private static void doTestPrimitiveFieldsFloat() {
    FloatObject floatObject = new FloatObject();
    setTag(floatObject, 10000);
    System.out.println(followReferencesPrimitiveFields(floatObject));
    System.out.println(getTag(floatObject));
  }

  static ArrayList<Object> extensionTestHolder;

  private static void doExtensionTests() {
    checkForExtensionApis();

    extensionTestHolder = new ArrayList<>();
    System.out.println();

    try {
      getHeapName(-1);
      System.out.println("Expected failure for -1");
    } catch (Exception e) {
    }
    System.out.println(getHeapName(0));
    System.out.println(getHeapName(1));
    System.out.println(getHeapName(2));
    System.out.println(getHeapName(3));
    try {
      getHeapName(4);
      System.out.println("Expected failure for -1");
    } catch (Exception e) {
    }

    System.out.println();

    setTag(Object.class, 100000);
    int objectClassHeapId = getObjectHeapId(100000);
    int objClassExpectedHeapId = hasImage() ? 1 : 3;
    if (objectClassHeapId != objClassExpectedHeapId) {
      throw new RuntimeException("Expected object class in heap " + objClassExpectedHeapId +
          " but received " + objectClassHeapId);
    }

    A a = new A();
    extensionTestHolder.add(a);
    setTag(a, 100001);
    System.out.println(getObjectHeapId(100001));

    checkGetObjectHeapIdInCallback(100000, objClassExpectedHeapId);
    checkGetObjectHeapIdInCallback(100001, 3);

    long baseTag = 30000000;
    setTag(Object.class, baseTag + objClassExpectedHeapId);
    setTag(Class.class, baseTag + objClassExpectedHeapId);
    Object o = new Object();
    extensionTestHolder.add(o);
    setTag(o, baseTag + 3);

    iterateThroughHeapExt();

    extensionTestHolder = null;
  }

  private static void runGc() {
    clearStats();
    forceGarbageCollection();
    printStats();
  }

  private static void clearStats() {
    getGcStarts();
    getGcFinishes();
  }

  private static void printStats() {
    System.out.println("---");
    int s = getGcStarts();
    int f = getGcFinishes();
    System.out.println((s > 0) + " " + (f > 0));
  }

  private static boolean hasImage() {
    try {
      int pid = Integer.parseInt(new File("/proc/self").getCanonicalFile().getName());
      BufferedReader reader = new BufferedReader(new FileReader("/proc/" + pid + "/maps"));
      String line;
      while ((line = reader.readLine()) != null) {
        if (line.endsWith(".art")) {
          reader.close();
          return true;
        }
      }
      reader.close();
      return false;
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }

  private static class TestConfig {
    private Class<?> klass = null;
    private int heapFilter = 0;
    private int stopAfter = Integer.MAX_VALUE;
    private int followSet = -1;

    public TestConfig() {
    }
    public TestConfig(Class<?> klass, int heapFilter) {
      this.klass = klass;
      this.heapFilter = heapFilter;
    }
    public TestConfig(Class<?> klass, int heapFilter, int stopAfter, int followSet) {
      this.klass = klass;
      this.heapFilter = heapFilter;
      this.stopAfter = stopAfter;
      this.followSet = followSet;
    }

    public void doFollowReferencesTest() throws Exception {
      // Force GCs to clean up dirt.
      Runtime.getRuntime().gc();
      Runtime.getRuntime().gc();

      setTag(Thread.currentThread(), 3000);

      {
        ArrayList<Object> tmpStorage = new ArrayList<>();
        doFollowReferencesTestNonRoot(tmpStorage);
        tmpStorage = null;
      }

      // Force GCs to clean up dirt.
      Runtime.getRuntime().gc();
      Runtime.getRuntime().gc();

      doFollowReferencesTestRoot();

      // Force GCs to clean up dirt.
      Runtime.getRuntime().gc();
      Runtime.getRuntime().gc();
    }

    private void doFollowReferencesTestNonRoot(ArrayList<Object> tmpStorage) {
      Verifier v = new Verifier();
      tagClasses(v);
      A a = createTree(v);
      tmpStorage.add(a);
      v.add("0@0", "1@1000");  // tmpStorage[0] --(array-element)--> a.

      doFollowReferencesTestImpl(null, stopAfter, followSet, null, v, null);
      doFollowReferencesTestImpl(a.foo2, stopAfter, followSet, null, v, "3@1001");

      tmpStorage.clear();
    }

    private void doFollowReferencesTestRoot() {
      Verifier v = new Verifier();
      tagClasses(v);
      A a = createTree(v);

      doFollowReferencesTestImpl(null, stopAfter, followSet, a, v, null);
      doFollowReferencesTestImpl(a.foo2, stopAfter, followSet, a, v, "3@1001");
    }

    private void doFollowReferencesTestImpl(A root, int stopAfter, int followSet,
        Object asRoot, Verifier v, String additionalEnabled) {
      String[] lines =
          followReferences(heapFilter, klass, root, stopAfter, followSet, asRoot);

      v.process(lines, additionalEnabled, heapFilter != 0 || klass != null);
    }

    private static void tagClasses(Verifier v) {
      setTag(A.class, 1000);
      registerClass(1000, A.class);

      setTag(B.class, 1001);
      registerClass(1001, B.class);
      v.add("1001@0", "1000@0");  // B.class --(superclass)--> A.class.

      setTag(C.class, 1002);
      registerClass(1002, C.class);
      v.add("1002@0", "1001@0");  // C.class --(superclass)--> B.class.
      v.add("1002@0", "2001@0");  // C.class --(interface)--> I2.class.

      setTag(I1.class, 2000);
      registerClass(2000, I1.class);

      setTag(I2.class, 2001);
      registerClass(2001, I2.class);
      v.add("2001@0", "2000@0");  // I2.class --(interface)--> I1.class.
    }

    private static A createTree(Verifier v) {
      A aInst = new A();
      setTag(aInst, 1);
      String aInstStr = "1@1000";
      String aClassStr = "1000@0";
      v.add(aInstStr, aClassStr);  // A -->(class) --> A.class.

      A a2Inst = new A();
      setTag(a2Inst, 2);
      aInst.foo = a2Inst;
      String a2InstStr = "2@1000";
      v.add(a2InstStr, aClassStr);  // A2 -->(class) --> A.class.
      v.add(aInstStr, a2InstStr);   // A -->(field) --> A2.

      B bInst = new B();
      setTag(bInst, 3);
      aInst.foo2 = bInst;
      String bInstStr = "3@1001";
      String bClassStr = "1001@0";
      v.add(bInstStr, bClassStr);  // B -->(class) --> B.class.
      v.add(aInstStr, bInstStr);   // A -->(field) --> B.

      A a3Inst = new A();
      setTag(a3Inst, 4);
      bInst.bar = a3Inst;
      String a3InstStr = "4@1000";
      v.add(a3InstStr, aClassStr);  // A3 -->(class) --> A.class.
      v.add(bInstStr, a3InstStr);   // B -->(field) --> A3.

      C cInst = new C();
      setTag(cInst, 5);
      bInst.bar2 = cInst;
      String cInstStr = "5@1000";
      String cClassStr = "1002@0";
      v.add(cInstStr, cClassStr);  // C -->(class) --> C.class.
      v.add(bInstStr, cInstStr);   // B -->(field) --> C.

      A a4Inst = new A();
      setTag(a4Inst, 6);
      cInst.baz = a4Inst;
      String a4InstStr = "6@1000";
      v.add(a4InstStr, aClassStr);  // A4 -->(class) --> A.class.
      v.add(cInstStr, a4InstStr);   // C -->(field) --> A4.

      cInst.baz2 = aInst;
      v.add(cInstStr, aInstStr);  // C -->(field) --> A.

      A[] aArray = new A[2];
      setTag(aArray, 500);
      aArray[1] = a2Inst;
      cInst.array = aArray;
      String aArrayStr = "500@0";
      v.add(cInstStr, aArrayStr);
      v.add(aArrayStr, a2InstStr);

      return aInst;
    }
  }

  public static class A {
    public A foo;
    public A foo2;

    public A() {}
    public A(A a, A b) {
      foo = a;
      foo2 = b;
    }
  }

  public static class B extends A {
    public A bar;
    public A bar2;

    public B() {}
    public B(A a, A b) {
      bar = a;
      bar2 = b;
    }
  }

  public static interface I1 {
    public final static int i1Field = 1;
  }

  public static interface I2 extends I1 {
    public final static int i2Field = 2;
  }

  public static class C extends B implements I2 {
    public A baz;
    public A baz2;
    public A[] array;

    public C() {}
    public C(A a, A b) {
      baz = a;
      baz2 = b;
    }
  }

  private static interface Inf1 {
    public final static int A = 1;
  }

  private static interface Inf2 extends Inf1 {
    public final static int B = 1;
  }

  private static class IntObject implements Inf1 {
    byte b = (byte)1;
    char c= 'a';
    short s = (short)2;
    int i = 3;
    long l = 4;
    Object o = new Object();
    static int sI = 5;
  }

  private static class FloatObject extends IntObject implements Inf2 {
    float f = 1.23f;
    double d = 1.23;
    Object p = new Object();
    static int sI = 6;
  }

  public static class Verifier {
    // Should roots with vreg=-1 be printed?
    public final static boolean PRINT_ROOTS_WITH_UNKNOWN_VREG = false;

    public static class Node {
      public String referrer;

      public HashSet<String> referrees = new HashSet<>();

      public Node(String r) {
        referrer = r;
      }

      public boolean isRoot() {
        return referrer.startsWith("root@");
      }
    }

    HashMap<String, Node> nodes = new HashMap<>();

    public Verifier() {
    }

    public void add(String referrer, String referree) {
      if (!nodes.containsKey(referrer)) {
        nodes.put(referrer, new Node(referrer));
      }
      if (referree != null) {
        nodes.get(referrer).referrees.add(referree);
      }
    }

    public void process(String[] lines, String additionalEnabledReferrer, boolean filtered) {
      // This method isn't optimal. The loops could be merged. However, it's more readable if
      // the different parts are separated.

      ArrayList<String> rootLines = new ArrayList<>();
      ArrayList<String> nonRootLines = new ArrayList<>();

      // Check for consecutive chunks of referrers. Also ensure roots come first.
      {
        String currentHead = null;
        boolean rootsDone = false;
        HashSet<String> completedReferrers = new HashSet<>();
        for (String l : lines) {
          String referrer = getReferrer(l);

          if (isRoot(referrer)) {
            if (rootsDone) {
              System.out.println("ERROR: Late root " + l);
              print(lines);
              return;
            }
            rootLines.add(l);
            continue;
          }

          rootsDone = true;

          if (currentHead == null) {
            currentHead = referrer;
          } else {
            // Ignore 0@0, as it can happen at any time (as it stands for all other objects).
            if (!currentHead.equals(referrer) && !referrer.equals("0@0")) {
              completedReferrers.add(currentHead);
              currentHead = referrer;
              if (completedReferrers.contains(referrer)) {
                System.out.println("Non-contiguous referrer " + l);
                print(lines);
                return;
              }
            }
          }
          nonRootLines.add(l);
        }
      }

      // Sort (root order is not specified) and print the roots.
      // TODO: What about extra roots? JNI and the interpreter seem to introduce those (though it
      //       isn't clear why a debuggable-AoT test doesn't have the same, at least for locals).
      //       For now, swallow duplicates, and resolve once we have the metadata for the roots.
      {
        Collections.sort(rootLines);
        String lastRoot = null;
        for (String l : rootLines) {
          if (lastRoot != null && lastRoot.equals(l)) {
            continue;
          }
          lastRoot = l;
          if (!PRINT_ROOTS_WITH_UNKNOWN_VREG && l.indexOf("vreg=-1") > 0) {
            continue;
          }
          System.out.println(l);
        }
      }

      if (filtered) {
        // If we aren't tracking dependencies, just sort the lines and print.
        // TODO: As the verifier is currently using the output lines to track dependencies, we
        //       cannot verify that output is correct when parts of it are suppressed by filters.
        //       To correctly track this we need to take node information into account, and
        //       actually analyze the graph.
        Collections.sort(nonRootLines);
        for (String l : nonRootLines) {
          System.out.println(l);
        }

        System.out.println("---");
        return;
      }

      // Iterate through the lines, keeping track of which referrers are visited, to ensure the
      // order is acceptable.
      HashSet<String> enabled = new HashSet<>();
      if (additionalEnabledReferrer != null) {
        enabled.add(additionalEnabledReferrer);
      }
      // Always add "0@0".
      enabled.add("0@0");

      for (String l : lines) {
        String referrer = getReferrer(l);
        String referree = getReferree(l);
        if (isRoot(referrer)) {
          // For a root src, just enable the referree.
          enabled.add(referree);
        } else {
          // Check that the referrer is enabled (may be visited).
          if (!enabled.contains(referrer)) {
            System.out.println("Referrer " + referrer + " not enabled: " + l);
            print(lines);
            return;
          }
          enabled.add(referree);
        }
      }

      // Now just sort the non-root lines and output them
      Collections.sort(nonRootLines);
      for (String l : nonRootLines) {
        System.out.println(l);
      }

      System.out.println("---");
    }

    public static boolean isRoot(String ref) {
      return ref.startsWith("root@");
    }

    private static String getReferrer(String line) {
      int i = line.indexOf(" --");
      if (i <= 0) {
        throw new IllegalArgumentException(line);
      }
      int j = line.indexOf(' ');
      if (i != j) {
        throw new IllegalArgumentException(line);
      }
      return line.substring(0, i);
    }

    private static String getReferree(String line) {
      int i = line.indexOf("--> ");
      if (i <= 0) {
        throw new IllegalArgumentException(line);
      }
      int j = line.indexOf(' ', i + 4);
      if (j < 0) {
        throw new IllegalArgumentException(line);
      }
      return line.substring(i + 4, j);
    }

    private static void print(String[] lines) {
      for (String l : lines) {
        System.out.println(l);
      }
    }
  }

  private static void setTag(Object o, long tag) {
    Main.setTag(o, tag);
  }
  private static long getTag(Object o) {
    return Main.getTag(o);
  }

  private static native boolean checkInitialized(Class<?> klass);
  private static native void setupGcCallback();
  private static native void enableGcTracking(boolean enable);
  private static native int getGcStarts();
  private static native int getGcFinishes();
  private static native void forceGarbageCollection();

  private static native void checkForExtensionApis();
  private static native int getObjectHeapId(long tag);
  private static native String getHeapName(int heapId);
  private static native void checkGetObjectHeapIdInCallback(long tag, int heapId);

  public static native String[] followReferences(int heapFilter, Class<?> klassFilter,
      Object initialObject, int stopAfter, int followSet, Object jniRef);
  public static native String[] followReferencesString(Object initialObject);
  public static native String followReferencesPrimitiveArray(Object initialObject);
  public static native String followReferencesPrimitiveFields(Object initialObject);

  private static native void iterateThroughHeapExt();

  private static native void registerClass(long tag, Object obj);
}
