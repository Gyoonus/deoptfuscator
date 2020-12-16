/*
 * Copyright (C) 2014 The Android Open Source Project
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

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.nio.ByteBuffer;
import java.util.ArrayList;

public class Main {
    public static void main(String[] args) throws Exception {
        String name = System.getProperty("java.vm.name");
        if (!"Dalvik".equals(name)) {
            System.out.println("This test is not supported on " + name);
            return;
        }
        testRecentAllocationTracking();
    }

    private static ArrayList<Object> staticHolder = new ArrayList<>(100000);

    private static void testRecentAllocationTracking() throws Exception {
        System.out.println("Confirm empty");
        Allocations empty = new Allocations(DdmVmInternal.getRecentAllocations());
        System.out.println("empty=" + empty);

        System.out.println("Confirm enable");
        System.out.println("status=" + DdmVmInternal.getRecentAllocationStatus());
        DdmVmInternal.enableRecentAllocations(true);
        System.out.println("status=" + DdmVmInternal.getRecentAllocationStatus());

        System.out.println("Capture some allocations (note just this causes allocations)");
        Allocations before = new Allocations(DdmVmInternal.getRecentAllocations());
        System.out.println("before > 0=" + (before.numberOfEntries > 0));

        System.out.println("Confirm when we overflow, we don't roll over to zero. b/17392248");
        final int overflowAllocations = 64 * 1024;  // Won't fit in unsigned 16-bit value.
        for (int i = 0; i < overflowAllocations; i++) {
            allocate(i, 0);
        }
        Allocations after = new Allocations(DdmVmInternal.getRecentAllocations());
        System.out.println("before < overflowAllocations=" + (before.numberOfEntries < overflowAllocations));
        System.out.println("after > before=" + (after.numberOfEntries > before.numberOfEntries));
        System.out.println("after.numberOfEntries=" + after.numberOfEntries);

        staticHolder.clear();  // Free the allocated objects.

        System.out.println("Disable and confirm back to empty");
        DdmVmInternal.enableRecentAllocations(false);
        System.out.println("status=" + DdmVmInternal.getRecentAllocationStatus());
        Allocations reset = new Allocations(DdmVmInternal.getRecentAllocations());
        System.out.println("reset=" + reset);

        System.out.println("Confirm we can disable twice in a row");
        DdmVmInternal.enableRecentAllocations(false);
        System.out.println("status=" + DdmVmInternal.getRecentAllocationStatus());
        DdmVmInternal.enableRecentAllocations(false);
        System.out.println("status=" + DdmVmInternal.getRecentAllocationStatus());

        System.out.println("Confirm we can reenable twice in a row without losing allocations");
        DdmVmInternal.enableRecentAllocations(true);
        System.out.println("status=" + DdmVmInternal.getRecentAllocationStatus());
        for (int i = 0; i < 16 * 1024; i++) {
            staticHolder.add(new String("fnord"));
        }
        Allocations first = new Allocations(DdmVmInternal.getRecentAllocations());
        DdmVmInternal.enableRecentAllocations(true);
        System.out.println("status=" + DdmVmInternal.getRecentAllocationStatus());
        Allocations second = new Allocations(DdmVmInternal.getRecentAllocations());
        System.out.println("second > first =" + (second.numberOfEntries > first.numberOfEntries));

        System.out.println("Goodbye");
        DdmVmInternal.enableRecentAllocations(false);
        Allocations goodbye = new Allocations(DdmVmInternal.getRecentAllocations());
        System.out.println("goodbye=" + goodbye);
    }

    // Allocate a simple object. Use depth for a reasonably deep stack.
    private static final int ALLOCATE1_DEPTH = 50;

    private static Object createProxy() {
        try {
            InvocationHandler handler = new InvocationHandler() {
                public Object invoke(Object proxy, Method method, Object[] args) {
                    // Don't expect to be invoked.
                    return null;
                }
            };
            return Proxy.newProxyInstance(Main.class.getClassLoader(),
                    new Class[] { Runnable.class }, handler);
        } catch (Exception e) {
            // We don't really expect exceptions here.
            throw new RuntimeException(e);
        }
    }

    private static void allocate(int i, int depth) {
        if (depth >= ALLOCATE1_DEPTH) {
            // Mix proxies, int arrays and Objects to test the different descriptor paths.
            switch (i) {
                case 0:
                    staticHolder.add(createProxy());
                    break;

                case 1:
                    staticHolder.add(new int[0]);
                    break;

                case 2:
                    staticHolder.add(new Object[0]);
                    break;

                default:
                    staticHolder.add(new Object());
                    break;
            }
        } else {
            allocate(i, depth + 1);
        }
    }

    private static class Allocations {
        final int messageHeaderLen;
        final int entryHeaderLen;
        final int stackFrameLen;
        final int numberOfEntries;
        final int offsetToStringTableFromStartOfMessage;
        final int numberOfClassNameStrings;
        final int numberOfMethodNameStrings;
        final int numberOfSourceFileNameStrings;

        Allocations(byte[] allocations) {
            ByteBuffer b = ByteBuffer.wrap(allocations);
            messageHeaderLen = b.get() & 0xff;
            if (messageHeaderLen != 15) {
                throw new IllegalArgumentException("Unexpected messageHeaderLen " + messageHeaderLen);
            }
            entryHeaderLen = b.get() & 0xff;
            if (entryHeaderLen != 9) {
                throw new IllegalArgumentException("Unexpected entryHeaderLen " + entryHeaderLen);
            }
            stackFrameLen = b.get() & 0xff;
            if (stackFrameLen != 8) {
                throw new IllegalArgumentException("Unexpected messageHeaderLen " + stackFrameLen);
            }
            numberOfEntries = b.getShort() & 0xffff;
            offsetToStringTableFromStartOfMessage = b.getInt();
            numberOfClassNameStrings = b.getShort() & 0xffff;
            numberOfMethodNameStrings = b.getShort() & 0xffff;
            numberOfSourceFileNameStrings = b.getShort() & 0xffff;
        }

        public String toString() {
            return ("Allocations[message header len: " + messageHeaderLen +
                    " entry header len: " + entryHeaderLen +
                    " stack frame len: " + stackFrameLen +
                    " number of entries: " + numberOfEntries +
                    " offset to string table from start of message: " + offsetToStringTableFromStartOfMessage +
                    " number of class name strings: " + numberOfClassNameStrings +
                    " number of method name strings: " + numberOfMethodNameStrings +
                    " number of source file name strings: " + numberOfSourceFileNameStrings +
                    "]");
        }
    }

    private static class DdmVmInternal {
        private static final Method enableRecentAllocationsMethod;
        private static final Method getRecentAllocationStatusMethod;
        private static final Method getRecentAllocationsMethod;
        static {
            try {
                Class<?> c = Class.forName("org.apache.harmony.dalvik.ddmc.DdmVmInternal");
                enableRecentAllocationsMethod = c.getDeclaredMethod("enableRecentAllocations",
                                                                    Boolean.TYPE);
                getRecentAllocationStatusMethod = c.getDeclaredMethod("getRecentAllocationStatus");
                getRecentAllocationsMethod = c.getDeclaredMethod("getRecentAllocations");
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void enableRecentAllocations(boolean enable) throws Exception {
            enableRecentAllocationsMethod.invoke(null, enable);
        }
        public static boolean getRecentAllocationStatus() throws Exception {
            return (boolean) getRecentAllocationStatusMethod.invoke(null);
        }
        public static byte[] getRecentAllocations() throws Exception {
            return (byte[]) getRecentAllocationsMethod.invoke(null);
        }
    }
}
