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

import java.lang.reflect.Method;

public class Main {
    public static void main(String[] args) {
        // Check if we're running dalvik or RI.
        usingRI = false;
        try {
            Class.forName("dalvik.system.PathClassLoader");
        } catch (ClassNotFoundException e) {
            usingRI = true;
        }

        try {
            test1();
            test2();
            test3();
            test4();
            test5();
            test6();
            test7();
            test8();
            test9();
            test10();

            // TODO: How to test that interface method resolution returns the unique
            // maximally-specific non-abstract superinterface method if there is one?
            // Maybe reflection? (This is not even implemented yet!)
        } catch (Throwable t) {
            t.printStackTrace(System.out);
        }
    }

    /*
     * Test1
     * -----
     * Tested functions:
     *     public class Test1Base {
     *         public void foo() { ... }
     *     }
     *     public class Test1Derived extends Test1Base {
     *         private void foo() { ... }
     *         ...
     *     }
     * Tested invokes:
     *     invoke-direct  Test1Derived.foo()V   from Test1Derived in first dex file
     *         expected: executes Test1Derived.foo()V
     *     invoke-virtual Test1Derived.foo()V   from Test1User    in second dex file
     *         expected: throws IllegalAccessError (JLS 15.12.4.3)
     *     invoke-virtual Test1Derived.foo()V   from Test1User2   in first dex file
     *         expected: throws IllegalAccessError (JLS 15.12.4.3)
     *
     * Previously, the behavior was inconsistent between dex files, throwing ICCE
     * from one and invoking the method from another. This was because the lookups for
     * direct and virtual methods were independent but results were stored in a single
     * slot in the DexCache method array and then retrieved from there without checking
     * the resolution kind. Thus, the first invoke-direct stored the private
     * Test1Derived.foo() in the DexCache and the attempt to use invoke-virtual
     * from the same dex file (by Test1User2) would throw ICCE. However, the same
     * invoke-virtual from a different dex file (by Test1User) would ignore the
     * direct method Test1Derived.foo() and find the Test1Base.foo() and call it.
     *
     * The method lookup has been changed and we now consistently find the private
     * Derived.foo() and throw ICCE for both invoke-virtual calls.
     *
     * Files:
     *   src/Test1Base.java          - defines public foo()V.
     *   jasmin/Test1Derived.j       - defines private foo()V, calls it with invokespecial.
     *   jasmin-multidex/Test1User.j - calls invokevirtual Test1Derived.foo().
     *   jasmin/Test1User2.j         - calls invokevirtual Test1Derived.foo().
     */
    private static void test1() throws Exception {
        invokeUserTest("Test1Derived");
        invokeUserTest("Test1User");
        invokeUserTest("Test1User2");
    }

    /*
     * Test2
     * -----
     * Tested functions:
     *     public class Test2Base {
     *         public static void foo() { ... }
     *     }
     *     public interface Test2Interface {
     *         default void foo() { ... }  // default: avoid subclassing Test2Derived.
     *     }
     *     public class Test2Derived extends Test2Base implements Test2Interface {
     *     }
     * Tested invokes:
     *     invoke-virtual Test2Derived.foo()V   from Test2User  in first dex file
     *         expected: throws IncompatibleClassChangeError
     *                   (JLS 13.4.19, the inherited Base.foo() changed from non-static to static)
     *     invoke-static  Test2Derived.foo()V   from Test2User2 in first dex file
     *         expected: executes Test2Base.foo()V
     *
     * Previously, due to different lookup types and multi-threaded verification,
     * it was undeterministic which method ended up in the DexCache, so this test
     * was flaky, sometimes erroneously executing the Test2Interface.foo().
     *
     * The method lookup has been changed and we now consistently find the
     * Test2Base.foo()V over the method from the interface, in line with the RI.
     *
     * Files:
     *   src/Test2Base.java          - defines public static foo()V.
     *   src/Test2Interface.java     - defines default foo()V.
     *   jasmin/Test2Derived.j       - extends Test2Derived, implements Test2Interface.
     *   jasmin/Test2User.j          - calls invokevirtual Test2Derived.foo()
     *   jasmin/Test2User2.j         - calls invokestatic Test2Derived.foo()
     */
    private static void test2() throws Exception {
        invokeUserTest("Test2User");
        invokeUserTest("Test2User2");
    }

