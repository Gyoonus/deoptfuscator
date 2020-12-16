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
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

import java.time.Duration;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;
import java.util.Random;
import java.util.Stack;
import java.util.Vector;

import java.util.function.Supplier;

public class Test993 {

  public static final Breakpoint.Manager MANAGER = new Breakpoint.Manager();

  // A function we can use as a start breakpoint.
  public static void breakpoint() {
    return;
  }

  private static void privateBreakpoint() {
    return;
  }

  // An interface with a default method we can break on.
  static interface Breakable {
    public static void iBreakpoint() {
      return;
    }

    public default void breakit() {
      return;
    }
  }

  // A class that has a default method we breakpoint on.
  public static class TestClass1 implements Breakable {
    public TestClass1() {
      super();
    }
    public String toString() { return "TestClass1"; }
  }

  // A class that overrides a default method that we can breakpoint on and calls super.
  public static class TestClass1ext extends TestClass1 {
    public TestClass1ext() {
      super();
    }
    public String toString() { return "TestClass1Ext"; }
    public void breakit() {
      super.breakit();
    }
  }


  // A class that overrides a default method that we can breakpoint on.
  public static class TestClass2 implements Breakable {
    public String toString() { return "TestClass2"; }
    public void breakit() {
      return;
    }
  }

  // A class that overrides a default method that we can breakpoint on and calls super.
  public static class TestClass2ext extends TestClass2 {
    public String toString() { return "TestClass2ext"; }
    public void breakit() {
      super.breakit();
    }
  }

  // A class that overrides a default method and calls it directly with interface invoke-super
  public static class TestClass3 implements Breakable {
    public String toString() { return "TestClass3"; }
    public void breakit() {
      Breakable.super.breakit();
    }
  }

  // A class that overrides a default method that we can breakpoint on and calls super to a class
  // that uses interface-invoke-super.
  public static class TestClass3ext extends TestClass3 {
    public String toString() { return "TestClass3ext"; }
    public void breakit() {
      super.breakit();
    }
  }

  public static class TestClass4 {
    public String toString() { return "TestClass4"; }
    public void callPrivateMethod() {
      privateMethod();
    }
    private void privateMethod() {
      return;
    }
  }

  public static void notifyBreakpointReached(Thread thr, Executable e, long loc) {
    String line;
    if (e.getDeclaringClass().getPackage().equals(Test993.class.getPackage())) {
      line = Integer.valueOf(Breakpoint.locationToLine(e, loc)).toString();
    } else {
      line = "<NON-DETERMINISTIC>";
    }
    System.out.println("\t\t\tBreakpoint: " + e + " @ line=" + line);
  }

  public static interface ThrowRunnable extends Runnable {
    public default void run() {
      try {
        runThrow();
      } catch (Exception e) {
        throw new Error("Caught error while running " + this, e);
      }
    }
    public void runThrow() throws Exception;
  }

  public static class InvokeDirect implements Runnable {
    String msg;
    Runnable r;
    public InvokeDirect(String msg, Runnable r) {
      this.msg = msg;
      this.r = r;
    }
    @Override
    public void run() {
      System.out.println("\t\tInvoking \"" + msg + "\"");
      r.run();
    }
  }

  public static class InvokeReflect implements ThrowRunnable {
    Method m;
    Object this_arg;
    public InvokeReflect(Method m, Object this_arg) {
      this.m = m;
      this.this_arg = this_arg;
    }

    @Override
    public void runThrow() throws Exception {
      System.out.println("\t\tReflective invoking: " + m + " args: [this: " + this_arg + "]");
      m.invoke(this_arg);
    }
  }

  public static class InvokeNative implements Runnable {
    Method m;
    Object this_arg;
    public InvokeNative(Method m, Object this_arg) {
      this.m = m;
      this.this_arg = this_arg;
    }

    @Override
    public void run() {
      System.out.println("\t\tNative invoking: " + m + " args: [this: " + this_arg + "]");
      invokeNative(m, m.getDeclaringClass(), this_arg);
    }
  }

