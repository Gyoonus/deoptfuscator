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

import java.lang.ref.Reference;
import java.lang.ref.WeakReference;

public class Main {
    public static void main(String[] args) {
        System.out.println("Starting");
        WeakReference wrefs[] = new WeakReference[5];
        String str0 = generateString("String", 0);
        String str1 = generateString("String", 1);
        String str2 = generateString("String", 2);
        String str3 = generateString("String", 3);
        String str4 = generateString("String", 4);
        wrefs[0] = new WeakReference(str0);
        wrefs[1] = new WeakReference(str1);
        wrefs[2] = new WeakReference(str2);
        wrefs[3] = new WeakReference(str3);
        wrefs[4] = new WeakReference(str4);
        // Clear a couple as a sanity check.
        str1 = null;
        str2 = null;
        // str<n> dead here; in the future we will possibly reuse the registers.
        // Give the compiler something to fill the registers with.
        String str5 = generateString("String", 5);
        String str6 = generateString("String", 6);
        String str7 = generateString("String", 7);
        String str8 = generateString("String", 8);
        String str9 = generateString("String", 9);
        Runtime.getRuntime().gc();
        for (int i = 0; i < 5; ++i) {
          if (wrefs[i].get() != null) {
            System.out.println("Reference " + i + " was live.");
          }
        }
        Reference.reachabilityFence(str0);
        Reference.reachabilityFence(str1);
        Reference.reachabilityFence(str2);
        Reference.reachabilityFence(str3);
        Reference.reachabilityFence(str4);
        System.out.println("Finished");
    }

    private static String generateString(String base, int num) {
        return base + num;
    }
}
