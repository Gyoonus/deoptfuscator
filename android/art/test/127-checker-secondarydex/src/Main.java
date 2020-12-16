/*
 * Copyright (C) 2014 The Android Open Source Project
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

import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

/**
 * Secondary dex file test.
 */
public class Main {
    public static void main(String[] args) {
        testSlowPathDirectInvoke();
        testString();
    }

    public static void testSlowPathDirectInvoke() {
        System.out.println("testSlowPathDirectInvoke");
        try {
            Test t1 = new Test();
            Test t2 = new Test();
            Test t3 = null;
            t1.test(t2);
            t1.test(t3);
        } catch (NullPointerException npe) {
            System.out.println("Got null pointer exception");
        } catch (Exception e) {
            System.out.println("Got unexpected exception " + e);
        }
    }

    // For string change, test that String.<init> is compiled properly in
    // secondary dex. See http://b/20870917
    public static void testString() {
        Test t = new Test();
        System.out.println(t.toString());
    }
}
