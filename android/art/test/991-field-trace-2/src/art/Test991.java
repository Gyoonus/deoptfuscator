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

import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;

public class Test991 {
  static List<Field> WATCH_FIELDS = Arrays.asList(TestClass1.class.getDeclaredFields());

  static FieldTracer TRACE = null;

  static abstract class FieldTracer {
    public final void notifyFieldAccess(
        Executable method, long location, Class<?> f_klass, Object target, Field f) {
      System.out.println("FieldTracer: " + this.getClass());
      System.out.println("\tACCESS of " + f + " on object of" +
          " type: " + (target == null ? null : target.getClass()) +
          " in method " + method);
      handleFieldAccess(method, location, f_klass, target, f);
    }

    public final void notifyFieldModify(
        Executable method, long location, Class<?> f_klass, Object target, Field f, Object value) {
      System.out.println("FieldTracer: " + this.getClass());
      System.out.println("\tMODIFY of " + f + " on object of" +
          " type: " + (target == null ? null : target.getClass()) +
          " in method " + method +
          ". New value: " + value + " (type: " + value.getClass() + ")");
      handleFieldModify(method, location, f_klass, target, f, value);
    }

    public void handleFieldAccess(Executable m, long l, Class<?> fk, Object t, Field f) {}
    public void handleFieldModify(Executable m, long l, Class<?> fk, Object t, Field f, Object v) {}
  }

  private static class TestError extends Error {
    private static final long serialVersionUID = 0;
    public TestError(String s) { super(s); }
  }
  static class DoNothingFieldTracer extends FieldTracer {}
  static class ThrowReadFieldTracer extends FieldTracer {
    @Override
    public void handleFieldAccess(Executable m, long l, Class<?> fk, Object t, Field f) {
      throw new TestError("Throwing error during access");
    }
  }
  static class ThrowWriteFieldTracer extends FieldTracer {
    @Override
    public void handleFieldModify(Executable m, long l, Class<?> fk, Object t, Field f, Object v) {
      throw new TestError("Throwing error during modify");
    }
  }
  static class ModifyDuringReadAndWriteFieldTracer extends FieldTracer {
    @Override
    public void handleFieldModify(Executable m, long l, Class<?> fk, Object t, Field f, Object v) {
      // NB This is only safe because the agent doesn't send recursive access/modification events up
      // to the java layer here.
      ((TestClass1)t).xyz += 100;
    }
    @Override
    public void handleFieldAccess(Executable m, long l, Class<?> fk, Object t, Field f) {
      // NB This is only safe because the agent doesn't send recursive access/modification events up
      // to the java layer here.
      ((TestClass1)t).xyz += 10;
    }
  }

  static class ModifyDuringWriteFieldTracer extends FieldTracer {
    @Override
    public void handleFieldModify(Executable m, long l, Class<?> fk, Object t, Field f, Object v) {
      // NB This is only safe because the agent doesn't send recursive access/modification events up
      // to the java layer here.
      ((TestClass1)t).xyz += 200;
    }
  }

  static class ModifyDuringReadFieldTracer extends FieldTracer {
    @Override
    public void handleFieldAccess(Executable m, long l, Class<?> fk, Object t, Field f) {
      // NB This is only safe because the agent doesn't send recursive access/modification events up
      // to the java layer here.
      ((TestClass1)t).xyz += 20;
    }
  }

  public static void notifyFieldModify(
      Executable m, long location, Class<?> f_klass, Object target, Field f, Object value) {
    if (TRACE != null) {
      TRACE.notifyFieldModify(m, location, f_klass, target, f, value);
    }
  }

  public static void notifyFieldAccess(
      Executable m, long location, Class<?> f_klass, Object target, Field f) {
    if (TRACE != null) {
      TRACE.notifyFieldAccess(m, location, f_klass, target, f);
    }
  }

  public static class TestClass1 {
    public int xyz;
    public TestClass1(int xyz) {
      this.xyz = xyz;
    }
  }

  public static int readFieldUntraced(TestClass1 target) {
    FieldTracer tmp = TRACE;
    TRACE = null;
    int res = target.xyz;
    TRACE = tmp;
    return res;
  }

  public static class JavaReadWrite implements Consumer<TestClass1> {
    public void accept(TestClass1 t1) {
      int val = t1.xyz;
      System.out.println("normal read: xyz = " + val);
      t1.xyz = val + 1;
    }
  }

  public static class ReflectiveReadWrite implements Consumer<TestClass1> {
    public void accept(TestClass1 t1) {
      try {
        Field f = t1.getClass().getDeclaredField("xyz");
        int val = f.getInt(t1);
        System.out.println("reflective read: xyz = " + val);
        f.setInt(t1, val + 1);
      } catch (IllegalAccessException iae) {
        throw new InternalError("Could not set field xyz", iae);
      } catch (NoSuchFieldException nsfe) {
        throw new InternalError("Could not find field xyz", nsfe);
      }
    }
  }

  public static class NativeReadWrite implements Consumer<TestClass1> {
    public void accept(TestClass1 t1) {
      doNativeReadWrite(t1);
    }
  }

  public static TestClass1 createTestClassNonTraced() {
    FieldTracer tmp = TRACE;
    TRACE = null;
    TestClass1 n = new TestClass1(0);
    TRACE = tmp;
    return n;
  }

  public static void run() throws Exception {
    Trace.disableTracing(Thread.currentThread());
    Trace.enableFieldTracing(
        Test991.class,
        Test991.class.getDeclaredMethod("notifyFieldAccess",
          Executable.class, Long.TYPE, Class.class, Object.class, Field.class),
        Test991.class.getDeclaredMethod("notifyFieldModify",
          Executable.class, Long.TYPE, Class.class, Object.class, Field.class, Object.class),
        Thread.currentThread());
    for (Field f : WATCH_FIELDS) {
      Trace.watchFieldAccess(f);
      Trace.watchFieldModification(f);
    }
    FieldTracer[] tracers = new FieldTracer[] {
      new DoNothingFieldTracer(),
      new ThrowReadFieldTracer(),
      new ThrowWriteFieldTracer(),
      new ModifyDuringReadFieldTracer(),
      new ModifyDuringWriteFieldTracer(),
      new ModifyDuringReadAndWriteFieldTracer(),
    };
    Consumer<TestClass1>[] field_modification = new Consumer[] {
      new JavaReadWrite(),
      new ReflectiveReadWrite(),
      new NativeReadWrite(),
    };
    for (Consumer<TestClass1> c : field_modification) {
      for (FieldTracer trace : tracers) {
        System.out.println("Test is " + trace.getClass() + " & " + c.getClass());
        TestClass1 t1 = createTestClassNonTraced();
        TRACE = trace;
        System.out.println("Initial state: xyz = " + readFieldUntraced(t1));
        try {
          c.accept(t1);
        } catch (TestError e) {
          System.out.println("Caught error. " + e);
        } finally {
          System.out.println("Final state: xyz = " + readFieldUntraced(t1));
        }
      }
    }
    Trace.disableTracing(Thread.currentThread());
  }

  public static native void doNativeReadWrite(TestClass1 t1);

  public static void doPrintNativeNotification(int val) {
    System.out.println("native read: xyz = " + val);
  }
}
