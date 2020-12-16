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

import java.lang.reflect.Method;
import dalvik.system.AnnotatedStackTraceElement;

public class Main {
    public static void main(String args[]) throws Exception {
        Class<?> vmStack = Class.forName("dalvik.system.VMStack");
        Method getAnnotatedThreadStackTrace =
                vmStack.getDeclaredMethod("getAnnotatedThreadStackTrace", Thread.class);
        Object[] annotatedStackTrace =
                (Object[]) getAnnotatedThreadStackTrace.invoke(null, Thread.currentThread());
        AnnotatedStackTraceElement annotatedElement =
            (AnnotatedStackTraceElement) annotatedStackTrace[0];
        // This used to fail an assertion that the AnnotatedStackTraceElement.class
        // is at least initializing (i.e. initializing, initialized or resolved-erroneous).
        // Note: We cannot use reflection for this test because getDeclaredMethod() would
        // initialize the class and hide the failure.
        annotatedElement.getStackTraceElement();

        System.out.println("passed");
    }
}
