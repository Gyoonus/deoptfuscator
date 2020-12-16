/*
 * Copyright (C) 2017 The Android Open Source Project
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

import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.Vector;
import java.util.function.Function;

public class Test990 {

  // Fields of these classes are accessed/modified differently in the RI and ART so we ignore them.
  static Collection<Class<?>> IGNORED_CLASSES = Arrays.asList(new Class<?>[] {
    ClassLoader.class,
    Vector.class,
  });

  static interface Printable { public void Print(); }

  static final class FieldWrite implements Printable {
    private Executable method;
    private Object target;
    private Field f;
    private String initialValue;
    private Class<?> initialValueType;

    public FieldWrite(Executable method, Object target, Field f, Object v) {
      this.method = method;
      this.target = target;
      this.f = f;
      this.initialValue =  genericToString(v);
      this.initialValueType = v != null ? v.getClass() : null;
    }

    @Override
    public void Print() {
        System.out.println("MODIFY of " + f + " on object of" +
            " type: " + (target == null ? null : target.getClass()) +
            " in method " + method +
            ". New value: " + initialValue + " (type: " + initialValueType + ")");
    }
  }

  static final class FieldRead implements Printable {
    private Executable method;
    private Object target;
    private Field f;

    public FieldRead(Executable method, Object target, Field f) {
      this.method = method;
      this.target = target;
      this.f = f;
    }

    @Override
    public void Print() {
        System.out.println("ACCESS of " + f + " on object of" +
            " type " + (target == null ? null : target.getClass()) +
            " in method " + method);
    }
  }

  private static String genericToString(Object val) {
    if (val == null) {
      return "null";
    } else if (val.getClass().isArray()) {
      return arrayToString(val);
    } else if (val instanceof Throwable) {
      StringWriter w = new StringWriter();
      ((Throwable) val).printStackTrace(new PrintWriter(w));
      return w.toString();
    } else {
      return val.toString();
    }
  }

  private static String charArrayToString(char[] src) {
    String[] res = new String[src.length];
    for (int i = 0; i < src.length; i++) {
      if (Character.isISOControl(src[i])) {
        res[i] = Character.getName(src[i]);
      } else {
        res[i] = Character.toString(src[i]);
      }
    }
    return Arrays.toString(res);
  }

  private static String arrayToString(Object val) {
    Class<?> klass = val.getClass();
    if ((new Object[0]).getClass().isAssignableFrom(klass)) {
      return Arrays.toString(
          Arrays.stream((Object[])val).map(new Function<Object, String>() {
            public String apply(Object o) {
              return genericToString(o);
            }
          }).toArray());
    } else if ((new byte[0]).getClass().isAssignableFrom(klass)) {
      return Arrays.toString((byte[])val);
    } else if ((new char[0]).getClass().isAssignableFrom(klass)) {
      return charArrayToString((char[])val);
    } else if ((new short[0]).getClass().isAssignableFrom(klass)) {
      return Arrays.toString((short[])val);
    } else if ((new int[0]).getClass().isAssignableFrom(klass)) {
      return Arrays.toString((int[])val);
    } else if ((new long[0]).getClass().isAssignableFrom(klass)) {
      return Arrays.toString((long[])val);
    } else if ((new float[0]).getClass().isAssignableFrom(klass)) {
      return Arrays.toString((float[])val);
    } else if ((new double[0]).getClass().isAssignableFrom(klass)) {
      return Arrays.toString((double[])val);
    } else {
      throw new Error("Unknown type " + klass);
    }
  }

  private static List<Printable> results = new ArrayList<>();

  public static void notifyFieldModify(
      Executable m, long location, Class<?> f_klass, Object target, Field f, Object value) {
    if (IGNORED_CLASSES.contains(f_klass)) {
      return;
    }
    results.add(new FieldWrite(m, target, f, value));
  }

  public static void notifyFieldAccess(
      Executable m, long location, Class<?> f_klass, Object target, Field f) {
    if (IGNORED_CLASSES.contains(f_klass)) {
      return;
    }
    results.add(new FieldRead(m, target, f));
  }

  static class TestClass1 {
    Object abc;
    int xyz;
    int foobar;
    public TestClass1(int xyz, Object abc) {
      this.xyz = xyz;
      this.abc = abc;
    }

    public void tweak(int def) {
      if (def == xyz) {
        foobar++;
      }
    }
    public String toString() {
      return "TestClass1 { abc: \"" + genericToString(abc) + "\", xyz: " + xyz
          + ", foobar: " + foobar + " }";
    }
  }

  static class TestClass2 extends TestClass1 {
    static long TOTAL = 0;
    long baz;
    public TestClass2(long baz) {
      super(1337, "TESTING");
      this.baz = baz;
    }

    public void tweak(int def) {
      TOTAL++;
      super.tweak(def);
      baz++;
    }

    public String toString() {
      return "TestClass2 { super: \"%s\", TOTAL: %d, baz: %d }".format(
          super.toString(), TOTAL, baz);
    }
  }


  public static void run() throws Exception {
      Trace.disableTracing(Thread.currentThread());
      Trace.enableFieldTracing(
          Test990.class,
          Test990.class.getDeclaredMethod("notifyFieldAccess",
            Executable.class, Long.TYPE, Class.class, Object.class, Field.class),
          Test990.class.getDeclaredMethod("notifyFieldModify",
            Executable.class, Long.TYPE, Class.class, Object.class, Field.class, Object.class),
          Thread.currentThread());
      Trace.watchAllFieldAccesses();
      Trace.watchAllFieldModifications();
      TestClass1 t1 = new TestClass1(1, "tc1");
      TestClass1 t2 = new TestClass2(2);
      TestClass1 t3 = new TestClass1(3, t1);
      TestClass1 t4 = new TestClass1(4, t2);
      t1.tweak(1);
      t1.tweak(1);
      t2.tweak(12);
      t2.tweak(1337);
      t2.tweak(12);
      t2.tweak(1338);
      t1.tweak(t3.foobar);
      t4.tweak((int)((TestClass2)t2).baz);
      t4.tweak((int)TestClass2.TOTAL);
      t2.tweak((int)TestClass2.TOTAL);

      // Turn off tracing so we don't have to deal with print internals.
      Trace.disableTracing(Thread.currentThread());
      printResults();
  }

  public static void printResults() {
    for (Printable p : results) {
      p.Print();
    }
  }
}
