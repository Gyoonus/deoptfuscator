/*
 * Copyright (C) 2017 The Android Open Source Project
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

package art;

import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.util.Base64;

public class Test996 {
  // The line we are going to break on. This should be the println in the Transform class. We set a
  // breakpoint here after we have redefined the class.
  public static final int TRANSFORM_BREAKPOINT_REDEFINED_LINE = 40;

  // The line we initially set a breakpoint on. This should be the doNothing call. This should be
  // cleared by the redefinition and should only be caught on the initial run.
  public static final int TRANSFORM_BREAKPOINT_INITIAL_LINE = 42;

  // A function that doesn't do anything. Used for giving places to break on in a function.
  public static void doNothing() {}

  public static final class Transform {
    public void run(Runnable r) {
      r.run();
      // Make sure we don't change anything above this line to keep all the breakpoint stuff
      // working. We will be putting a breakpoint before this line in the runnable.
      System.out.println("Should be after first breakpoint.");
      // This is set as a breakpoint prior to redefinition. It should not be hit.
      doNothing();
    }
  }

  /* ******************************************************************************************** */
  // Try to keep all edits to this file below the above line. If edits need to be made above this
  // line be sure to update the TRANSFORM_BREAKPOINT_REDEFINED_LINE and
  // TRANSFORM_BREAKPOINT_INITIAL_LINE to their appropriate values.

  public static final int TRANSFORM_BREAKPOINT_POST_REDEFINITION_LINE = 8;

  // The base64 encoding of the following class. The redefined 'run' method should have the same
  // instructions as the original. This means that the locations of each line should stay the same
  // and the set of valid locations will not change. We use this to ensure that breakpoints are
  // removed from the redefined method.
  // public static final class Transform {
  //   public void run(Runnable r) {
  //     r.run();
  //     System.out.println("Doing nothing transformed");
  //     doNothing();  // try to catch non-removed breakpoints
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAKAoACAARCwASABMJABQAFQgAFgoAFwAYCgAZABoHABsHAB4BAAY8aW5pdD4BAAMo" +
    "KVYBAARDb2RlAQAPTGluZU51bWJlclRhYmxlAQADcnVuAQAXKExqYXZhL2xhbmcvUnVubmFibGU7" +
    "KVYBAApTb3VyY2VGaWxlAQAMVGVzdDk5Ni5qYXZhDAAJAAoHAB8MAA0ACgcAIAwAIQAiAQAZRG9p" +
    "bmcgbm90aGluZyB0cmFuc2Zvcm1lZAcAIwwAJAAlBwAmDAAnAAoBABVhcnQvVGVzdDk5NiRUcmFu" +
    "c2Zvcm0BAAlUcmFuc2Zvcm0BAAxJbm5lckNsYXNzZXMBABBqYXZhL2xhbmcvT2JqZWN0AQASamF2" +
    "YS9sYW5nL1J1bm5hYmxlAQAQamF2YS9sYW5nL1N5c3RlbQEAA291dAEAFUxqYXZhL2lvL1ByaW50" +
    "U3RyZWFtOwEAE2phdmEvaW8vUHJpbnRTdHJlYW0BAAdwcmludGxuAQAVKExqYXZhL2xhbmcvU3Ry" +
    "aW5nOylWAQALYXJ0L1Rlc3Q5OTYBAAlkb05vdGhpbmcAMQAHAAgAAAAAAAIAAQAJAAoAAQALAAAA" +
    "HQABAAEAAAAFKrcAAbEAAAABAAwAAAAGAAEAAAAEAAEADQAOAAEACwAAADYAAgACAAAAEiu5AAIB" +
    "ALIAAxIEtgAFuAAGsQAAAAEADAAAABIABAAAAAYABgAHAA4ACAARAAkAAgAPAAAAAgAQAB0AAAAK" +
    "AAEABwAZABwAGQ==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQBzn3TiKGAiM0fubj25v816W0k+niqj+SQcBAAAcAAAAHhWNBIAAAAAAAAAAFgDAAAW" +
    "AAAAcAAAAAoAAADIAAAAAwAAAPAAAAABAAAAFAEAAAYAAAAcAQAAAQAAAEwBAACwAgAAbAEAANoB" +
    "AADiAQAA/QEAABYCAAAlAgAASQIAAGkCAACAAgAAlAIAAKoCAAC+AgAA0gIAAOACAADrAgAA7gIA" +
    "APICAAD/AgAACgMAABADAAAVAwAAHgMAACMDAAACAAAAAwAAAAQAAAAFAAAABgAAAAcAAAAIAAAA" +
    "CQAAAAoAAAANAAAADQAAAAkAAAAAAAAADgAAAAkAAADMAQAADgAAAAkAAADUAQAACAAEABIAAAAA" +
    "AAAAAAAAAAAAAQAUAAAAAQAAABAAAAAEAAIAEwAAAAUAAAAAAAAABgAAABQAAAAAAAAAEQAAAAUA" +
    "AAAAAAAACwAAALwBAABHAwAAAAAAAAIAAAA4AwAAPgMAAAEAAQABAAAAKgMAAAQAAABwEAQAAAAO" +
    "AAQAAgACAAAALwMAAA4AAAByEAUAAwBiAAAAGgEBAG4gAwAQAHEAAgAAAA4AbAEAAAAAAAAAAAAA" +
    "AAAAAAEAAAAGAAAAAQAAAAcABjxpbml0PgAZRG9pbmcgbm90aGluZyB0cmFuc2Zvcm1lZAAXTGFy" +
    "dC9UZXN0OTk2JFRyYW5zZm9ybTsADUxhcnQvVGVzdDk5NjsAIkxkYWx2aWsvYW5ub3RhdGlvbi9F" +
    "bmNsb3NpbmdDbGFzczsAHkxkYWx2aWsvYW5ub3RhdGlvbi9Jbm5lckNsYXNzOwAVTGphdmEvaW8v" +
    "UHJpbnRTdHJlYW07ABJMamF2YS9sYW5nL09iamVjdDsAFExqYXZhL2xhbmcvUnVubmFibGU7ABJM" +
    "amF2YS9sYW5nL1N0cmluZzsAEkxqYXZhL2xhbmcvU3lzdGVtOwAMVGVzdDk5Ni5qYXZhAAlUcmFu" +
    "c2Zvcm0AAVYAAlZMAAthY2Nlc3NGbGFncwAJZG9Ob3RoaW5nAARuYW1lAANvdXQAB3ByaW50bG4A" +
    "A3J1bgAFdmFsdWUABAAHDgAGAQAHDjx4PAACAgEVGAECAwIPBBkRFwwAAAEBAIGABPgCAQGQAwAA" +
    "ABAAAAAAAAAAAQAAAAAAAAABAAAAFgAAAHAAAAACAAAACgAAAMgAAAADAAAAAwAAAPAAAAAEAAAA" +
    "AQAAABQBAAAFAAAABgAAABwBAAAGAAAAAQAAAEwBAAADEAAAAQAAAGwBAAABIAAAAgAAAHgBAAAG" +
    "IAAAAQAAALwBAAABEAAAAgAAAMwBAAACIAAAFgAAANoBAAADIAAAAgAAACoDAAAEIAAAAgAAADgD" +
    "AAAAIAAAAQAAAEcDAAAAEAAAAQAAAFgDAAA=");

  public static void notifyBreakpointReached(Thread thr, Executable e, long loc) {
    int line = Breakpoint.locationToLine(e, loc);
    if (line == -1 && e.getName().equals("run") && e.getDeclaringClass().equals(Transform.class)) {
      // RI always reports line = -1 for obsolete methods. Just replace it with the real line for
      // consistency.
      line = TRANSFORM_BREAKPOINT_REDEFINED_LINE;
    }
    System.out.println("Breakpoint reached: " + e + " @ line=" + line);
  }

  public static void run() throws Exception {
    // Set up breakpoints
    Breakpoint.stopBreakpointWatch(Thread.currentThread());
    Breakpoint.startBreakpointWatch(
        Test996.class,
        Test996.class.getDeclaredMethod(
            "notifyBreakpointReached", Thread.class, Executable.class, Long.TYPE),
        Thread.currentThread());

    Transform t = new Transform();
    Method non_obsolete_run_method = Transform.class.getDeclaredMethod("run", Runnable.class);
    final long obsolete_breakpoint_location =
        Breakpoint.lineToLocation(non_obsolete_run_method, TRANSFORM_BREAKPOINT_REDEFINED_LINE);

    System.out.println("Initially setting breakpoint to line " + TRANSFORM_BREAKPOINT_INITIAL_LINE);
    long initial_breakpoint_location =
        Breakpoint.lineToLocation(non_obsolete_run_method, TRANSFORM_BREAKPOINT_INITIAL_LINE);
    Breakpoint.setBreakpoint(non_obsolete_run_method, initial_breakpoint_location);

    System.out.println("Running transform without redefinition.");
    t.run(() -> {});

    System.out.println("Running transform with redefinition.");
    t.run(() -> {
      System.out.println("Redefining calling function!");
      // This should clear the breakpoint set to TRANSFORM_BREAKPOINT_INITIAL_LINE
      Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
      System.out.println("Setting breakpoint on now obsolete method to line " +
          TRANSFORM_BREAKPOINT_REDEFINED_LINE);
      setBreakpointOnObsoleteMethod(obsolete_breakpoint_location);
    });
    System.out.println("Running transform post redefinition. Should not hit any breakpoints.");
    t.run(() -> {});

    System.out.println("Setting initial breakpoint on redefined method.");
    long final_breakpoint_location =
        Breakpoint.lineToLocation(non_obsolete_run_method,
                                  TRANSFORM_BREAKPOINT_POST_REDEFINITION_LINE);
    Breakpoint.setBreakpoint(non_obsolete_run_method, final_breakpoint_location);
    t.run(() -> {});

    Breakpoint.stopBreakpointWatch(Thread.currentThread());
  }

  public static native void setBreakpointOnObsoleteMethod(long location);
}
