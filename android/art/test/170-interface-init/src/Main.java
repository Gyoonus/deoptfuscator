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

import java.util.concurrent.CountDownLatch;

interface I {
}

class A implements I {
    static int x = (int)(10*Math.random());  // Suppress compile-time initialization.
}

public class Main {
    public static void main(String[] args) throws Exception {
        final CountDownLatch first = new CountDownLatch(1);
        final CountDownLatch second = new CountDownLatch(1);

        new Thread(new Runnable() {
            public void run() {
                try {
                    synchronized(I.class) {
                        first.countDown();
                        second.await();
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }).start();

        first.await();
        new A();  // Will initialize A.
        second.countDown();

        System.out.println("Done.");
    }
}
