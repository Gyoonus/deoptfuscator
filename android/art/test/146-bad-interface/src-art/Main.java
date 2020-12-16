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

import java.lang.reflect.Method;
import dalvik.system.PathClassLoader;

/**
 * Structural hazard test.
 */
public class Main {
  static final String DEX_LOCATION = System.getenv("DEX_LOCATION");
  static final String DEX_FILES =
      DEX_LOCATION + "/146-bad-interface-ex.jar" + ":" +
      DEX_LOCATION + "/146-bad-interface.jar";
  public static void main(String[] args) {
    try {
      PathClassLoader p = new PathClassLoader(DEX_FILES, Main.class.getClassLoader());
      Class<?> c = Class.forName("A", true, p);
      Object o = c.newInstance();
      Class<?> runner = Class.forName("InvokeInf", true, p);
      Class<?> arg = Class.forName("Iface", true, p);
      Method r = runner.getDeclaredMethod("doInvoke", arg);
      r.invoke(null, o);
    } catch (Throwable t) {
      System.out.println("Error occurred");
      System.out.println(t);
      t.printStackTrace(System.out);
    }
  }
}
