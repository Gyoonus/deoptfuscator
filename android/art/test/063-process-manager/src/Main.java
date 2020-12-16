import java.util.Map;

public class Main {
    static public void main(String[] args) throws Exception {
        checkManager();

        // Warm up the reaper so that there are no issues with scheduling because of static
        // initialization.
        {
            ProcessBuilder pb = new ProcessBuilder("sleep", "0");
            Process proc = pb.start();
            proc.waitFor();
            waitForReaperTimedWaiting(true /* reaperMustExist */);
        }

        for (int i = 1; i <= 2; i++) {
            System.out.println("\nspawning child #" + i);
            child();
            Thread.sleep(2000);
            checkManager();
        }
        System.out.println("\ndone!");
    }

    static private void child() throws Exception {
        System.out.println("spawning child");
        ProcessBuilder pb = new ProcessBuilder("sleep", "5");
        Process proc = pb.start();
        Thread.sleep(250);
        checkManager();
        proc.waitFor();
        System.out.println("child died");
    }

    private static boolean isReaperThread(Thread t) {
        String name = t.getName();
        return name.indexOf("process reaper") >= 0;
    }

    static private void checkManager() {
        Map<Thread, StackTraceElement[]> traces = Thread.getAllStackTraces();
        boolean found = false;

        for (Map.Entry<Thread, StackTraceElement[]> entry :
                 traces.entrySet()) {
            Thread t = entry.getKey();
            if (isReaperThread(t)) {
                Thread.State state = t.getState();
                System.out.println("process manager: " + state);
                if (state != Thread.State.RUNNABLE && state != Thread.State.TIMED_WAITING) {
                    for (StackTraceElement e : entry.getValue()) {
                        System.out.println("  " + e);
                    }
                }
                found = true;
            }
        }

        if (! found) {
            System.out.println("process manager: nonexistent");
        }
    }

    private static void waitForReaperTimedWaiting(boolean reaperMustExist) {
        for (;;) {
            Map<Thread, StackTraceElement[]> traces = Thread.getAllStackTraces();

            boolean ok = true;
            boolean found = false;

            for (Thread t : traces.keySet()) {
                if (isReaperThread(t)) {
                    found = true;
                    Thread.State state = t.getState();
                    if (state != Thread.State.TIMED_WAITING) {
                        ok = false;
                        break;
                    }
                }
            }

            if (ok && (!reaperMustExist || found)) {
                return;
            }

            try {
                Thread.sleep(100);
            } catch (Exception e) {
                // Ignore.
            }
        }
    }
}
