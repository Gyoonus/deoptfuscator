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

import java.util.concurrent.CountDownLatch;

/**
 * Make sure that a sub-thread can join the main thread.
 */
public class Main {
    public static void main(String[] args) throws Exception {
        Thread t;
        CountDownLatch waitLatch = new CountDownLatch(1);
        CountDownLatch progressLatch = new CountDownLatch(1);

        t = new Thread(new JoinMainSub(Thread.currentThread(), waitLatch, progressLatch), "Joiner");
        System.out.print("Starting thread '" + t.getName() + "'\n");
        t.start();

        waitLatch.await();
        System.out.print("JoinMain starter returning\n");
        progressLatch.countDown();

        // Keep the thread alive a little longer, giving the other thread a chance to join on a
        // live thread (though that isn't critically important for the test).
        Thread.currentThread().sleep(500);
    }
}

class JoinMainSub implements Runnable {
    private Thread mJoinMe;
    private CountDownLatch waitLatch;
    private CountDownLatch progressLatch;

    public JoinMainSub(Thread joinMe, CountDownLatch waitLatch, CountDownLatch progressLatch) {
        mJoinMe = joinMe;
        this.waitLatch = waitLatch;
        this.progressLatch = progressLatch;
    }

    public void run() {
        System.out.print("@ JoinMainSub running\n");

        try {
            waitLatch.countDown();
            progressLatch.await();
            mJoinMe.join();
            System.out.print("@ JoinMainSub successfully joined main\n");
        } catch (InterruptedException ie) {
            System.out.print("@ JoinMainSub interrupted!\n");
        }
        finally {
            System.out.print("@ JoinMainSub bailing\n");
        }
    }
}
