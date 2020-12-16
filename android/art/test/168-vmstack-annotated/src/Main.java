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

import java.lang.Thread.State;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.BrokenBarrierException;
import java.util.concurrent.CyclicBarrier;

public class Main {

    static class Runner implements Runnable {
        List<Object> locks;
        List<CyclicBarrier> barriers;

        public Runner(List<Object> locks, List<CyclicBarrier> barriers) {
            this.locks = locks;
            this.barriers = barriers;
        }

        @Override
        public void run() {
            step(locks, barriers);
        }

        private void step(List<Object> l, List<CyclicBarrier> b) {
            if (l.isEmpty()) {
                // Nothing to do, sleep indefinitely.
                try {
                    Thread.sleep(100000000);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            } else {
                Object lockObject = l.remove(0);
                CyclicBarrier barrierObject = b.remove(0);

                if (lockObject == null) {
                    // No lock object: only take barrier, recurse.
                    try {
                        barrierObject.await();
                    } catch (InterruptedException | BrokenBarrierException e) {
                        throw new RuntimeException(e);
                    }
                    step(l, b);
                } else if (barrierObject != null) {
                    // Have barrier: sync, wait and recurse.
                    synchronized(lockObject) {
                        try {
                            barrierObject.await();
                        } catch (InterruptedException | BrokenBarrierException e) {
                            throw new RuntimeException(e);
                        }
                        step(l, b);
                    }
                } else {
                    // Sync, and get next step (which is assumed to have object and barrier).
                    synchronized (lockObject) {
                        Object lockObject2 = l.remove(0);
                        CyclicBarrier barrierObject2 = b.remove(0);
                        synchronized(lockObject2) {
                            try {
                                barrierObject2.await();
                            } catch (InterruptedException | BrokenBarrierException e) {
                                throw new RuntimeException(e);
                            }
                            step(l, b);
                        }
                    }
                }
            }
        }
    }

    public static void main(String[] args) throws Exception {
        try {
            testCluster1();
        } catch (Exception e) {
            Map<Thread,StackTraceElement[]> stacks = Thread.getAllStackTraces();
            for (Map.Entry<Thread,StackTraceElement[]> entry : stacks.entrySet()) {
                System.out.println(entry.getKey());
                System.out.println(Arrays.toString(entry.getValue()));
            }
            throw e;
        }
    }

    private static void testCluster1() throws Exception {
        // Test setup (at deadlock):
        //
        // Thread 1:
        //   #0 step: synchornized(o3) { synchronized(o2) }
        //   #1 step: synchronized(o1)
        //
        // Thread 2:
        //   #0 step: synchronized(o1)
        //   #1 step: synchronized(o4) { synchronized(o2) }
        //
        LinkedList<Object> l1 = new LinkedList<>();
        LinkedList<CyclicBarrier> b1 = new LinkedList<>();
        LinkedList<Object> l2 = new LinkedList<>();
        LinkedList<CyclicBarrier> b2 = new LinkedList<>();

        Object o1 = new Object();
        Object o2 = new Object();
        Object o3 = new Object();
        Object o4 = new Object();

        l1.add(o1);
        l1.add(o3);
        l1.add(o2);
        l2.add(o4);
        l2.add(o2);
        l2.add(o1);

        CyclicBarrier c1 = new CyclicBarrier(3);
        CyclicBarrier c2 = new CyclicBarrier(2);
        b1.add(c1);
        b1.add(null);
        b1.add(c2);
        b2.add(null);
        b2.add(c1);
        b2.add(c2);

        Thread t1 = new Thread(new Runner(l1, b1));
        t1.setDaemon(true);
        t1.start();
        Thread t2 = new Thread(new Runner(l2, b2));
        t2.setDaemon(true);
        t2.start();

        c1.await();

        waitNotRunnable(t1);
        waitNotRunnable(t2);
        Thread.sleep(250);    // Unfortunately this seems necessary. :-(

        // Thread 1.
        {
            Object[] stack1 = getAnnotatedStack(t1);
            assertBlockedOn(stack1[0], o2);              // Blocked on o2.
            assertLocks(stack1[0], o3);                  // Locked o3.
            assertStackTraceElementStep(stack1[0]);

            assertBlockedOn(stack1[1], null);            // Frame can't be blocked.
            assertLocks(stack1[1], o1);                  // Locked o1.
            assertStackTraceElementStep(stack1[1]);
        }

        // Thread 2.
        {
            Object[] stack2 = getAnnotatedStack(t2);
            assertBlockedOn(stack2[0], o1);              // Blocked on o1.
            assertLocks(stack2[0]);                      // Nothing locked.
            assertStackTraceElementStep(stack2[0]);

            assertBlockedOn(stack2[1], null);            // Frame can't be blocked.
            assertLocks(stack2[1], o4, o2);              // Locked o4, o2.
            assertStackTraceElementStep(stack2[1]);
        }
    }

    private static void waitNotRunnable(Thread t) throws InterruptedException {
        while (t.getState() == State.RUNNABLE) {
            Thread.sleep(100);
        }
    }

    private static Object[] getAnnotatedStack(Thread t) throws Exception {
        Class<?> vmStack = Class.forName("dalvik.system.VMStack");
        Method m = vmStack.getDeclaredMethod("getAnnotatedThreadStackTrace", Thread.class);
        return (Object[]) m.invoke(null, t);
    }

    private static void assertEquals(Object o1, Object o2) {
        if (o1 != o2) {
            throw new RuntimeException("Expected " + o1 + " == " + o2);
        }
    }
    private static void assertLocks(Object fromTrace, Object... locks) throws Exception {
        Object fieldValue = fromTrace.getClass().getDeclaredMethod("getHeldLocks").
                invoke(fromTrace);
        assertEquals((Object[]) fieldValue,
                (locks == null) ? null : (locks.length == 0 ? null : locks));
    }
    private static void assertBlockedOn(Object fromTrace, Object block) throws Exception {
        Object fieldValue = fromTrace.getClass().getDeclaredMethod("getBlockedOn").
                invoke(fromTrace);
        assertEquals(fieldValue, block);
    }
    private static void assertEquals(Object[] o1, Object[] o2) {
        if (!Arrays.equals(o1, o2)) {
            throw new RuntimeException(
                    "Expected " + Arrays.toString(o1) + " == " + Arrays.toString(o2));
        }
    }
    private static void assertStackTraceElementStep(Object o) throws Exception {
        Object fieldValue = o.getClass().getDeclaredMethod("getStackTraceElement").invoke(o);
        if (fieldValue instanceof StackTraceElement) {
            StackTraceElement elem = (StackTraceElement) fieldValue;
            if (!elem.getMethodName().equals("step")) {
                throw new RuntimeException("Expected step method");
            }
            return;
        }
        throw new RuntimeException("Expected StackTraceElement " + fieldValue + " / " + o);
    }
}

