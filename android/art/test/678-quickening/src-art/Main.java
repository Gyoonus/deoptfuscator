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

import java.lang.reflect.Field;
import java.lang.reflect.Method;

class NotLoaded {
  public void foo() {}
}

public class Main {
  public static void main(String[] args) throws Exception {
    runTest(null);
  }

  // This method being synchronized means the SIGQUIT code in ART will call
  // FindLocksAtDexPc (we check for the presence of try blocks and monitor-enter),
  // which triggered a DCHECK of an invariant.
  public static synchronized void runTest(Object m) throws Exception {
    if (m != null) {
      // We used to crash while trying to resolve NotLoaded and beint interrupted
      // by the SIGQUIT.
      if (m instanceof NotLoaded) {
        ((NotLoaded)m).foo();
      }
    }
    SigQuit.doKill();
    // Sleep some time to get the kill while executing this method.
    Thread.sleep(2);
    System.out.println("Done");
  }

  private final static class SigQuit {
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
        throw new Error(e);
      }

      pid = pidTemp;
      sigquit = sigquitTemp;
      kill = killTemp;
    }

    public static void doKill() throws Exception {
      kill.invoke(null, pid, sigquit);
    }
  }
}