  public static native void invokeNative(Method m, Class<?> clazz, Object thizz);

  public static class InvokeNativeBool implements Runnable {
    Method m;
    Object this_arg;
    public InvokeNativeBool(Method m, Object this_arg) {
      this.m = m;
      this.this_arg = this_arg;
    }

    @Override
    public void run() {
      System.out.println("\t\tNative invoking: " + m + " args: [this: " + this_arg + "]");
      invokeNativeBool(m, m.getDeclaringClass(), this_arg);
    }
  }

  public static native void invokeNativeBool(Method m, Class<?> clazz, Object thizz);

  public static class InvokeNativeObject implements Runnable {
    Method m;
    Object this_arg;
    public InvokeNativeObject(Method m, Object this_arg) {
      this.m = m;
      this.this_arg = this_arg;
    }

    @Override
    public void run() {
      System.out.println("\t\tNative invoking: " + m + " args: [this: " + this_arg + "]");
      invokeNativeObject(m, m.getDeclaringClass(), this_arg);
    }
  }

  public static native void invokeNativeObject(Method m, Class<?> clazz, Object thizz);

  public static class InvokeNativeLong implements Runnable {
    Method m;
    Object this_arg;
    public InvokeNativeLong(Method m, Object this_arg) {
      this.m = m;
      this.this_arg = this_arg;
    }

    @Override
    public void run() {
      System.out.println("\t\tNative invoking: " + m + " args: [this: " + this_arg + "]");
      invokeNativeLong(m, m.getDeclaringClass(), this_arg);
    }
  }

  public static native void invokeNativeLong(Method m, Class<?> clazz, Object thizz);

  public static class ConstructDirect implements Runnable {
    String msg;
    Supplier<Object> s;
    public ConstructDirect(String msg, Supplier<Object> s) {
      this.msg = msg;
      this.s = s;
    }

    @Override
    public void run() {
      System.out.println("\t\tConstructing: " + msg);
      System.out.println("\t\t\tCreated: " + s.get());
    }
  }

  public static class ConstructReflect implements ThrowRunnable {
    Constructor<?> m;
    public ConstructReflect(Constructor<?> m) {
      this.m = m;
    }

    @Override
    public void runThrow() throws Exception {
      System.out.println("\t\tReflective constructor: " + m);
      System.out.println("\t\t\tCreated: " + m.newInstance());
    }
  }

  public static class ConstructNative implements Runnable {
    Constructor<?> m;
    Class type;
    public ConstructNative(Constructor<?> m) {
      this.m = m;
      this.type = m.getDeclaringClass();
    }

    @Override
    public void run() {
      System.out.println("\t\tNative constructor: " + m + ", type: " + type);
      System.out.println("\t\t\tCreated: " + constructNative(m, type));
    }
  }

  public static native Object constructNative(Constructor m, Class<?> clazz);

  private static <T> List<List<T>> combinations(List<T> items, int len) {
    if (len > items.size()) {
      throw new Error("Bad length" + len + " " + items);
    }
    if (len == 1) {
      List<List<T>> out = new ArrayList<>();
      for (T t : items) {
        out.add(Arrays.asList(t));
      }
      return out;
    }
    List<List<T>> out = new ArrayList<>();
    for (int rem = 0; rem <= items.size() - len; rem++) {
      for (List<T> others : combinations(items.subList(rem + 1, items.size()), len - 1)) {
        List<T> newone = new ArrayList<>();
        newone.add(items.get(rem));
        newone.addAll(others);
        out.add(newone);
      }
    }
    return out;
  }

  private static <T> List<List<T>> allCombinations(List<T> items) {
    List<List<T>> out = new ArrayList<List<T>>();
    out.add(new ArrayList<>());
    for (int i = 0; i < items.size(); i++) {
      out.addAll(combinations(items, i + 1));
    }
    return out;
  }

