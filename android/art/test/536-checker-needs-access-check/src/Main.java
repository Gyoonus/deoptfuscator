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

import other.InaccessibleClass;
import other.InaccessibleClassProxy;

public class Main {
    public static void main(String[] args) {
        try {
            testInstanceOf();
        } catch (IllegalAccessError e) {
            System.out.println("Got expected error instanceof");
        }

         try {
            testInstanceOfNull();
        } catch (IllegalAccessError e) {
            System.out.println("Got expected error instanceof null");
        }

        try {
            testCheckCastNull();
        } catch (IllegalAccessError e) {
            System.out.println("Got expected error checkcast null");
        }

        try {
            testDontGvnLoadClassWithAccessChecks(new Object());
        } catch (IllegalAccessError e) {
            System.out.println("Got expected error instanceof (keep LoadClass with access check)");
        }
    }

    /// CHECK-START: boolean Main.testInstanceOf() register (after)
    /// CHECK: InstanceOf
    public static boolean testInstanceOf() {
        return ic instanceof InaccessibleClass;
    }

    /// CHECK-START: boolean Main.testInstanceOfNull() register (after)
    /// CHECK: InstanceOf
    public static boolean testInstanceOfNull() {
        return null instanceof InaccessibleClass;
    }

    // TODO: write a test for for CheckCast with not null constant (after RTP can parse arguments).

    /// CHECK-START: other.InaccessibleClass Main.testCheckCastNull() register (after)
    /// CHECK: CheckCast
    public static InaccessibleClass testCheckCastNull() {
        return (InaccessibleClass) null;
    }

    /// CHECK-START: boolean Main.testDontGvnLoadClassWithAccessChecks(java.lang.Object) inliner (before)
    /// CHECK: InvokeStaticOrDirect

    /// CHECK-START: boolean Main.testDontGvnLoadClassWithAccessChecks(java.lang.Object) inliner (after)
    /// CHECK-NOT: InvokeStaticOrDirect

    /// CHECK-START: boolean Main.testDontGvnLoadClassWithAccessChecks(java.lang.Object) GVN (after)
    /// CHECK: LoadClass needs_access_check:false
    /// CHECK: LoadClass needs_access_check:true
    public static boolean testDontGvnLoadClassWithAccessChecks(Object o) {
        InaccessibleClassProxy.test(o);
        return ic instanceof InaccessibleClass;
    }

    public static InaccessibleClass ic;
}
