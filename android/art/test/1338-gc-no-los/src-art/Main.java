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

import dalvik.system.VMRuntime;
import java.lang.ref.WeakReference;
import java.lang.reflect.Method;

public class Main {
    public static void main(String[] args) {
        try {
            // Allocate a large object.
            byte[] arr = new byte[128 * 1024];
            // Allocate a non movable object.
            byte[] arr2 = (byte[])VMRuntime.getRuntime().newNonMovableArray(Byte.TYPE, 200);
            // Put the array in a weak reference so that IsMarked is called by the GC.
            WeakReference weakRef = new WeakReference(arr2);
            // Do a GC.
            Runtime.getRuntime().gc();
            arr[0] = 1;
            arr2[0] = 1;
            System.out.println(arr.length + " " + arr2.length + " " + (weakRef.get() != null));
        } catch (Exception e) {
            System.out.println(e);
        }
    }
}