  private static Breakpoint.Manager.BP BP(Executable m) {
    return new Breakpoint.Manager.BP(m) {
      public String toString() {
        if (method.getDeclaringClass().getPackage().equals(Test993.class.getPackage())) {
          return super.toString();
        } else {
          return method.toString() + " @ <NON-DETERMINISTIC>";
        }
      }
    };
  }

  public static void run() throws Exception {
    // Set up breakpoints
    Breakpoint.stopBreakpointWatch(Thread.currentThread());
    Breakpoint.startBreakpointWatch(
        Test993.class,
        Test993.class.getDeclaredMethod("notifyBreakpointReached",
          Thread.class, Executable.class, Long.TYPE),
        Thread.currentThread());

    runMethodTests();
    runBCPMethodTests();
    runConstructorTests();

    Breakpoint.stopBreakpointWatch(Thread.currentThread());
  }

  public static void runConstructorTests() throws Exception {
    // The constructors we will be breaking on.
    Constructor<?> tc1_construct = TestClass1.class.getConstructor();
    Constructor<?> tc1ext_construct = TestClass1ext.class.getConstructor();

    Runnable[] tc1_constructors = new Runnable[] {
      new ConstructNative(tc1_construct),
      new ConstructReflect(tc1_construct),
      new ConstructDirect("new TestClass1()", TestClass1::new),
    };
    Breakpoint.Manager.BP[] tc1_bps = new Breakpoint.Manager.BP[] {
      BP(tc1_construct),
    };
    runTestGroups("TestClass1 constructor", tc1_constructors, tc1_bps);

    Runnable[] tc1ext_constructors = new Runnable[] {
      new ConstructNative(tc1ext_construct),
      new ConstructReflect(tc1ext_construct),
      new ConstructDirect("new TestClass1ext()", TestClass1ext::new),
    };
    Breakpoint.Manager.BP[] tc1ext_bps = new Breakpoint.Manager.BP[] {
      BP(tc1_construct), BP(tc1ext_construct),
    };
    runTestGroups("TestClass1ext constructor", tc1ext_constructors, tc1ext_bps);
  }

