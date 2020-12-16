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

public class Main {
    public static void main(String[] args) {
        Main m = new Main();
        Nested n = new Nested();
        n.$noinline$setPrivateIntField(m, 42);
        System.out.println(n.$noinline$getPrivateIntField(m));
    }

    private int privateIntField;

    private static class Nested {
        /// CHECK-START: void Main$Nested.$noinline$setPrivateIntField(Main, int) inliner (before)
        /// CHECK:                  InvokeStaticOrDirect

        /// CHECK-START: void Main$Nested.$noinline$setPrivateIntField(Main, int) inliner (before)
        /// CHECK-NOT:              InstanceFieldSet

        /// CHECK-START: void Main$Nested.$noinline$setPrivateIntField(Main, int) inliner (after)
        /// CHECK-NOT:              InvokeStaticOrDirect

        /// CHECK-START: void Main$Nested.$noinline$setPrivateIntField(Main, int) inliner (after)
        /// CHECK:                  InstanceFieldSet

        public void $noinline$setPrivateIntField(Main m, int value) {
            m.privateIntField = value;
        }

        /// CHECK-START: int Main$Nested.$noinline$getPrivateIntField(Main) inliner (before)
        /// CHECK:                  InvokeStaticOrDirect

        /// CHECK-START: int Main$Nested.$noinline$getPrivateIntField(Main) inliner (before)
        /// CHECK-NOT:              InstanceFieldGet

        /// CHECK-START: int Main$Nested.$noinline$getPrivateIntField(Main) inliner (after)
        /// CHECK-NOT:              InvokeStaticOrDirect

        /// CHECK-START: int Main$Nested.$noinline$getPrivateIntField(Main) inliner (after)
        /// CHECK:                  InstanceFieldGet

        public int $noinline$getPrivateIntField(Main m) {
            return m.privateIntField;
        }
    }
}
