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

import other.InaccessibleClass;

public class Main {
    public static void main(String[] args) {
        try {
            testNoInline();
        } catch (IllegalAccessError e) {
            // expected
        }
        testInline();
    }

    /// CHECK-START: void Main.testNoInline() inliner (before)
    /// CHECK: InvokeStaticOrDirect method_name:Main.$opt$noinline$testNoInline

    /// CHECK-START: void Main.testNoInline() inliner (after)
    /// CHECK: InvokeStaticOrDirect method_name:Main.$opt$noinline$testNoInline
    public static void testNoInline() {
        $opt$noinline$testNoInline();
    }

    /// CHECK-START: void Main.testInline() inliner (before)
    /// CHECK: InvokeStaticOrDirect method_name:Main.$opt$inline$testInline

    /// CHECK-START: void Main.testInline() inliner (after)
    /// CHECK-NOT: InvokeStaticOrDirect
    public static void testInline() {
        $opt$inline$testInline();
    }

    public static boolean $opt$noinline$testNoInline() {
        try {
            return null instanceof InaccessibleClass;
        } catch (IllegalAccessError e) {
            // expected
        }
        return false;
    }

    public static boolean $opt$inline$testInline() {
        return null instanceof Main;
    }
}
