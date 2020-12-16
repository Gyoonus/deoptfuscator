/*
 * Copyright (C) 2015 The Android Open Source Project
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

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.concurrent.ConcurrentSkipListMap;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

public class Main {
    private static final String TEMP_FILE_NAME_PREFIX = "test";
    private static final String TEMP_FILE_NAME_SUFFIX = ".trace";
    private static File file;

    public static void main(String[] args) throws Exception {
        String name = System.getProperty("java.vm.name");
        if (!"Dalvik".equals(name)) {
            System.out.println("This test is not supported on " + name);
            return;
        }
        file = createTempFile();
        try {
            new Main().ensureCaller(true, 0);
            new Main().ensureCaller(false, 0);
        } finally {
            if (file != null) {
              file.delete();
            }
        }
    }

    private static File createTempFile() throws Exception {
        try {
            return  File.createTempFile(TEMP_FILE_NAME_PREFIX, TEMP_FILE_NAME_SUFFIX);
        } catch (IOException e) {
            System.setProperty("java.io.tmpdir", "/data/local/tmp");
            try {
                return File.createTempFile(TEMP_FILE_NAME_PREFIX, TEMP_FILE_NAME_SUFFIX);
            } catch (IOException e2) {
                System.setProperty("java.io.tmpdir", "/sdcard");
                return File.createTempFile(TEMP_FILE_NAME_PREFIX, TEMP_FILE_NAME_SUFFIX);
            }
        }
    }

    // We make sure 'doLoadsOfStuff' has a caller, because it is this caller that will be
    // pushed in the side instrumentation frame.
    public void ensureCaller(boolean warmup, int invocationCount) throws Exception {
        doLoadsOfStuff(warmup, invocationCount);
    }

    // The number of recursive calls we are going to do in 'doLoadsOfStuff' to ensure
    // the JIT sees it hot.
    static final int NUMBER_OF_INVOCATIONS = 5;

    public void doLoadsOfStuff(boolean warmup, int invocationCount) throws Exception {
        // Warmup is to make sure the JIT gets a chance to compile 'doLoadsOfStuff'.
        if (warmup) {
            if (invocationCount < NUMBER_OF_INVOCATIONS) {
                doLoadsOfStuff(warmup, ++invocationCount);
            } else {
                // Give the JIT a chance to compiler.
                Thread.sleep(1000);
            }
        } else {
            if (invocationCount == 0) {
                // When running the trace in trace mode, there is already a trace running.
                if (VMDebug.getMethodTracingMode() != 0) {
                    VMDebug.stopMethodTracing();
                }
                VMDebug.startMethodTracing(file.getPath(), 0, 0, false, 0);
            }
            fillJit();
            if (invocationCount < NUMBER_OF_INVOCATIONS) {
                doLoadsOfStuff(warmup, ++invocationCount);
            } else {
                VMDebug.stopMethodTracing();
            }
        }
    }

    // This method creates enough profiling data to fill the code cache and trigger
    // a collection in debug mode (at the time of the test 10KB of data space). We
    // used to crash by not looking at the instrumentation stack and deleting JIT code
    // that will be later restored by the instrumentation.
    public static void fillJit() throws Exception {
        Map map = new HashMap();
        map.put("foo", "bar");
        map.clear();
        map.containsKey("foo");
        map.containsValue("foo");
        map.entrySet();
        map.equals(map);
        map.hashCode();
        map.isEmpty();
        map.keySet();
        map.putAll(map);
        map.remove("foo");
        map.size();
        map.put("bar", "foo");
        map.values();

        map = new LinkedHashMap();
        map.put("foo", "bar");
        map.clear();
        map.containsKey("foo");
        map.containsValue("foo");
        map.entrySet();
        map.equals(map);
        map.hashCode();
        map.isEmpty();
        map.keySet();
        map.putAll(map);
        map.remove("foo");
        map.size();
        map.put("bar", "foo");
        map.values();

        map = new TreeMap();
        map.put("foo", "bar");
        map.clear();
        map.containsKey("foo");
        map.containsValue("foo");
        map.entrySet();
        map.equals(map);
        map.hashCode();
        map.isEmpty();
        map.keySet();
        map.putAll(map);
        map.remove("foo");
        map.size();
        map.put("bar", "foo");
        map.values();

        map = new ConcurrentSkipListMap();
        map.put("foo", "bar");
        map.clear();
        map.containsKey("foo");
        map.containsValue("foo");
        map.entrySet();
        map.equals(map);
        map.hashCode();
        map.isEmpty();
        map.keySet();
        map.putAll(map);
        map.remove("foo");
        map.size();
        map.put("bar", "foo");
        map.values();

        Set set = new HashSet();
        set.add("foo");
        set.addAll(set);
        set.clear();
        set.contains("foo");
        set.containsAll(set);
        set.equals(set);
        set.hashCode();
        set.isEmpty();
        set.iterator();
        set.remove("foo");
        set.removeAll(set);
        set.retainAll(set);
        set.size();
        set.add("foo");
        set.toArray();

        set = new LinkedHashSet();
        set.add("foo");
        set.addAll(set);
        set.clear();
        set.contains("foo");
        set.containsAll(set);
        set.equals(set);
        set.hashCode();
        set.isEmpty();
        set.iterator();
        set.remove("foo");
        set.removeAll(set);
        set.retainAll(set);
        set.size();
        set.add("foo");
        set.toArray();

        set = new TreeSet();
        set.add("foo");
        set.addAll(set);
        set.clear();
        set.contains("foo");
        set.containsAll(set);
        set.equals(set);
        set.hashCode();
        set.isEmpty();
        set.iterator();
        set.remove("foo");
        set.removeAll(set);
        set.retainAll(set);
        set.size();
        set.add("foo");
        set.toArray();
    }

    private static class VMDebug {
        private static final Method startMethodTracingMethod;
        private static final Method stopMethodTracingMethod;
        private static final Method getMethodTracingModeMethod;
        static {
            try {
                Class<?> c = Class.forName("dalvik.system.VMDebug");
                startMethodTracingMethod = c.getDeclaredMethod("startMethodTracing", String.class,
                        Integer.TYPE, Integer.TYPE, Boolean.TYPE, Integer.TYPE);
                stopMethodTracingMethod = c.getDeclaredMethod("stopMethodTracing");
                getMethodTracingModeMethod = c.getDeclaredMethod("getMethodTracingMode");
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void startMethodTracing(String filename, int bufferSize, int flags,
                boolean samplingEnabled, int intervalUs) throws Exception {
            startMethodTracingMethod.invoke(null, filename, bufferSize, flags, samplingEnabled,
                    intervalUs);
        }
        public static void stopMethodTracing() throws Exception {
            stopMethodTracingMethod.invoke(null);
        }
        public static int getMethodTracingMode() throws Exception {
            return (int) getMethodTracingModeMethod.invoke(null);
        }
    }
}
