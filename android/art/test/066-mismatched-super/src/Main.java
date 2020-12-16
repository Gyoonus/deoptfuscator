/*
 * Copyright (C) 2008 The Android Open Source Project
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

/*
 * Test field access through reflection.
 */
public class Main {
    public static void main(String[] args) {
        try {
            Base base = new Base();
            System.out.println("Succeeded unexpectedly");
        } catch (IncompatibleClassChangeError icce) {
            System.out.println("Got expected ICCE");
        }
        try {
            ExtendsFinal ef = new ExtendsFinal();
            System.out.println("Succeeded unexpectedly");
        } catch (VerifyError ve) {
            System.out.println("Got expected VerifyError");
        }
    }
}
