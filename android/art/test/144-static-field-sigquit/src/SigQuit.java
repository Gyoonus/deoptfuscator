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

import java.lang.reflect.*;

public class SigQuit implements Runnable {
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
            pidTemp = (Integer) getpid.invoke(null);

            Class<?> osConstants = Class.forName("android.system.OsConstants");
            Field sigquitField = osConstants.getDeclaredField("SIGQUIT");
            sigquitTemp = (Integer) sigquitField.get(null);

            killTemp = osClass.getDeclaredMethod("kill", int.class, int.class);
        } catch (Exception e) {
            if (!e.getClass().getName().equals("ErrnoException")) {
                e.printStackTrace(System.out);
            }
        }

        pid = pidTemp;
        sigquit = sigquitTemp;
        kill = killTemp;
    }

    public boolean perform() {
        try {
            kill.invoke(null, pid, sigquit);
        } catch (Exception e) {
            if (!e.getClass().getName().equals("ErrnoException")) {
                e.printStackTrace(System.out);
            }
        }
        return true;
    }

    public void run() {
        long endTime = System.currentTimeMillis() + 5000;
        System.out.println("Performing sigquits for 5 seconds");
        while (System.currentTimeMillis() < endTime) {
            perform();
        }
    }
}
