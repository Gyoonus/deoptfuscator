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

import dalvik.system.DexClassLoader;
import java.io.File;
import java.lang.reflect.Method;

/**
 * Structural hazard test.
 */
public class Main {
    public static void main(String[] args) {
        new Main().run();
    }

    private void run() {
        System.out.println(new A().i);

        // Now run the class from the -ex file.

        String dexPath = System.getenv("DEX_LOCATION") + "/138-duplicate-classes-check-ex.jar";
        String optimizedDirectory = System.getenv("DEX_LOCATION");
        String librarySearchPath = null;
        DexClassLoader loader = new DexClassLoader(dexPath, optimizedDirectory, librarySearchPath,
                getClass().getClassLoader());

        try {
            Class<?> testEx = loader.loadClass("TestEx");
            Method test = testEx.getDeclaredMethod("test");
            test.invoke(null);
        } catch (Exception exc) {
            exc.printStackTrace(System.out);
        }
    }
}
