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

public class StringIndexOfBenchmark {
    public static final String string36 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";  // length = 36

    public void timeIndexOf0(int count) {
        final char c = '0';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    public void timeIndexOf1(int count) {
        final char c = '1';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    public void timeIndexOf2(int count) {
        final char c = '2';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    public void timeIndexOf3(int count) {
        final char c = '3';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    public void timeIndexOf4(int count) {
        final char c = '4';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    public void timeIndexOf7(int count) {
        final char c = '7';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    public void timeIndexOf8(int count) {
        final char c = '8';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    public void timeIndexOfF(int count) {
        final char c = 'F';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    public void timeIndexOfG(int count) {
        final char c = 'G';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    public void timeIndexOfV(int count) {
        final char c = 'V';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    public void timeIndexOfW(int count) {
        final char c = 'W';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    public void timeIndexOf_(int count) {
        final char c = '_';
        String s = string36;
        for (int i = 0; i < count; ++i) {
            $noinline$indexOf(s, c);
        }
    }

    static int $noinline$indexOf(String s, char c) {
        if (doThrow) { throw new Error(); }
        return s.indexOf(c);
    }

    public static boolean doThrow = false;
}
