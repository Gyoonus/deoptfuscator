/*
 * Copyright (C) 2016 The Android Open Source Project
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

public class RacyMisbehavingHelper {
    public static ClassPair get() {
        Class<?> helper1_class = Helper1.class;
        Class<?> test_class = Test.class;
        try {
            // After loading the correct class, allow loading the incorrect class.
            ClassLoader loader = helper1_class.getClassLoader();
            Method reportAfterLoading = loader.getClass().getDeclaredMethod("reportAfterLoading");
            reportAfterLoading.invoke(loader);
        } catch (Throwable t) {
            t.printStackTrace(System.out);
        }
        return new ClassPair(helper1_class, test_class);
    }
}
