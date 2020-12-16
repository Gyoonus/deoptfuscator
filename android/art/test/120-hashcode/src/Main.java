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

public class Main {
    public static void main(String[] args) {
        Object o = new Object();
        // Generate a hashcode and put it in the lock word.
        int hashOrig = o.hashCode();
        int hashInflated = 0;
        int hashSystemOrig = System.identityHashCode(o);
        int hashSystemInflated = 0;
        // Inflate the monitor to move the hash from the lock word to the Monitor.
        synchronized (o) {
            hashInflated = o.hashCode();
            hashSystemInflated = System.identityHashCode(o);
        }
        // Make sure that all the hashes agree.
        if (hashOrig != hashInflated || hashOrig != hashSystemOrig ||
            hashSystemOrig != hashSystemInflated) {
            System.out.println("hash codes dont match: " + hashOrig + " " + hashInflated + " " +
            hashSystemOrig + " " + hashSystemInflated);
        }
        System.out.println("Done.");
    }
}


