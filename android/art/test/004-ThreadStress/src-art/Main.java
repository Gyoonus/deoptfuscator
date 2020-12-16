/*
 * Copyright (C) 2011 The Android Open Source Project
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

import dalvik.system.VMRuntime;

import java.lang.reflect.*;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Semaphore;

// Run on host with:
//   javac ThreadTest.java && java ThreadStress && rm *.class
// Through run-test:
//   test/run-test {run-test-args} 004-ThreadStress [Main {ThreadStress-args}]
//   (It is important to pass Main if you want to give parameters...)
//
// ThreadStress command line parameters:
//    -n X .............. number of threads
//    -d X .............. number of daemon threads
//    -o X .............. number of overall operations
//    -t X .............. number of operations per thread
//    -p X .............. number of permits granted by semaphore
//    --dumpmap ......... print the frequency map
//    --locks-only ...... select a pre-set frequency map with lock-related operations only
//    --allocs-only ..... select a pre-set frequency map with allocation-related operations only
//    -oom:X ............ frequency of OOM (double)
//    -sigquit:X ........ frequency of SigQuit (double)
//    -alloc:X .......... frequency of Alloc (double)
//    -largealloc:X ..... frequency of LargeAlloc (double)
//    -nonmovingalloc:X.. frequency of NonMovingAlloc (double)
//    -stacktrace:X ..... frequency of StackTrace (double)
//    -exit:X ........... frequency of Exit (double)
//    -sleep:X .......... frequency of Sleep (double)
//    -wait:X ........... frequency of Wait (double)
//    -timedwait:X ...... frequency of TimedWait (double)
//    -syncandwork:X .... frequency of SyncAndWork (double)
//    -queuedwait:X ..... frequency of QueuedWait (double)

public class Main implements Runnable {

    public static final boolean DEBUG = false;

    private static abstract class Operation {
        /**
         * Perform the action represented by this operation. Returns true if the thread should
         * continue when executed by a runner (non-daemon) thread.
         */
        public abstract boolean perform();
    }

    private final static class OOM extends Operation {
        private final static int ALLOC_SIZE = 1024;

        @Override
        public boolean perform() {
            try {
                List<byte[]> l = new ArrayList<byte[]>();
                while (true) {
                    l.add(new byte[ALLOC_SIZE]);
                }
            } catch (OutOfMemoryError e) {
            }
            return true;
        }
    }

    private final static class SigQuit extends Operation {
        private final static int sigquit;
        private final static Method kill;
        private final static int pid;

        static {
            int pidTemp = -1;
            int sigquitTemp = -1;
            Method killTemp = null;

            try {
                Class<?> osClass = Class.forName("android.system.Os");
                Method getpid = osClass.getDeclaredMethod("getpid");
                pidTemp = (Integer)getpid.invoke(null);

                Class<?> osConstants = Class.forName("android.system.OsConstants");
                Field sigquitField = osConstants.getDeclaredField("SIGQUIT");
                sigquitTemp = (Integer)sigquitField.get(null);

                killTemp = osClass.getDeclaredMethod("kill", int.class, int.class);
            } catch (Exception e) {
                Main.printThrowable(e);
            }

            pid = pidTemp;
            sigquit = sigquitTemp;
            kill = killTemp;
        }

        @Override
        public boolean perform() {
            try {
                kill.invoke(null, pid, sigquit);
            } catch (OutOfMemoryError e) {
            } catch (Exception e) {
                if (!e.getClass().getName().equals(Main.errnoExceptionName)) {
                    Main.printThrowable(e);
                }
            }
            return true;
        }
    }

    private final static class Alloc extends Operation {
        private final static int ALLOC_SIZE = 1024;  // Needs to be small enough to not be in LOS.
        private final static int ALLOC_COUNT = 1024;

        @Override
        public boolean perform() {
            try {
                List<byte[]> l = new ArrayList<byte[]>();
                for (int i = 0; i < ALLOC_COUNT; i++) {
                    l.add(new byte[ALLOC_SIZE]);
                }
            } catch (OutOfMemoryError e) {
            }
            return true;
        }
    }

    private final static class LargeAlloc extends Operation {
        private final static int PAGE_SIZE = 4096;
        private final static int PAGE_SIZE_MODIFIER = 10;  // Needs to be large enough for LOS.
        private final static int ALLOC_COUNT = 100;

        @Override
        public boolean perform() {
            try {
                List<byte[]> l = new ArrayList<byte[]>();
                for (int i = 0; i < ALLOC_COUNT; i++) {
                    l.add(new byte[PAGE_SIZE_MODIFIER * PAGE_SIZE]);
                }
            } catch (OutOfMemoryError e) {
            }
            return true;
        }
    }

  private final static class NonMovingAlloc extends Operation {
        private final static int ALLOC_SIZE = 1024;  // Needs to be small enough to not be in LOS.
        private final static int ALLOC_COUNT = 1024;
        private final static VMRuntime runtime = VMRuntime.getRuntime();

        @Override
        public boolean perform() {
            try {
                List<byte[]> l = new ArrayList<byte[]>();
                for (int i = 0; i < ALLOC_COUNT; i++) {
                    l.add((byte[]) runtime.newNonMovableArray(byte.class, ALLOC_SIZE));
                }
            } catch (OutOfMemoryError e) {
            }
            return true;
        }
    }


    private final static class StackTrace extends Operation {
        @Override
        public boolean perform() {
            try {
                Thread.currentThread().getStackTrace();
            } catch (OutOfMemoryError e) {
            }
            return true;
        }
    }

    private final static class Exit extends Operation {
        @Override
        public boolean perform() {
            return false;
        }
    }

    private final static class Sleep extends Operation {
        private final static int SLEEP_TIME = 100;

        @Override
        public boolean perform() {
            try {
                Thread.sleep(SLEEP_TIME);
            } catch (InterruptedException ignored) {
            }
            return true;
        }
    }

    private final static class TimedWait extends Operation {
        private final static int SLEEP_TIME = 100;

        private final Object lock;

        public TimedWait(Object lock) {
            this.lock = lock;
        }

        @Override
        public boolean perform() {
            synchronized (lock) {
                try {
                    lock.wait(SLEEP_TIME, 0);
                } catch (InterruptedException ignored) {
                }
            }
            return true;
        }
    }

    private final static class Wait extends Operation {
        private final Object lock;

        public Wait(Object lock) {
            this.lock = lock;
        }

        @Override
        public boolean perform() {
            synchronized (lock) {
                try {
                    lock.wait();
                } catch (InterruptedException ignored) {
                }
            }
            return true;
        }
    }

    private final static class SyncAndWork extends Operation {
        private final Object lock;

        public SyncAndWork(Object lock) {
            this.lock = lock;
        }

        @Override
        public boolean perform() {
            synchronized (lock) {
                try {
                    Thread.sleep((int)(Math.random() * 50 + 50));
                } catch (InterruptedException ignored) {
                }
            }
            return true;
        }
    }

    // An operation requiring the acquisition of a permit from a semaphore
    // for its execution. This operation has been added to exercise
    // java.util.concurrent.locks.AbstractQueuedSynchronizer, used in the
    // implementation of java.util.concurrent.Semaphore. We use the latter,
    // as the former is not supposed to be used directly (see b/63822989).
    private final static class QueuedWait extends Operation {
        private final static int SLEEP_TIME = 100;

        private final Semaphore semaphore;

        public QueuedWait(Semaphore semaphore) {
            this.semaphore = semaphore;
        }

        @Override
        public boolean perform() {
            boolean permitAcquired = false;
            try {
                semaphore.acquire();
                permitAcquired = true;
                Thread.sleep(SLEEP_TIME);
            } catch (OutOfMemoryError ignored) {
              // The call to semaphore.acquire() above may trigger an OOME,
              // despite the care taken doing some warm-up by forcing
              // ahead-of-time initialization of classes used by the Semaphore
              // class (see forceTransitiveClassInitialization below).
              // For instance, one of the code paths executes
              // AbstractQueuedSynchronizer.addWaiter, which allocates an
              // AbstractQueuedSynchronizer$Node (see b/67730573).
              // In that case, just ignore the OOME and continue.
            } catch (InterruptedException ignored) {
            } finally {
                if (permitAcquired) {
                    semaphore.release();
                }
            }
            return true;
        }
    }

    private final static Map<Operation, Double> createDefaultFrequencyMap(Object lock,
            Semaphore semaphore) {
        Map<Operation, Double> frequencyMap = new HashMap<Operation, Double>();
        frequencyMap.put(new OOM(), 0.005);                   //   1/200
        frequencyMap.put(new SigQuit(), 0.095);               //  19/200
        frequencyMap.put(new Alloc(), 0.225);                 //  45/200
        frequencyMap.put(new LargeAlloc(), 0.05);             //  10/200
        // TODO: NonMovingAlloc operations fail an assertion with the
        // GSS collector (see b/72738921); disable them for now.
        frequencyMap.put(new NonMovingAlloc(), 0.0);          //   0/200
        frequencyMap.put(new StackTrace(), 0.1);              //  20/200
        frequencyMap.put(new Exit(), 0.225);                  //  45/200
        frequencyMap.put(new Sleep(), 0.125);                 //  25/200
        frequencyMap.put(new TimedWait(lock), 0.05);          //  10/200
        frequencyMap.put(new Wait(lock), 0.075);              //  15/200
        frequencyMap.put(new QueuedWait(semaphore), 0.05);    //  10/200

        return frequencyMap;
    }

    private final static Map<Operation, Double> createAllocFrequencyMap() {
        Map<Operation, Double> frequencyMap = new HashMap<Operation, Double>();
        frequencyMap.put(new Sleep(), 0.2);                   //  40/200
        frequencyMap.put(new Alloc(), 0.575);                 // 115/200
        frequencyMap.put(new LargeAlloc(), 0.15);             //  30/200
        frequencyMap.put(new NonMovingAlloc(), 0.075);        //  15/200

        return frequencyMap;
    }

    private final static Map<Operation, Double> createLockFrequencyMap(Object lock) {
      Map<Operation, Double> frequencyMap = new HashMap<Operation, Double>();
      frequencyMap.put(new Sleep(), 0.2);                     //  40/200
      frequencyMap.put(new TimedWait(lock), 0.2);             //  40/200
      frequencyMap.put(new Wait(lock), 0.2);                  //  40/200
      frequencyMap.put(new SyncAndWork(lock), 0.4);           //  80/200

      return frequencyMap;
    }

    public static void main(String[] args) throws Exception {
        System.loadLibrary(args[0]);
        parseAndRun(args);
    }

    private static Map<Operation, Double> updateFrequencyMap(Map<Operation, Double> in,
            Object lock, Semaphore semaphore, String arg) {
        String split[] = arg.split(":");
        if (split.length != 2) {
            throw new IllegalArgumentException("Can't split argument " + arg);
        }
        double d;
        try {
            d = Double.parseDouble(split[1]);
        } catch (Exception e) {
            throw new IllegalArgumentException(e);
        }
        if (d < 0) {
            throw new IllegalArgumentException(arg + ": value must be >= 0.");
        }
        Operation op = null;
        if (split[0].equals("-oom")) {
            op = new OOM();
        } else if (split[0].equals("-sigquit")) {
            op = new SigQuit();
        } else if (split[0].equals("-alloc")) {
            op = new Alloc();
        } else if (split[0].equals("-largealloc")) {
            op = new LargeAlloc();
        } else if (split[0].equals("-stacktrace")) {
            op = new StackTrace();
        } else if (split[0].equals("-exit")) {
            op = new Exit();
        } else if (split[0].equals("-sleep")) {
            op = new Sleep();
        } else if (split[0].equals("-wait")) {
            op = new Wait(lock);
        } else if (split[0].equals("-timedwait")) {
            op = new TimedWait(lock);
        } else if (split[0].equals("-syncandwork")) {
            op = new SyncAndWork(lock);
        } else if (split[0].equals("-queuedwait")) {
            op = new QueuedWait(semaphore);
        } else {
            throw new IllegalArgumentException("Unknown arg " + arg);
        }

        if (in == null) {
            in = new HashMap<Operation, Double>();
        }
        in.put(op, d);

        return in;
    }

    private static void normalize(Map<Operation, Double> map) {
        double sum = 0;
        for (Double d : map.values()) {
            sum += d;
        }
        if (sum == 0) {
            throw new RuntimeException("No elements!");
        }
        if (sum != 1.0) {
            // Avoid ConcurrentModificationException.
            Set<Operation> tmp = new HashSet<>(map.keySet());
            for (Operation op : tmp) {
                map.put(op, map.get(op) / sum);
            }
        }
    }

    public static void parseAndRun(String[] args) throws Exception {
        int numberOfThreads = -1;
        int numberOfDaemons = -1;
        int totalOperations = -1;
        int operationsPerThread = -1;
        int permits = -1;
        Object lock = new Object();
        Map<Operation, Double> frequencyMap = null;
        boolean dumpMap = false;

        if (args != null) {
            // args[0] is libarttest
            for (int i = 1; i < args.length; i++) {
                if (args[i].equals("-n")) {
                    i++;
                    numberOfThreads = Integer.parseInt(args[i]);
                } else if (args[i].equals("-d")) {
                    i++;
                    numberOfDaemons = Integer.parseInt(args[i]);
                } else if (args[i].equals("-o")) {
                    i++;
                    totalOperations = Integer.parseInt(args[i]);
                } else if (args[i].equals("-t")) {
                    i++;
                    operationsPerThread = Integer.parseInt(args[i]);
                } else if (args[i].equals("-p")) {
                    i++;
                    permits = Integer.parseInt(args[i]);
                } else if (args[i].equals("--locks-only")) {
                    frequencyMap = createLockFrequencyMap(lock);
                } else if (args[i].equals("--allocs-only")) {
                    frequencyMap = createAllocFrequencyMap();
                } else if (args[i].equals("--dumpmap")) {
                    dumpMap = true;
                } else {
                    // Processing an argument of the form "-<operation>:X"
                    // (where X is a double value).
                    Semaphore semaphore = getSemaphore(permits);
                    frequencyMap = updateFrequencyMap(frequencyMap, lock, semaphore, args[i]);
                }
            }
        }

        if (totalOperations != -1 && operationsPerThread != -1) {
            throw new IllegalArgumentException(
                    "Specified both totalOperations and operationsPerThread");
        }

        if (numberOfThreads == -1) {
            numberOfThreads = 5;
        }

        if (numberOfDaemons == -1) {
            numberOfDaemons = 3;
        }

        if (totalOperations == -1) {
            totalOperations = 1000;
        }

        if (operationsPerThread == -1) {
            operationsPerThread = totalOperations/numberOfThreads;
        }

        if (frequencyMap == null) {
            Semaphore semaphore = getSemaphore(permits);
            frequencyMap = createDefaultFrequencyMap(lock, semaphore);
        }
        normalize(frequencyMap);

        if (dumpMap) {
            System.out.println(frequencyMap);
        }

        try {
            runTest(numberOfThreads, numberOfDaemons, operationsPerThread, lock, frequencyMap);
        } catch (Throwable t) {
            // In this case, the output should not contain all the required
            // "Finishing worker" lines.
            Main.printThrowable(t);
        }
    }

    private static Semaphore getSemaphore(int permits) {
        if (permits == -1) {
            // Default number of permits.
            permits = 3;
        }

        Semaphore semaphore = new Semaphore(permits, /* fair */ true);
        forceTransitiveClassInitialization(semaphore, permits);
        return semaphore;
    }

    // Force ahead-of-time initialization of classes used by Semaphore
    // code. Try to exercise all code paths likely to be taken during
    // the actual test later (including having a thread blocking on
    // the semaphore trying to acquire a permit), so that we increase
    // the chances to initialize all classes indirectly used by
    // QueuedWait (e.g. AbstractQueuedSynchronizer$Node).
    private static void forceTransitiveClassInitialization(Semaphore semaphore, final int permits) {
        // Ensure `semaphore` has the expected number of permits
        // before we start.
        assert semaphore.availablePermits() == permits;

        // Let the main (current) thread acquire all permits from
        // `semaphore`. Then create an auxiliary thread acquiring a
        // permit from `semaphore`, blocking because none is
        // available. Have the main thread release one permit, thus
        // unblocking the second thread.

        // Auxiliary thread.
        Thread auxThread = new Thread("Aux") {
            public void run() {
                try {
                    // Try to acquire one permit, and block until
                    // that permit is released by the main thread.
                    semaphore.acquire();
                    // When unblocked, release the acquired permit
                    // immediately.
                    semaphore.release();
                } catch (InterruptedException ignored) {
                    throw new RuntimeException("Test set up failed in auxiliary thread");
                }
            }
        };

        // Main thread.
        try {
            // Acquire all permits.
            semaphore.acquire(permits);
            // Start the auxiliary thread and have it try to acquire a
            // permit.
            auxThread.start();
            // Synchronization: Wait until the auxiliary thread is
            // blocked trying to acquire a permit from `semaphore`.
            while (!semaphore.hasQueuedThreads()) {
                Thread.sleep(100);
            }
            // Release one permit, thus unblocking `auxThread` and let
            // it acquire a permit.
            semaphore.release();
            // Synchronization: Wait for the auxiliary thread to die.
            auxThread.join();
            // Release remaining permits.
            semaphore.release(permits - 1);

            // Verify that all permits have been released.
            assert semaphore.availablePermits() == permits;
        } catch (InterruptedException ignored) {
            throw new RuntimeException("Test set up failed in main thread");
        }
    }

    public static void runTest(final int numberOfThreads, final int numberOfDaemons,
                               final int operationsPerThread, final Object lock,
                               Map<Operation, Double> frequencyMap) throws Exception {
        final Thread mainThread = Thread.currentThread();
        final Barrier startBarrier = new Barrier(numberOfThreads + numberOfDaemons + 1);

        // Each normal thread is going to do operationsPerThread
        // operations. Each daemon thread will loop over all
        // the operations and will not stop.
        // The distribution of operations is determined by
        // the frequencyMap values. We fill out an Operation[]
        // for each thread with the operations it is to perform. The
        // Operation[] is shuffled so that there is more random
        // interactions between the threads.

        // Fill in the Operation[] array for each thread by laying
        // down references to operation according to their desired
        // frequency.
        // The first numberOfThreads elements are normal threads, the last
        // numberOfDaemons elements are daemon threads.
        final Main[] threadStresses = new Main[numberOfThreads + numberOfDaemons];
        for (int t = 0; t < threadStresses.length; t++) {
            Operation[] operations = new Operation[operationsPerThread];
            int o = 0;
            LOOP:
            while (true) {
                for (Operation op : frequencyMap.keySet()) {
                    int freq = (int)(frequencyMap.get(op) * operationsPerThread);
                    for (int f = 0; f < freq; f++) {
                        if (o == operations.length) {
                            break LOOP;
                        }
                        operations[o] = op;
                        o++;
                    }
                }
            }
            // Randomize the operation order
            Collections.shuffle(Arrays.asList(operations));
            threadStresses[t] = (t < numberOfThreads)
                    ? new Main(lock, t, operations)
                    : new Daemon(lock, t, operations, mainThread, startBarrier);
        }

        // Enable to dump operation counts per thread to make sure its
        // sane compared to frequencyMap.
        if (DEBUG) {
            for (int t = 0; t < threadStresses.length; t++) {
                Operation[] operations = threadStresses[t].operations;
                Map<Operation, Integer> distribution = new HashMap<Operation, Integer>();
                for (Operation operation : operations) {
                    Integer ops = distribution.get(operation);
                    if (ops == null) {
                        ops = 1;
                    } else {
                        ops++;
                    }
                    distribution.put(operation, ops);
                }
                System.out.println("Distribution for " + t);
                for (Operation op : frequencyMap.keySet()) {
                    System.out.println(op + " = " + distribution.get(op));
                }
            }
        }

        // Create the runners for each thread. The runner Thread
        // ensures that thread that exit due to operation Exit will be
        // restarted until they reach their desired
        // operationsPerThread.
        Thread[] runners = new Thread[numberOfThreads];
        for (int r = 0; r < runners.length; r++) {
            final Main ts = threadStresses[r];
            runners[r] = new Thread("Runner thread " + r) {
                final Main threadStress = ts;
                public void run() {
                    try {
                        int id = threadStress.id;
                        // No memory hungry task are running yet, so println() should succeed.
                        System.out.println("Starting worker for " + id);
                        // Wait until all runners and daemons reach the starting point.
                        startBarrier.await();
                        // Run the stress tasks.
                        while (threadStress.nextOperation < operationsPerThread) {
                            try {
                                Thread thread = new Thread(ts, "Worker thread " + id);
                                thread.start();
                                thread.join();

                                if (DEBUG) {
                                    System.out.println(
                                        "Thread exited for " + id + " with " +
                                        (operationsPerThread - threadStress.nextOperation) +
                                        " operations remaining.");
                                }
                            } catch (OutOfMemoryError e) {
                                // Ignore OOME since we need to print "Finishing worker"
                                // for the test to pass. This OOM can come from creating
                                // the Thread or from the DEBUG output.
                                // Note that the Thread creation may fail repeatedly,
                                // preventing the runner from making any progress,
                                // especially if the number of daemons is too high.
                            }
                        }
                        // Print "Finishing worker" through JNI to avoid OOME.
                        Main.printString(Main.finishingWorkerMessage);
                    } catch (Throwable t) {
                        Main.printThrowable(t);
                        // Interrupt the main thread, so that it can orderly shut down
                        // instead of waiting indefinitely for some Barrier.
                        mainThread.interrupt();
                    }
                }
            };
        }

        // The notifier thread is a daemon just loops forever to wake
        // up threads in operation Wait.
        if (lock != null) {
            Thread notifier = new Thread("Notifier") {
                public void run() {
                    while (true) {
                        synchronized (lock) {
                            lock.notifyAll();
                        }
                    }
                }
            };
            notifier.setDaemon(true);
            notifier.start();
        }

        // Create and start the daemon threads.
        for (int r = 0; r < numberOfDaemons; r++) {
            Main daemon = threadStresses[numberOfThreads + r];
            Thread t = new Thread(daemon, "Daemon thread " + daemon.id);
            t.setDaemon(true);
            t.start();
        }

        for (int r = 0; r < runners.length; r++) {
            runners[r].start();
        }
        // Wait for all threads to reach the starting point.
        startBarrier.await();
        // Wait for runners to finish.
        for (int r = 0; r < runners.length; r++) {
            runners[r].join();
        }
    }

    protected final Operation[] operations;
    private final Object lock;
    protected final int id;

    private int nextOperation;

    private Main(Object lock, int id, Operation[] operations) {
        this.lock = lock;
        this.id = id;
        this.operations = operations;
    }

    public void run() {
        try {
            if (DEBUG) {
                System.out.println("Starting ThreadStress " + id);
            }
            while (nextOperation < operations.length) {
                Operation operation = operations[nextOperation];
                if (DEBUG) {
                    System.out.println("ThreadStress " + id
                                       + " operation " + nextOperation
                                       + " is " + operation);
                }
                nextOperation++;
                if (!operation.perform()) {
                    return;
                }
            }
        } finally {
            if (DEBUG) {
                System.out.println("Finishing ThreadStress for " + id);
            }
        }
    }

    private static class Daemon extends Main {
        private Daemon(Object lock,
                       int id,
                       Operation[] operations,
                       Thread mainThread,
                       Barrier startBarrier) {
            super(lock, id, operations);
            this.mainThread = mainThread;
            this.startBarrier = startBarrier;
        }

        public void run() {
            try {
                if (DEBUG) {
                    System.out.println("Starting ThreadStress Daemon " + id);
                }
                startBarrier.await();
                try {
                    int i = 0;
                    while (true) {
                        Operation operation = operations[i];
                        if (DEBUG) {
                            System.out.println("ThreadStress Daemon " + id
                                               + " operation " + i
                                               + " is " + operation);
                        }
                        // Ignore the result of the performed operation, making
                        // Exit.perform() essentially a no-op for daemon threads.
                        operation.perform();
                        i = (i + 1) % operations.length;
                    }
                } catch (OutOfMemoryError e) {
                    // Catch OutOfMemoryErrors since these can cause the test to fail it they print
                    // the stack trace after "Finishing worker". Note that operations should catch
                    // their own OOME, this guards only agains OOME in the DEBUG output.
                }
                if (DEBUG) {
                    System.out.println("Finishing ThreadStress Daemon for " + id);
                }
            } catch (Throwable t) {
                Main.printThrowable(t);
                // Interrupt the main thread, so that it can orderly shut down
                // instead of waiting indefinitely for some Barrier.
                mainThread.interrupt();
            }
        }

        final Thread mainThread;
        final Barrier startBarrier;
    }

    // Note: java.util.concurrent.CyclicBarrier.await() allocates memory and may throw OOM.
    // That is highly undesirable in this test, so we use our own simple barrier class.
    // The only memory allocation that can happen here is the lock inflation which uses
    // a native allocation. As such, it should succeed even if the Java heap is full.
    // If the native allocation surprisingly fails, the program shall abort().
    private static class Barrier {
        public Barrier(int initialCount) {
            count = initialCount;
        }

        public synchronized void await() throws InterruptedException {
            --count;
            if (count != 0) {
                do {
                    wait();
                } while (count != 0);  // Check for spurious wakeup.
            } else {
                notifyAll();
            }
        }

        private int count;
    }

    // Printing a String/Throwable through JNI requires only native memory and space
    // in the local reference table, so it should succeed even if the Java heap is full.
    private static native void printString(String s);
    private static native void printThrowable(Throwable t);

    static final String finishingWorkerMessage;
    static final String errnoExceptionName;
    static {
        // We pre-allocate the strings in class initializer to avoid const-string
        // instructions in code using these strings later as they may throw OOME.
        finishingWorkerMessage = "Finishing worker\n";
        errnoExceptionName = "ErrnoException";
    }
}