  // These test to make sure we are able to break on functions that might have been quickened or
  // inlined from the boot-image. These were all chosen for being in the bootclasspath, not being
  // long enough to prevent inlining, and not being used for the testing framework.
  public static void runBCPMethodTests() throws Exception {
    // The methods we will be breaking on.
    Method bcp_private_method = Duration.class.getDeclaredMethod("toSeconds");
    Method bcp_virtual_method = Optional.class.getDeclaredMethod("isPresent");
    Method bcp_static_method = Optional.class.getDeclaredMethod("empty");
    Method bcp_private_static_method = Random.class.getDeclaredMethod("seedUniquifier");

    // Some constructors we will break on.
    Constructor<?> bcp_stack_constructor = Stack.class.getConstructor();
    Constructor<?> bcp_vector_constructor = Vector.class.getConstructor();
    if (!(Vector.class.isAssignableFrom(Stack.class))) {
      throw new Error("Expected Stack to extend Vector!");
    }

    // BCP constructors.
    Runnable[] vector_constructors = new Runnable[] {
      new ConstructNative(bcp_vector_constructor),
      new ConstructReflect(bcp_vector_constructor),
      new ConstructDirect("new Vector()", Vector::new),
    };
    Breakpoint.Manager.BP[] vector_breakpoints = new Breakpoint.Manager.BP[] {
      BP(bcp_vector_constructor),
    };
    runTestGroups("Vector constructor", vector_constructors, vector_breakpoints);

    Runnable[] stack_constructors = new Runnable[] {
      new ConstructNative(bcp_stack_constructor),
      new ConstructReflect(bcp_stack_constructor),
      new ConstructDirect("new Stack()", Stack::new),
    };
    Breakpoint.Manager.BP[] stack_breakpoints = new Breakpoint.Manager.BP[] {
      BP(bcp_stack_constructor), BP(bcp_vector_constructor),
    };
    runTestGroups("Stack constructor", stack_constructors, stack_breakpoints);

    // Static function
    Runnable[] static_invokes = new Runnable[] {
      new InvokeNativeObject(bcp_static_method, null),

      new InvokeReflect(bcp_static_method, null),

      new InvokeDirect("Optional::empty", () -> { Optional.empty(); }),
    };
    Breakpoint.Manager.BP[] static_breakpoints = new Breakpoint.Manager.BP[] {
      BP(bcp_static_method)
    };
    runTestGroups("bcp static invoke", static_invokes, static_breakpoints);

    // Static private class function
    Runnable[] private_static_invokes = new Runnable[] {
      new InvokeNativeLong(bcp_private_static_method, null),

      new InvokeDirect("Random::seedUniquifier", () -> { new Random(); }),
    };
    Breakpoint.Manager.BP[] private_static_breakpoints = new Breakpoint.Manager.BP[] {
      BP(bcp_private_static_method)
    };
    runTestGroups("bcp private static invoke", private_static_invokes, private_static_breakpoints);

    // private class method
    Duration test_duration = Duration.ofDays(14);
    Runnable[] private_invokes = new Runnable[] {
      new InvokeNativeObject(bcp_private_method, test_duration),

      new InvokeDirect("Duration::toSeconds", () -> { test_duration.multipliedBy(2); }),
    };
    Breakpoint.Manager.BP[] private_breakpoints = new Breakpoint.Manager.BP[] {
      BP(bcp_private_method)
    };
    runTestGroups("bcp private invoke", private_invokes, private_breakpoints);

    // class method
    Runnable[] public_invokes = new Runnable[] {
      new InvokeNativeBool(bcp_virtual_method, Optional.of("test")),

      new InvokeReflect(bcp_virtual_method, Optional.of("test2")),

      new InvokeDirect("Optional::isPresent", () -> { Optional.of("test3").isPresent(); }),
    };
    Breakpoint.Manager.BP[] public_breakpoints = new Breakpoint.Manager.BP[] {
      BP(bcp_virtual_method)
    };
    runTestGroups("bcp invoke", public_invokes, public_breakpoints);
  }

