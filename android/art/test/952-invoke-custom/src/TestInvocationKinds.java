/*
 * Copyright (C) 2018 The Android Open Source Project
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

import annotations.BootstrapMethod;
import annotations.CalledByIndy;
import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

class TestInvocationKinds extends TestBase {
    private static int static_field;
    private double instance_field;

    static CallSite lookupStaticFieldGetter(
            MethodHandles.Lookup lookup, String name, MethodType methodType) throws Throwable {
        // methodType = "()LfieldType;"
        MethodHandle mh =
                lookup.findStaticGetter(TestInvocationKinds.class, name, methodType.returnType());
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestInvocationKinds.class,
                    name = "lookupStaticFieldSetter"
                ),
        fieldOrMethodName = "static_field",
        returnType = void.class,
        parameterTypes = {int.class}
    )
    private static void setStaticField(int value) {
        assertNotReached();
    }

    static CallSite lookupStaticFieldSetter(
            MethodHandles.Lookup lookup, String name, MethodType methodType) throws Throwable {
        // methodType = "(LfieldType;)V"
        MethodHandle mh =
                lookup.findStaticSetter(
                        TestInvocationKinds.class, name, methodType.parameterType(0));
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestInvocationKinds.class,
                    name = "lookupStaticFieldGetter"
                ),
        fieldOrMethodName = "static_field",
        returnType = int.class,
        parameterTypes = {}
    )
    private static int getStaticField() {
        assertNotReached();
        return 0;
    }

    static CallSite lookupInstanceFieldSetter(
            MethodHandles.Lookup lookup, String name, MethodType methodType) throws Throwable {
        // methodType = "(Lreceiver;LfieldType;)V"
        MethodHandle mh =
                lookup.findSetter(methodType.parameterType(0), name, methodType.parameterType(1));
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestInvocationKinds.class,
                    name = "lookupInstanceFieldSetter"
                ),
        fieldOrMethodName = "instance_field",
        returnType = void.class,
        parameterTypes = {TestInvocationKinds.class, double.class}
    )
    private static void setInstanceField(TestInvocationKinds instance, double value) {
        assertNotReached();
        instance.instance_field = Double.NaN;
    }

    static CallSite lookupInstanceFieldGetter(
            MethodHandles.Lookup lookup, String name, MethodType methodType) throws Throwable {
        // methodType = "(Lreceiver;)LfieldType;"
        MethodHandle mh =
                lookup.findGetter(methodType.parameterType(0), name, methodType.returnType());
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestInvocationKinds.class,
                    name = "lookupInstanceFieldGetter"
                ),
        fieldOrMethodName = "instance_field",
        returnType = double.class,
        parameterTypes = {TestInvocationKinds.class}
    )
    private static double getInstanceField(TestInvocationKinds instance) {
        assertNotReached();
        return Double.NaN;
    }

    private static void testStaticFieldAccessors() {
        System.out.println("testStaticFieldAccessors");
        setStaticField(3);
        assertEquals(static_field, 3);
        setStaticField(4);
        assertEquals(static_field, 4);
        assertEquals(static_field, getStaticField());
        static_field = Integer.MAX_VALUE;
        assertEquals(Integer.MAX_VALUE, getStaticField());
    }

    private static void testInstanceFieldAccessors() {
        System.out.println("testInstanceFieldAccessors");
        TestInvocationKinds instance = new TestInvocationKinds();
        instance.instance_field = Double.MIN_VALUE;
        setInstanceField(instance, Math.PI);
        assertEquals(Math.PI, instance.instance_field);
        instance.instance_field = Math.E;
        assertEquals(Math.E, getInstanceField(instance));
    }

    private static CallSite lookupVirtual(
            MethodHandles.Lookup lookup, String name, MethodType methodType) throws Throwable {
        // To get the point-of-use and invokedynamic to work the methodType here has the
        // receiver type as the leading paramter which needs to be dropped for findVirtual().
        MethodType mt = methodType.dropParameterTypes(0, 1);
        MethodHandle mh = lookup.findVirtual(TestInvocationKinds.class, name, mt);
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(enclosingType = TestInvocationKinds.class, name = "lookupVirtual"),
        fieldOrMethodName = "getMaxIntegerValue",
        returnType = int.class,
        parameterTypes = {TestInvocationKinds.class, int.class, int.class}
    )
    private static int maxIntegerValue(TestInvocationKinds receiver, int x, int y) {
        assertNotReached();
        return 0;
    }

    public int getMaxIntegerValue(int x, int y) {
        return x > y ? x : y;
    }

    static void testInvokeVirtual() {
        System.out.print("testInvokeVirtual => max(77, -3) = ");
        TestInvocationKinds receiver = new TestInvocationKinds();
        int result = maxIntegerValue(receiver, 77, -3);
        System.out.println(result);
    }

    static class Widget {
        int value;
        public Widget(int value) {}
    }

    private static CallSite lookupConstructor(
            MethodHandles.Lookup lookup, String name, MethodType methodType) throws Throwable {
        // methodType = (constructorParams);classToBeConstructed
        Class<?> cls = methodType.returnType();
        MethodType constructorMethodType = methodType.changeReturnType(void.class);
        MethodHandle mh = lookup.findConstructor(cls, constructorMethodType);
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestInvocationKinds.class,
                    name = "lookupConstructor"
                ),
        fieldOrMethodName = "unused",
        returnType = Widget.class,
        parameterTypes = {int.class}
    )
    private static Widget makeWidget(int v) {
        assertNotReached();
        return null;
    }

    static void testConstructor() {
        System.out.print("testConstructor => ");
        Widget receiver = makeWidget(3);
        assertEquals(Widget.class, receiver.getClass());
        System.out.println(receiver.getClass());
    }

    public static void test() {
        System.out.println(TestInvocationKinds.class.getName());
        testStaticFieldAccessors();
        testInstanceFieldAccessors();
        testInvokeVirtual();
        testConstructor();
    }
}
