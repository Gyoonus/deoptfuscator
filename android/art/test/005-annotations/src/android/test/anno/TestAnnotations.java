/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.test.anno;

import java.lang.annotation.Annotation;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.TreeMap;

public class TestAnnotations {
    /**
     * Print the annotations in sorted order, so as to avoid
     * any (legitimate) non-determinism with regard to the iteration order.
     */
    static private void printAnnotationArray(String prefix, Annotation[] arr) {
        TreeMap<String, Annotation> sorted =
            new TreeMap<String, Annotation>();

        for (Annotation a : arr) {
            sorted.put(a.annotationType().getName(), a);
        }

        for (Annotation a : sorted.values()) {
            System.out.println(prefix + "  " + a);
            System.out.println(prefix + "    " + a.annotationType());
        }
    }

    static void printAnnotations(Class<?> clazz) {
        Annotation[] annos;
        Annotation[][] parAnnos;

        annos = clazz.getAnnotations();
        System.out.println("annotations on TYPE " + clazz +
            "(" + annos.length + "):");
        printAnnotationArray("", annos);
        System.out.println();

        for (Constructor<?> c: clazz.getDeclaredConstructors()) {
            annos = c.getDeclaredAnnotations();
            System.out.println("  annotations on CTOR " + c + ":");
            printAnnotationArray("  ", annos);

            System.out.println("    constructor parameter annotations:");
            for (Annotation[] pannos: c.getParameterAnnotations()) {
                printAnnotationArray("    ", pannos);
            }
        }

        for (Method m: clazz.getDeclaredMethods()) {
            annos = m.getDeclaredAnnotations();
            System.out.println("  annotations on METH " + m + ":");
            printAnnotationArray("  ", annos);

            System.out.println("    method parameter annotations:");
            for (Annotation[] pannos: m.getParameterAnnotations()) {
                printAnnotationArray("    ", pannos);
            }
        }

        for (Field f: clazz.getDeclaredFields()) {
            annos = f.getDeclaredAnnotations();
            System.out.println("  annotations on FIELD " + f + ":");
            printAnnotationArray("  ", annos);

            AnnoFancyField aff;
            aff = (AnnoFancyField) f.getAnnotation(AnnoFancyField.class);
            if (aff != null) {
                System.out.println("    aff: " + aff + " / " + Proxy.isProxyClass(aff.getClass()));
                System.out.println("    --> nombre is '" + aff.nombre() + "'");
            }
        }
        System.out.println();
    }


    @ExportedProperty(mapping = {
        @IntToString(from = 0, to = "NORMAL_FOCUS"),
        @IntToString(from = 2, to = "WEAK_FOCUS")
    })
    public int getFocusType() {
        return 2;
    }


    @AnnoArrayField
    String thing1;

    @AnnoArrayField(
            zz = {true,false,true},
            bb = {-1,0,1},
            cc = {'Q'},
            ss = {12,13,14,15,16,17},
            ii = {1,2,3,4},
            ff = {1.1f,1.2f,1.3f},
            jj = {-5,0,5},
            dd = {0.3,0.6,0.9},
            str = {"hickory","dickory","dock"}
            )
    String thing2;

    public static void testArrays() {
        TestAnnotations ta = new TestAnnotations();
        Field field;
        Annotation[] annotations;

        try {
            field = TestAnnotations.class.getDeclaredField("thing1");
            annotations = field.getAnnotations();
            System.out.println(field + ": " + annotations[0].toString());

            field = TestAnnotations.class.getDeclaredField("thing2");
            annotations = field.getAnnotations();
            System.out.println(field + ": " + annotations[0].toString());
        } catch (NoSuchFieldException nsfe) {
            throw new RuntimeException(nsfe);
        }
    }

    public static void testArrayProblem() {
        Method meth;
        ExportedProperty property;
        final IntToString[] mapping;

        try {
            meth = TestAnnotations.class.getMethod("getFocusType");
        } catch (NoSuchMethodException nsme) {
            throw new RuntimeException(nsme);
        }
        property = meth.getAnnotation(ExportedProperty.class);
        mapping = property.mapping();

        System.out.println("mapping is " + mapping.getClass() +
            "\n  0='" + mapping[0] + "'\n  1='" + mapping[1] + "'");

        /* while we're here, check isAnnotationPresent on Method */
        System.out.println("present(getFocusType, ExportedProperty): " +
            meth.isAnnotationPresent(ExportedProperty.class));
        System.out.println("present(getFocusType, AnnoSimpleType): " +
            meth.isAnnotationPresent(AnnoSimpleType.class));

        System.out.println("");
    }