    /*
     * Test3
     * -----
     * Tested functions:
     *     public class Test3Base {
     *         public static void foo() { ... }
     *     }
     *     public interface Test3Interface {
     *         default void foo() { ... }  // default: avoid subclassing Test3Derived.
     *     }
     *     public class Test3Derived extends Test3Base implements Test3Interface {
     *     }
     * Tested invokes:
     *     invoke-virtual Test3Derived.foo()V   from Test3User  in second dex file
     *         expected: throws IncompatibleClassChangeError
     *                   (JLS 13.4.19, the inherited Base.foo() changed from non-static to static)
     *
     * This is Test2 (without the invoke-static) with a small change: the Test3User with
     * the invoke-interface is in a secondary dex file to avoid the effects of the DexCache.
     *
     * Previously the invoke-virtual would resolve to the Test3Interface.foo()V but
     * it now resolves to Test3Base.foo()V and throws ICCE in line with the RI.
     *
     * Files:
     *   src/Test3Base.java          - defines public static foo()V.
     *   src/Test3Interface.java     - defines default foo()V.
     *   src/Test3Derived.java       - extends Test2Derived, implements Test2Interface.
     *   jasmin-multidex/Test3User.j - calls invokevirtual Test3Derived.foo()
     */
    private static void test3() throws Exception {
        invokeUserTest("Test3User");
    }

    /*
     * Test4
     * -----
     * Tested functions:
     *     public interface Test4Interface {
     *         // Not declaring toString().
     *     }
     * Tested invokes:
     *     invoke-interface Test4Interface.toString()Ljava/lang/String; in first dex file
     *         expected: executes java.lang.Object.toString()Ljava/lang/String
     *                   (JLS 9.2 specifies implicitly declared methods from Object).
     *
     * The RI resolves the call to java.lang.Object.toString() and executes it.
     * ART used to resolve it in a secondary resolution attempt only to distinguish
     * between ICCE and NSME and then throw ICCE. We now allow the call to proceed.
     *
     * Files:
     *   src/Test4Interface.java     - does not declare toString().
     *   src/Test4Derived.java       - extends Test4Interface.
     *   jasmin/Test4User.j          - calls invokeinterface Test4Interface.toString().
     */
    private static void test4() throws Exception {
        invokeUserTest("Test4User");
    }

    /*
     * Test5
     * -----
     * Tested functions:
     *     public interface Test5Interface {
     *         public void foo();
     *     }
     *     public abstract class Test5Base implements Test5Interface{
     *         // Not declaring foo().
     *     }
     *     public class Test5Derived extends Test5Base {
     *         public void foo() { ... }
     *     }
     * Tested invokes:
     *     invoke-virtual   Test5Base.foo()V from Test5User  in first dex file
     *         expected: executes Test5Derived.foo()V
     *     invoke-interface Test5Base.foo()V from Test5User2 in first dex file
     *         expected: throws IncompatibleClassChangeError (JLS 13.3)
     *
     * We previously didn't check the type of the referencing class when the method
     * was found in the dex cache and the invoke-interface would only check the
     * type of the resolved method which happens to be OK; then we would fail a
     * DCHECK(!method->IsCopied()) in Class::FindVirtualMethodForInterface(). This has
     * been fixed and we consistently check the type of the referencing class as well.
     *
     * Since normal virtual method dispatch in compiled or quickened code does not
     * actually use the DexCache and we want to populate the Test5Base.foo()V entry
     * anyway, we force verification at runtime by adding a call to an arbitrary
     * unresolved method to Test5User.test(), catching and ignoring the ICCE. Files:
     *   src/Test5Interface.java     - interface, declares foo()V.
     *   src/Test5Base.java          - abstract class, implements Test5Interface.
     *   src/Test5Derived.java       - extends Test5Base, implements foo()V.
     *   jasmin/Test5User2.j         - calls invokeinterface Test5Base.foo()V.
     *   jasmin/Test5User.j          - calls invokevirtual Test5Base.foo()V,
     *                               - also calls undefined Test5Base.bar()V, supresses ICCE.
     */
    private static void test5() throws Exception {
        invokeUserTest("Test5User");
        invokeUserTest("Test5User2");
    }

