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

import java.util.Objects;

abstract class TestBase {
    static void assertTrue(boolean value) {
        if (!value) {
            throw new AssertionError("assertTrue value: " + value);
        }
    }

    static void assertEquals(byte b1, byte b2) {
        if (b1 == b2) {
            return;
        }
        throw new AssertionError("assertEquals b1: " + b1 + ", b2: " + b2);
    }

    static void assertEquals(char c1, char c2) {
        if (c1 == c2) {
            return;
        }
        throw new AssertionError("assertEquals c1: " + c1 + ", c2: " + c2);
    }

    static void assertEquals(short s1, short s2) {
        if (s1 == s2) {
            return;
        }
        throw new AssertionError("assertEquals s1: " + s1 + ", s2: " + s2);
    }

    static void assertEquals(int i1, int i2) {
        if (i1 == i2) {
            return;
        }
        throw new AssertionError("assertEquals i1: " + i1 + ", i2: " + i2);
    }

    static void assertEquals(long l1, long l2) {
        if (l1 == l2) {
            return;
        }
        throw new AssertionError("assertEquals l1: " + l1 + ", l2: " + l2);
    }

    static void assertEquals(float f1, float f2) {
        if (f1 == f2) {
            return;
        }
        throw new AssertionError("assertEquals f1: " + f1 + ", f2: " + f2);
    }

    static void assertEquals(double d1, double d2) {
        if (d1 == d2) {
            return;
        }
        throw new AssertionError("assertEquals d1: " + d1 + ", d2: " + d2);
    }

    static void assertEquals(Object o, Object p) {
        if (!Objects.equals(o, p)) {
            throw new AssertionError("assertEquals: o1: " + o + ", o2: " + p);
        }
    }

    static void assertNotEquals(Object o, Object p) {
        if (Objects.equals(o, p)) {
            throw new AssertionError("assertNotEquals: o1: " + o + ", o2: " + p);
        }
    }

    static void assertNotReached() {
        throw new AssertionError("Unreachable");
    }

    static void fail() {
        System.out.println("fail");
        Thread.dumpStack();
    }
}