    public static void testVisibilityCompatibility() throws Exception {
        if (!VMRuntime.isAndroid()) {
            return;
        }
        Object runtime = VMRuntime.getRuntime();
        int currentSdkVersion = VMRuntime.getTargetSdkVersion(runtime);
        // SDK version 23 is M.
        int oldSdkVersion = 23;
        VMRuntime.setTargetSdkVersion(runtime, oldSdkVersion);
        // This annotation has CLASS retention, but is visible to the runtime in M and earlier.
        Annotation anno = SimplyNoted.class.getAnnotation(AnnoSimpleTypeInvis.class);
        if (anno == null) {
            System.out.println("testVisibilityCompatibility failed: " +
                    "SimplyNoted.get(AnnoSimpleTypeInvis) should not be null");
        }
        VMRuntime.setTargetSdkVersion(runtime, currentSdkVersion);
    }

    public static void main(String[] args) {
        System.out.println("TestAnnotations...");

        testArrays();
        testArrayProblem();

        System.out.println(
            "AnnoSimpleField " + AnnoSimpleField.class.isAnnotation() +
            ", SimplyNoted " + SimplyNoted.class.isAnnotation());

        printAnnotations(SimplyNoted.class);
        printAnnotations(INoted.class);
        printAnnotations(SubNoted.class);
        printAnnotations(FullyNoted.class);

        try {
            ClassWithInnerAnnotationClass.class.getDeclaredClasses();
            throw new AssertionError();
        } catch (NoClassDefFoundError expected) {
        }

        // this is expected to be non-null
        Annotation anno = SimplyNoted.class.getAnnotation(AnnoSimpleType.class);
        System.out.println("SimplyNoted.get(AnnoSimpleType) = " + anno);
        // this is expected to be null
        anno = SimplyNoted.class.getAnnotation(AnnoSimpleTypeInvis.class);
        System.out.println("SimplyNoted.get(AnnoSimpleTypeInvis) = " + anno);
        // this is non-null if the @Inherited tag is present
        anno = SubNoted.class.getAnnotation(AnnoSimpleType.class);
        System.out.println("SubNoted.get(AnnoSimpleType) = " + anno);

        System.out.println();

        // Package annotations aren't inherited, so getAnnotations and getDeclaredAnnotations are
        // the same.
        System.out.println("Package annotations:");
        printAnnotationArray("    ", TestAnnotations.class.getPackage().getAnnotations());
        System.out.println("Package declared annotations:");
        printAnnotationArray("    ", TestAnnotations.class.getPackage().getDeclaredAnnotations());

        System.out.println();

        // Test inner classes.
        System.out.println("Inner Classes:");
        new ClassWithInnerClasses().print();

        System.out.println();

        // Test TypeNotPresentException.
        try {
            AnnoMissingClass missingAnno =
                ClassWithMissingAnnotation.class.getAnnotation(AnnoMissingClass.class);
            System.out.println("Get annotation with missing class should not throw");
            System.out.println(missingAnno.value());
            System.out.println("Getting value of missing annotaton should have thrown");
        } catch (TypeNotPresentException expected) {
            System.out.println("Got expected TypeNotPresentException");
        }

        // Test renamed enums.
        try {
            for (Method m: RenamedNoted.class.getDeclaredMethods()) {
                Annotation[] annos = m.getDeclaredAnnotations();
                System.out.println("  annotations on METH " + m + ":");
            }
        } catch (NoSuchFieldError expected) {
            System.out.println("Got expected NoSuchFieldError");
        }

        // Test if annotations marked VISIBILITY_BUILD are visible to runtime in M and earlier.
        try {
            testVisibilityCompatibility();
        } catch (Exception e) {
            System.out.println("testVisibilityCompatibility failed: " + e);
        }
    }

    private static class VMRuntime {
        private static Class<?> vmRuntimeClass;
        private static Method getRuntimeMethod;
        private static Method getTargetSdkVersionMethod;
        private static Method setTargetSdkVersionMethod;
        static {
            init();
        }

        private static void init() {
            try {
                vmRuntimeClass = Class.forName("dalvik.system.VMRuntime");
            } catch (Exception e) {
                return;
            }
            try {
                getRuntimeMethod = vmRuntimeClass.getDeclaredMethod("getRuntime");
                getTargetSdkVersionMethod =
                        vmRuntimeClass.getDeclaredMethod("getTargetSdkVersion");
                setTargetSdkVersionMethod =
                        vmRuntimeClass.getDeclaredMethod("setTargetSdkVersion", Integer.TYPE);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static boolean isAndroid() {
            return vmRuntimeClass != null;
        }

        public static Object getRuntime() throws Exception {
            return getRuntimeMethod.invoke(null);
        }

        public static int getTargetSdkVersion(Object runtime) throws Exception {
            return (int) getTargetSdkVersionMethod.invoke(runtime);
        }

        public static void setTargetSdkVersion(Object runtime, int version) throws Exception {
            setTargetSdkVersionMethod.invoke(runtime, version);
        }
    }
}
