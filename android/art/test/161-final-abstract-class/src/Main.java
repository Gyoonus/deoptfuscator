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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
    public static void main(String[] args) {
        try {
            // Make sure that the abstract final class is marked as erroneous.
            Class.forName("AbstractFinal");
            System.out.println("UNREACHABLE!");
        } catch (VerifyError expected) {
        } catch (Throwable t) {
            t.printStackTrace(System.out);
        }
        try {
            // Verification of TestClass.test() used to crash when processing
            // the final abstract (erroneous) class.
            Class<?> tc = Class.forName("TestClass");
            Method test = tc.getDeclaredMethod("test");
            test.invoke(null);
            System.out.println("UNREACHABLE!");
        } catch (InvocationTargetException ite) {
            if (ite.getCause() instanceof InstantiationError) {
                System.out.println(
                    ite.getCause().getClass().getName() + ": " + ite.getCause().getMessage());
            } else {
                ite.printStackTrace(System.out);
            }
        } catch (Throwable t) {
            t.printStackTrace(System.out);
        }
    }
}
