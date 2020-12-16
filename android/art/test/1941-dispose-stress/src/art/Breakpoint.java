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
import java.util.HashSet;
import java.util.Set;
import java.util.Objects;

public class Breakpoint {
  public static class Manager {
    public static class BP {
      public final Executable method;
      public final long location;

      public BP(Executable method) {
        this(method, getStartLocation(method));
      }

      public BP(Executable method, long location) {
        this.method = method;
        this.location = location;
      }

      @Override
      public boolean equals(Object other) {
        return (other instanceof BP) &&
            method.equals(((BP)other).method) &&
            location == ((BP)other).location;
      }

      @Override
      public String toString() {
        return method.toString() + " @ " + getLine();
      }

      @Override
      public int hashCode() {
        return Objects.hash(method, location);
      }

      public int getLine() {
        try {
          LineNumber[] lines = getLineNumberTable(method);
          int best = -1;
          for (LineNumber l : lines) {
            if (l.location > location) {
              break;
            } else {
              best = l.line;
            }
          }
          return best;
        } catch (Exception e) {
          return -1;
        }
      }
    }

    private Set<BP> breaks = new HashSet<>();

    public void setBreakpoints(BP... bs) {
      for (BP b : bs) {
        if (breaks.add(b)) {
          Breakpoint.setBreakpoint(b.method, b.location);
        }
      }
    }
    public void setBreakpoint(Executable method, long location) {
      setBreakpoints(new BP(method, location));
    }

    public void clearBreakpoints(BP... bs) {
      for (BP b : bs) {
        if (breaks.remove(b)) {
          Breakpoint.clearBreakpoint(b.method, b.location);
        }
      }
    }
    public void clearBreakpoint(Executable method, long location) {
      clearBreakpoints(new BP(method, location));
    }

    public void clearAllBreakpoints() {
      clearBreakpoints(breaks.toArray(new BP[0]));
    }
  }

  public static void startBreakpointWatch(Class<?> methodClass,
                                          Executable breakpointReached,
                                          Thread thr) {
    startBreakpointWatch(methodClass, breakpointReached, false, thr);
  }

  /**
   * Enables the trapping of breakpoint events.
   *
   * If allowRecursive == true then breakpoints will be sent even if one is currently being handled.
   */
  public static native void startBreakpointWatch(Class<?> methodClass,
                                                 Executable breakpointReached,
                                                 boolean allowRecursive,
                                                 Thread thr);
  public static native void stopBreakpointWatch(Thread thr);

  public static final class LineNumber implements Comparable<LineNumber> {
    public final long location;
    public final int line;

    private LineNumber(long loc, int line) {
      this.location = loc;
      this.line = line;
    }

    public boolean equals(Object other) {
      return other instanceof LineNumber && ((LineNumber)other).line == line &&
          ((LineNumber)other).location == location;
    }

    public int compareTo(LineNumber other) {
      int v = Integer.valueOf(line).compareTo(Integer.valueOf(other.line));
      if (v != 0) {
        return v;
      } else {
        return Long.valueOf(location).compareTo(Long.valueOf(other.location));
      }
    }
  }

  public static native void setBreakpoint(Executable m, long loc);
  public static void setBreakpoint(Executable m, LineNumber l) {
    setBreakpoint(m, l.location);
  }

  public static native void clearBreakpoint(Executable m, long loc);
  public static void clearBreakpoint(Executable m, LineNumber l) {
    clearBreakpoint(m, l.location);
  }

  private static native Object[] getLineNumberTableNative(Executable m);
  public static LineNumber[] getLineNumberTable(Executable m) {
    Object[] nativeTable = getLineNumberTableNative(m);
    long[] location = (long[])(nativeTable[0]);
    int[] lines = (int[])(nativeTable[1]);
    if (lines.length != location.length) {
      throw new Error("Lines and locations have different lengths!");
    }
    LineNumber[] out = new LineNumber[lines.length];
    for (int i = 0; i < lines.length; i++) {
      out[i] = new LineNumber(location[i], lines[i]);
    }
    return out;
  }

  public static native long getStartLocation(Executable m);

  public static int locationToLine(Executable m, long location) {
    try {
      Breakpoint.LineNumber[] lines = Breakpoint.getLineNumberTable(m);
      int best = -1;
      for (Breakpoint.LineNumber l : lines) {
        if (l.location > location) {
          break;
        } else {
          best = l.line;
        }
      }
      return best;
    } catch (Exception e) {
      return -1;
    }
  }

  public static long lineToLocation(Executable m, int line) throws Exception {
    try {
      Breakpoint.LineNumber[] lines = Breakpoint.getLineNumberTable(m);
      for (Breakpoint.LineNumber l : lines) {
        if (l.line == line) {
          return l.location;
        }
      }
      throw new Exception("Unable to find line " + line + " in " + m);
    } catch (Exception e) {
      throw new Exception("Unable to get line number info for " + m, e);
    }
  }
}

