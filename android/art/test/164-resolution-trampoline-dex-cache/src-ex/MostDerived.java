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

public class MostDerived extends Derived {
    public static void test(Class main) {
        // The defining class loader of MostDerived (MDCL) is also the initiating loader of
        // superclass Derived but delegates the loading to its parent class loader (PCL) which
        // defines both Derived and Base. Thus Derived.class is recorded in MDCL's ClassTable
        // but the Base.class is not because the Base's initiating loader is PCL. This is the
        // case when loading the MostDerived class and remains the case after resolving the
        // "invoke-super Derived.foo(.)" called from from MostDerived.foo(.). When that
        // invoke-super is executed from AOT-compiled code, it goes through the .bss ArtMethod*
        // entry and on first execution goes through the resolution method. After resolving to
        // the Base.foo(.), the artQuickResolutionTrampoline() used to erroneously fill the
        // Base.foo(.) entry in the MostDerived's DexCache which is wrong as the referenced
        // class Base is not in the associated, i.e. MDCL's, ClassTable.
        new MostDerived().foo(main);
        try {
            // This discrepancy then used to crash when resolving the Base.foo(.) method
            // for JIT compilation of another method.
            main.getDeclaredMethod("ensureJitCompiled", Class.class, String.class).invoke(
                    null, MostDerived.class, "bar");
        } catch (Throwable t) {
            t.printStackTrace(System.out);
        }
        System.out.println("MostDerived.test(.) done.");
    }

    public void foo(Class main) {
        super.foo(main);
    }

    public void bar(Class main) {
        Base b = this;
        b.foo(main);
    }
}