  public static void runMethodTests() throws Exception {
    // The methods we will be breaking on.
    Method breakpoint_method = Test993.class.getDeclaredMethod("breakpoint");
    Method private_breakpoint_method = Test993.class.getDeclaredMethod("privateBreakpoint");
    Method i_breakpoint_method = Breakable.class.getDeclaredMethod("iBreakpoint");
    Method breakit_method = Breakable.class.getDeclaredMethod("breakit");
    Method breakit_method_tc1ext = TestClass1ext.class.getDeclaredMethod("breakit");
    Method breakit_method_tc2 = TestClass2.class.getDeclaredMethod("breakit");
    Method breakit_method_tc2ext = TestClass2ext.class.getDeclaredMethod("breakit");
    Method breakit_method_tc3 = TestClass3.class.getDeclaredMethod("breakit");
    Method breakit_method_tc3ext = TestClass3ext.class.getDeclaredMethod("breakit");
    Method private_method = TestClass4.class.getDeclaredMethod("privateMethod");

    // Static class function
    Runnable[] static_invokes = new Runnable[] {
      new InvokeNative(breakpoint_method, null),

      new InvokeReflect(breakpoint_method, null),

      new InvokeDirect("Test993::breakpoint", Test993::breakpoint),
    };
    Breakpoint.Manager.BP[] static_breakpoints = new Breakpoint.Manager.BP[] {
      BP(breakpoint_method)
    };
    runTestGroups("static invoke", static_invokes, static_breakpoints);

    // Static private class function
    Runnable[] private_static_invokes = new Runnable[] {
      new InvokeNative(private_breakpoint_method, null),

      new InvokeDirect("Test993::privateBreakpoint", Test993::privateBreakpoint),
    };
    Breakpoint.Manager.BP[] private_static_breakpoints = new Breakpoint.Manager.BP[] {
      BP(private_breakpoint_method)
    };
    runTestGroups("private static invoke", private_static_invokes, private_static_breakpoints);

    // Static interface function.
    Runnable[] i_static_invokes = new Runnable[] {
      new InvokeNative(i_breakpoint_method, null),

      new InvokeReflect(i_breakpoint_method, null),

      new InvokeDirect("Breakable::iBreakpoint", Breakable::iBreakpoint),
    };
    Breakpoint.Manager.BP[] i_static_breakpoints = new Breakpoint.Manager.BP[] {
      BP(i_breakpoint_method)
    };
    runTestGroups("interface static invoke", i_static_invokes, i_static_breakpoints);

    // Call default method through a class.
    Runnable[] tc1_invokes = new Runnable[] {
      new InvokeNative(breakit_method, new TestClass1()),

      new InvokeReflect(breakit_method, new TestClass1()),

      new InvokeDirect("((Breakable)new TestClass1()).breakit()",
                  () -> ((Breakable)new TestClass1()).breakit()),
      new InvokeDirect("new TestClass1().breakit()",
                  () -> new TestClass1().breakit()),
    };
    Breakpoint.Manager.BP[] tc1_breakpoints = new Breakpoint.Manager.BP[] {
      BP(breakit_method)
    };
    runTestGroups("TestClass1 invokes", tc1_invokes, tc1_breakpoints);

    // Call default method through an override and normal invoke-super
    Runnable[] tc1ext_invokes = new Runnable[] {
      new InvokeNative(breakit_method, new TestClass1ext()),
      new InvokeNative(breakit_method_tc1ext, new TestClass1ext()),

      new InvokeReflect(breakit_method, new TestClass1ext()),
      new InvokeReflect(breakit_method_tc1ext, new TestClass1ext()),

      new InvokeDirect("((Breakable)new TestClass1ext()).breakit()",
                  () -> ((Breakable)new TestClass1ext()).breakit()),
      new InvokeDirect("((TestClass1)new TestClass1ext()).breakit()",
                  () -> ((TestClass1)new TestClass1ext()).breakit()),
      new InvokeDirect("new TestClass1ext().breakit()",
                  () -> new TestClass1ext().breakit()),
    };
    Breakpoint.Manager.BP[] tc1ext_breakpoints = new Breakpoint.Manager.BP[] {
      BP(breakit_method), BP(breakit_method_tc1ext)
    };
    runTestGroups("TestClass1ext invokes", tc1ext_invokes, tc1ext_breakpoints);

    // Override default/interface method.
    Runnable[] tc2_invokes = new Runnable[] {
      new InvokeNative(breakit_method, new TestClass2()),
      new InvokeNative(breakit_method_tc2, new TestClass2()),

      new InvokeReflect(breakit_method, new TestClass2()),
      new InvokeReflect(breakit_method_tc2, new TestClass2()),

      new InvokeDirect("((Breakable)new TestClass2()).breakit()",
                  () -> ((Breakable)new TestClass2()).breakit()),
      new InvokeDirect("new TestClass2().breakit()",
                  () -> new TestClass2().breakit()),
    };
    Breakpoint.Manager.BP[] tc2_breakpoints = new Breakpoint.Manager.BP[] {
      BP(breakit_method), BP(breakit_method_tc2)
    };
    runTestGroups("TestClass2 invokes", tc2_invokes, tc2_breakpoints);

    // Call overridden method using invoke-super
    Runnable[] tc2ext_invokes = new Runnable[] {
      new InvokeNative(breakit_method, new TestClass2ext()),
      new InvokeNative(breakit_method_tc2, new TestClass2ext()),
      new InvokeNative(breakit_method_tc2ext, new TestClass2ext()),

      new InvokeReflect(breakit_method, new TestClass2ext()),
      new InvokeReflect(breakit_method_tc2, new TestClass2ext()),
      new InvokeReflect(breakit_method_tc2ext, new TestClass2ext()),

      new InvokeDirect("((Breakable)new TestClass2ext()).breakit()",
                  () -> ((Breakable)new TestClass2ext()).breakit()),
      new InvokeDirect("((TestClass2)new TestClass2ext()).breakit()",
                  () -> ((TestClass2)new TestClass2ext()).breakit()),
      new InvokeDirect("new TestClass2ext().breakit())",
                  () -> new TestClass2ext().breakit()),
    };
    Breakpoint.Manager.BP[] tc2ext_breakpoints = new Breakpoint.Manager.BP[] {
      BP(breakit_method), BP(breakit_method_tc2), BP(breakit_method_tc2ext)
    };
    runTestGroups("TestClass2ext invokes", tc2ext_invokes, tc2ext_breakpoints);

    // Override default method and call it using interface-invoke-super
    Runnable[] tc3_invokes = new Runnable[] {
      new InvokeNative(breakit_method, new TestClass3()),
      new InvokeNative(breakit_method_tc3, new TestClass3()),

      new InvokeReflect(breakit_method, new TestClass3()),
      new InvokeReflect(breakit_method_tc3, new TestClass3()),

      new InvokeDirect("((Breakable)new TestClass3()).breakit()",
                  () -> ((Breakable)new TestClass3()).breakit()),
      new InvokeDirect("new TestClass3().breakit())",
                  () -> new TestClass3().breakit()),
    };
    Breakpoint.Manager.BP[] tc3_breakpoints = new Breakpoint.Manager.BP[] {
      BP(breakit_method), BP(breakit_method_tc3)
    };
    runTestGroups("TestClass3 invokes", tc3_invokes, tc3_breakpoints);

    // Call overridden method using invoke-super
    Runnable[] tc3ext_invokes = new Runnable[] {
      new InvokeNative(breakit_method, new TestClass3ext()),
      new InvokeNative(breakit_method_tc3, new TestClass3ext()),
      new InvokeNative(breakit_method_tc3ext, new TestClass3ext()),

      new InvokeReflect(breakit_method, new TestClass3ext()),
      new InvokeReflect(breakit_method_tc3, new TestClass3ext()),
      new InvokeReflect(breakit_method_tc3ext, new TestClass3ext()),

      new InvokeDirect("((Breakable)new TestClass3ext()).breakit()",
                  () -> ((Breakable)new TestClass3ext()).breakit()),
      new InvokeDirect("((TestClass3)new TestClass3ext()).breakit()",
                  () -> ((TestClass3)new TestClass3ext()).breakit()),
      new InvokeDirect("new TestClass3ext().breakit())",
                  () -> new TestClass3ext().breakit()),
    };
    Breakpoint.Manager.BP[] tc3ext_breakpoints = new Breakpoint.Manager.BP[] {
      BP(breakit_method), BP(breakit_method_tc3), BP(breakit_method_tc3ext)
    };
    runTestGroups("TestClass3ext invokes", tc3ext_invokes, tc3ext_breakpoints);

    // private instance method.
    Runnable[] private_instance_invokes = new Runnable[] {
      new InvokeNative(private_method, new TestClass4()),

      new InvokeDirect("new TestClass4().callPrivateMethod()",
                  () -> new TestClass4().callPrivateMethod()),
    };
    Breakpoint.Manager.BP[] private_instance_breakpoints = new Breakpoint.Manager.BP[] {
      BP(private_method)
    };
    runTestGroups(
        "private instance invoke", private_instance_invokes, private_instance_breakpoints);
  }

  private static void runTestGroups(String name,
                                    Runnable[] invokes,
                                    Breakpoint.Manager.BP[] breakpoints) throws Exception {
    System.out.println("Running " + name);
    for (List<Breakpoint.Manager.BP> bps : allCombinations(Arrays.asList(breakpoints))) {
      System.out.println("\tBreaking on " + bps);
      for (Runnable test : invokes) {
        MANAGER.clearAllBreakpoints();
        MANAGER.setBreakpoints(bps.toArray(new Breakpoint.Manager.BP[0]));
        test.run();
      }
    }
  }
}