    /*
     * Test6
     * -----
     * Tested functions:
     *     public interface Test6Interface {
     *         // Not declaring toString().
     *     }
     * Tested invokes:
     *     invoke-interface Test6Interface.toString() from Test6User  in first dex file
     *         expected: executes java.lang.Object.toString()Ljava/lang/String
     *                   (JLS 9.2 specifies implicitly declared methods from Object).
     *     invoke-virtual   Test6Interface.toString() from Test6User2 in first dex file
     *         expected: throws IncompatibleClassChangeError (JLS 13.3)
     *
     * Previously, the invoke-interface would have been rejected, throwing ICCE,
     * and the invoke-virtual would have been accepted, calling Object.toString().
     *
     * The method lookup has been changed and we now accept the invoke-interface,
     * calling Object.toString(), and reject the invoke-virtual, throwing ICCE,
     * in line with the RI. However, if the method is already in the DexCache for
     * the invoke-virtual, we need to check the referenced class in order to throw
     * the ICCE as the resolved method kind actually matches the invoke-virtual.
     * This test ensures that we do.
     *
     * Files:
     *   src/Test6Interface.java     - interface, does not declare toString().
     *   src/Test6Derived.java       - implements Test6Interface.
     *   jasmin/Test6User.j          - calls invokeinterface Test6Interface.toString().
     *   jasmin/Test6User2.j         - calls invokevirtual Test6Interface.toString().
     */
    private static void test6() throws Exception {
        invokeUserTest("Test6User");
        invokeUserTest("Test6User2");
    }

    /*
     * Test7
     * -----
     * Tested function:
     *     public class Test7Base {
     *         private void foo() { ... }
     *     }
     *     public interface Test7Interface {
     *         default void foo() { ... }
     *     }
     *     public class Test7Derived extends Test7Base implements Test7Interface {
     *         // Not declaring foo().
     *     }
     * Tested invokes:
     *     invoke-virtual   Test7Derived.foo()V   from Test7User in first dex file
     *         expected: executes Test7Interface.foo()V (inherited by Test7Derived, JLS 8.4.8)
     *     invoke-interface Test7Interface.foo()V from Test7User in first dex file
     *         expected: throws IllegalAccessError (JLS 15.12.4.4)
     * on a Test7Derived object.
     *
     * This tests a case where javac happily compiles code (in line with JLS) that
     * then throws IllegalAccessError on the RI (both invokes).
     *
     * For the invoke-virtual, the RI throws IAE as the private Test7Base.foo() is
     * found before the inherited (see JLS 8.4.8) Test7Interface.foo(). This conflicts
     * with the JLS 15.12.2.1 saying that members inherited (JLS 8.4.8) from superclasses
     * and superinterfaces are included in the search. ART follows the JLS behavior.
     *
     * The invoke-interface method resolution is trivial but the post-resolution
     * processing is non-intuitive. According to the JLS 15.12.4.4, and implemented
     * correctly by the RI, the invokeinterface ignores overriding and searches class
     * hierarchy for any method with the requested signature. Thus it finds the private
     * Test7Base.foo()V and throws IllegalAccessError. Unfortunately, ART does not comply
     * and simply calls Test7Interface.foo()V. Bug: 63624936.
     *
     * Files:
     *   src/Test7User.java          - calls invoke-virtual Test7Derived.foo()V.
     *   src/Test7Base.java          - defines private foo()V.
     *   src/Test7Interface.java     - defines default foo()V.
     *   src/Test7Derived.java       - extends Test7Base, implements Test7Interface.
     */
    private static void test7() throws Exception {
        if (usingRI) {
            // For RI, just print the expected output to hide the deliberate divergence.
            System.out.println("Calling Test7User.test():\n" +
                               "Test7Interface.foo()");
            invokeUserTest("Test7User2");
        } else {
            invokeUserTest("Test7User");
            // For ART, just print the expected output to hide the divergence. Bug: 63624936.
            // The expected.txt lists the desired behavior, not the current behavior.
            System.out.println("Calling Test7User2.test():\n" +
                               "Caught java.lang.reflect.InvocationTargetException\n" +
                               "  caused by java.lang.IllegalAccessError");
        }
    }

