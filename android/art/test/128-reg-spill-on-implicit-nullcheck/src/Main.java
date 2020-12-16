/*
 * Copyright (C) 2007 The Android Open Source Project
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
        int t7q = 0;
        long q = 1L;

        try {
            for (int i = 1; i < 8; i++) {
                t7q = (--t7q);
                TestClass f = null;
                t7q = f.field;
            }
        }
        catch (NullPointerException wpw) {
            q++;
        }
        finally {
            t7q += (int)(1 - ((q - q) - 2));
        }

        System.out.println("t7q = " + t7q);
    }
}

class TestClass {
    public int field;
    public void meth() {field = 1;}
}
