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

import dalvik.system.PathClassLoader;
import dalvik.system.VMDebug;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.File;
import java.io.IOException;

public class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    System.out.println("Hello, world!");
    String agent = null;
    // By default allow debugging
    boolean debugging_allowed = true;
    for(String a : args) {
      if(a.startsWith("agent:")) {
        agent = a.substring(6);
      } else if (a.equals("disallow-debugging")) {
        debugging_allowed = false;
      }
    }
    if (agent == null) {
      throw new Error("Could not find agent: argument!");
    }
    setDebuggingAllowed(debugging_allowed);
    // Setup is finished. Try to attach agent in 2 ways.
    try {
      VMDebug.attachAgent(agent);
    } catch(SecurityException e) {
      System.out.println(e.getMessage());
    }
    attachWithClassLoader(args);
    System.out.println("Goodbye!");
  }

  private static native void setDebuggingAllowed(boolean val);

  private static void attachWithClassLoader(String[] args) throws Exception {
    for(String a : args) {
      if(a.startsWith("agent:")) {
        String agentName = a.substring(6, a.indexOf('='));
        File tmp = null;
        try {
          tmp = File.createTempFile("lib", ".so");
          prepare(agentName, tmp);

          String newAgentName = tmp.getName();
          String agent = a.substring(6).replace(agentName, newAgentName);

          ClassLoader cl = new PathClassLoader("", tmp.getParentFile().getAbsolutePath(),
              Main.class.getClassLoader());
          try {
            VMDebug.attachAgent(agent, cl);
          } catch(SecurityException e) {
            System.out.println(e.getMessage());
          }
        } catch (Exception e) {
          e.printStackTrace(System.out);
        } finally {
          if (tmp != null) {
            tmp.delete();
          }
        }
      }
    }
  }

  private static void prepare(String in, File tmp) throws Exception {
    // Find the original.
    File orig = find(in);
    if (orig == null) {
      throw new RuntimeException("Could not find " + in);
    }
    // Copy the original.
    {
      BufferedInputStream bis = new BufferedInputStream(new FileInputStream(orig));
      BufferedOutputStream bos = new BufferedOutputStream(new FileOutputStream(tmp));
      byte[] buf = new byte[16 * 1024];
      for (;;) {
        int r = bis.read(buf, 0, buf.length);
        if (r < 0) {
          break;
        } else if (r > 0) {
          bos.write(buf, 0, r);
        }
      }
      bos.close();
      bis.close();
    }
  }

  private static File find(String in) {
    String libraryPath = System.getProperty("java.library.path");
    for (String path : libraryPath.split(":")) {
      File f = new File(path + "/" + in);
      if (f.exists()) {
        return f;
      }
    }
    return null;
  }
}
