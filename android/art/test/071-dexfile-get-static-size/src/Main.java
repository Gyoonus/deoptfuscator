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
    private static long getDexFileSize(String filename) throws Exception {
        ClassLoader loader = Main.class.getClassLoader();
        Class<?> DexFile = loader.loadClass("dalvik.system.DexFile");
        Method DexFile_loadDex = DexFile.getMethod("loadDex",
                                                   String.class,
                                                   String.class,
                                                   Integer.TYPE);
        Method DexFile_getStaticSizeOfDexFile = DexFile.getMethod("getStaticSizeOfDexFile");
        Object dexFile = DexFile_loadDex.invoke(null, filename, null, 0);
        return (Long) DexFile_getStaticSizeOfDexFile.invoke(dexFile);
    }

    private static void test(String resource) throws Exception {
        String filename = System.getenv("DEX_LOCATION") + "/res/" + resource;
        long size = getDexFileSize(filename);
        System.out.println("Size for " + resource + ": " + size);
    }

    public static void main(String[] args) throws Exception {
        test("test1.dex");
        test("test2.dex");
        test("test-jar.jar");
        test("multi-jar.jar");
    }
}