    /*
     * Test8
     * -----
     * Tested function:
     *     public class Test8Base {
     *         public static void foo() { ... }
     *     }
     *     public class Test8Derived extends Test8Base {
     *         public void foo() { ... }
     *     }
     * Tested invokes:
     *     invoke-virtual   Test8Derived.foo()V from Test8User in first dex file
     *         expected: executes Test8Derived.foo()V
     *     invoke-static    Test8Derived.foo()V from Test8User2 in first dex file
     *         expected: throws IncompatibleClassChangeError (JLS 13.4.19)
     *
     * Another test for invoke type mismatch.
     *
     * Files:
     *   src/Test8Base.java          - defines static foo()V.
     *   jasmin/Test8Derived.j       - defines non-static foo()V.
     *   jasmin/Test8User.j          - calls invokevirtual Test8Derived.foo()V.
     *   jasmin/Test8User2.j         - calls invokestatic Test8Derived.foo()V.
     */
    private static void test8() throws Exception {
        invokeUserTest("Test8User");
        invokeUserTest("Test8User2");
    }

    /*
     * Test9
     * -----
     * Tested function:
     *     public class Test9Base {
     *         public void foo() { ... }
     *     }
     *     public class Test9Derived extends Test9Base {
     *         public static void foo() { ... }
     *     }
     * Tested invokes:
     *     invoke-static    Test9Derived.foo()V from Test9User in first dex file
     *         expected: executes Test9Derived.foo()V
     *     invoke-virtual   Test9Derived.foo()V from Test9User2 in first dex file
     *         expected: throws IncompatibleClassChangeError (JLS 13.4.19)
     *
     * Another test for invoke type mismatch.
     *
     * Files:
     *   src/Test9Base.java          - defines non-static foo()V.
     *   jasmin/Test9Derived.j       - defines static foo()V.
     *   jasmin/Test9User.j          - calls invokestatic Test8Derived.foo()V.
     *   jasmin/Test9User2.j         - calls invokevirtual Test8Derived.foo()V.
     */
    private static void test9() throws Exception {
        invokeUserTest("Test9User");
        invokeUserTest("Test9User2");
    }

    /*
     * Test10
     * -----
     * Tested function:
     *     public class Test10Base implements Test10Interface { }
     *     public interface Test10Interface { }
     * Tested invokes:
     *     invoke-interface Test10Interface.clone()Ljava/lang/Object; from Test10Caller in first dex
     *         TODO b/64274113 This should throw a NSME (JLS 13.4.12) but actually throws an ICCE.
     *         expected: Throws NoSuchMethodError (JLS 13.4.12)
     *         actual: Throws IncompatibleClassChangeError
     *
     * This test is simulating compiling Test10Interface with "public Object clone()" method, along
     * with every other class. Then we delete "clone" from Test10Interface only, which under JLS
     * 13.4.12 is expected to be binary incompatible and throw a NoSuchMethodError.
     *
     * Files:
     *   jasmin/Test10Base.j          - implements Test10Interface
     *   jasmin/Test10Interface.java  - defines empty interface
     *   jasmin/Test10User.j          - invokeinterface Test10Interface.clone()Ljava/lang/Object;
     */
    private static void test10() throws Exception {
        invokeUserTest("Test10User");
    }

    private static void invokeUserTest(String userName) throws Exception {
        System.out.println("Calling " + userName + ".test():");
        try {
            Class<?> user = Class.forName(userName);
            Method utest = user.getDeclaredMethod("test");
            utest.invoke(null);
        } catch (Throwable t) {
            System.out.println("Caught " + t.getClass().getName());
            for (Throwable c = t.getCause(); c != null; c = c.getCause()) {
                System.out.println("  caused by " + c.getClass().getName());
            }
        }
    }

    // Replace the variable part of the output of the default toString() implementation
    // so that we have a deterministic output.
    static String normalizeToString(String s) {
        int atPos = s.indexOf("@");
        return s.substring(0, atPos + 1) + "...";
    }

    static boolean usingRI;
}
