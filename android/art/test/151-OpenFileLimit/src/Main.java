/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static java.nio.file.StandardOpenOption.*;
import java.nio.file.*;
import java.io.*;
import java.util.*;

public class Main {
    private static final String TEMP_FILE_NAME_PREFIX = "oflimit";
    private static final String TEMP_FILE_NAME_SUFFIX = ".txt";

    public static void main(String[] args) throws IOException {

        // Exhaust the number of open file descriptors.
        List<File> files = new ArrayList<File>();
        List<OutputStream> streams = new ArrayList<OutputStream>();
        try {
            for (int i = 0; ; i++) {
                File file = createTempFile();
                files.add(file);
                streams.add(Files.newOutputStream(file.toPath(), CREATE, APPEND));
            }
        } catch (Throwable e) {
            if (e.getMessage().contains("Too many open files")) {
                System.out.println("Message includes \"Too many open files\"");
            } else {
                System.out.println(e.getMessage());
            }
        }

        // Now try to create a new thread.
        try {
            Thread thread = new Thread() {
                public void run() {
                    System.out.println("thread run.");
                }
            };
            thread.start();
            thread.join();
        } catch (Throwable e) {
            System.out.println(e.getMessage());
        }

        for (int i = 0; i < streams.size(); i++) {
          streams.get(i).close();
        }

        for (int i = 0; i < files.size(); i++) {
          files.get(i).delete();
        }
        System.out.println("done.");
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
}
