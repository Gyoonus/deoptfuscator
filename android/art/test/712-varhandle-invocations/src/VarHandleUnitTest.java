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

// Base class for VarHandle unit tests for accessor operations
public abstract class VarHandleUnitTest {
    public static VarHandleUnitTestCollector DEFAULT_COLLECTOR = new VarHandleUnitTestCollector();

    // Error log (lazily initialized on failure).
    private StringBuilder lazyErrorLog = null;

    // Tracker of test events (starts, skips, ends)
    private final VarHandleUnitTestCollector collector;

    public VarHandleUnitTest(VarHandleUnitTestCollector collector) {
        this.collector = collector;
    }

    public VarHandleUnitTest() {
        this.collector = DEFAULT_COLLECTOR;
    }

    // Method that can be overloaded to signify that a test should be
    // run or skipped. Returns true if the test should be run and
    // false if the test should be skipped.
    public boolean checkGuard() {
        return true;
    }

    // Method that implementations should use to perform a specific test.
    protected abstract void doTest() throws Exception;

    public final void assertTrue(boolean value) {
        assertEquals(true, value);
    }

    public final void assertFalse(boolean value) {
        assertEquals(false, value);
    }

    public final void assertEquals(boolean expected, boolean actual) {
        assertEquals(Boolean.valueOf(expected), Boolean.valueOf(actual));
    }

    public final void assertEquals(byte expected, byte actual) {
        assertEquals(Byte.valueOf(expected), Byte.valueOf(actual));
    }

    public final void assertEquals(char expected, char actual) {
        assertEquals(Character.valueOf(expected), Character.valueOf(actual));
    }

    public final void assertEquals(short expected, short actual) {
        assertEquals(Short.valueOf(expected), Short.valueOf(actual));
    }

    public final void assertEquals(int expected, int actual) {
        assertEquals(Integer.valueOf(expected), Integer.valueOf(actual));
    }

    public final void assertEquals(long expected, long actual) {
        assertEquals(Long.valueOf(expected), Long.valueOf(actual));
    }

    public final void assertEquals(float expected, float actual) {
        assertEquals(Float.valueOf(expected), Float.valueOf(actual));
    }

    public final void assertEquals(double expected, double actual) {
        assertEquals(Double.valueOf(expected), Double.valueOf(actual));
    }

    public final void assertEquals(Object expected, Object actual) {
        if (expected == null) {
            if (actual == null) {
                return;
            }
        } else if (expected.equals(actual)) {
            return;
        }
        failNotEquals("Failed assertion (expected != actual)", expected, actual);
    }

    public final void failUnreachable() {
        fail("Unreachable code");
    }

    public final void run() {
        collector.start(getClass().getSimpleName());
        if (!checkGuard()) {
            collector.skip();
            return;
        }

        try {
            doTest();
        } catch (Exception e) {
            fail("Unexpected exception", e);
        } finally {
            if (lazyErrorLog == null) {
                collector.success();
            } else {
                collector.fail(lazyErrorLog.toString());
            }
        }
    }

    private void failNotEquals(String message, Object expected, Object actual) {
        errorLog()
                .append(message)
                .append(": ")
                .append(expected)
                .append(" != ")
                .append(actual)
                .append(" in ")
                .append(getSourceInfo())
                .append('\n');
    }

    private void fail(String message) {
        errorLog().append(message).append(" in ").append(getSourceInfo()).append('\n');
    }

    private void fail(String message, String detail) {
        errorLog()
                .append(message)
                .append(": ")
                .append(detail)
                .append(" in ")
                .append(getSourceInfo())
                .append('\n');
    }

    private void fail(String message, Exception e) {
        errorLog()
                .append(message)
                .append(": ")
                .append(e.toString())
                .append(" in ")
                .append(getSourceInfo(e))
                .append('\n');
    }

    private String getSourceInfo(Exception e) {
        // Unit test has thrown an exception. Stack likely looks like
        // runtime frames then unit test frames then
        // VarHandleUnitFrames.
        StackTraceElement[] stackTraceElements = e.getStackTrace();
        int index = 1;
        for (int i = 1; i < stackTraceElements.length; ++i) {
            if ("VarHandleUnitTest".equals(stackTraceElements[i].getClassName())) {
                return stackTraceElements[i - 1].toString();
            }
        }
        return "Unknown";
    }

    private String getSourceInfo() {
        // Gets source info for a failure such as an assertion. The
        // test has called a method on VarHandleUnitTest so the stack
        // looks like some frames in VarHandleUnitTest methods and then
        // a frame in the test itself.
        StackTraceElement[] stackTraceElements = new Exception().getStackTrace();
        for (StackTraceElement stackTraceElement : stackTraceElements) {
            if (!"VarHandleUnitTest".equals(stackTraceElement.getClassName())) {
                return stackTraceElement.toString();
            }
        }
        return "Unknown";
    }

    private StringBuilder errorLog() {
        if (lazyErrorLog == null) {
            lazyErrorLog = new StringBuilder();
        }
        return lazyErrorLog;
    }
}
