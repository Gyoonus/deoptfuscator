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

public final class Main {

    public static void main(String[] args) throws Exception {
        System.out.println("Test Started");
        testMissingFieldType();
        testMissingMethodReturnType();
        testMissingMethodParameterType();
        testMissingInnerClass();
        System.out.println("Test Finished");
    }

    private static class ClassWithMissingFieldType {
        MissingClass field;
    }

    private static void testMissingFieldType() throws Exception {
        try {
            ClassWithMissingFieldType.class.getDeclaredFields();
            throw new AssertionError();
        } catch (NoClassDefFoundError e) {
            System.out.println("testMissingFieldType caught NoClassDefFoundError");
        }
    }

    private static class ClassWithMissingMethodReturnType {
        MissingClass method() {
            return null;
        }
    }

    private static void testMissingMethodReturnType() throws Exception {
        try {
            ClassWithMissingMethodReturnType.class.getDeclaredMethods();
            throw new AssertionError();
        } catch (NoClassDefFoundError e) {
            System.out.println("testMissingMethodReturnType caught NoClassDefFoundError");
        }
    }

    private static class ClassWithMissingMethodParameterType {
        void method(MissingClass arg) {}
    }

    private static void testMissingMethodParameterType() throws Exception {
        try {
            ClassWithMissingMethodParameterType.class.getDeclaredMethods();
            throw new AssertionError();
        } catch (NoClassDefFoundError e) {
            System.out.println("testMissingMethodParameterType caught NoClassDefFoundError");
        }
    }

    private static final class MissingInnerClass {
    }

    private static void testMissingInnerClass() throws Exception {
        try {
            Main.class.getDeclaredClasses();
            throw new AssertionError();
        } catch (NoClassDefFoundError e) {
            System.out.println("testMissingInnerClass caught NoClassDefFoundError");
        }
    }
}
