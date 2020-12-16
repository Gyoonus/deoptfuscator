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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

public class Test925 {
  public static void run() throws Exception {
    doTest();
  }

  private static void doTest() throws Exception {
    Thread t1 = Thread.currentThread();
    ThreadGroup curGroup = t1.getThreadGroup();

    ThreadGroup rootGroup = curGroup;
    while (rootGroup.getParent() != null) {
      rootGroup = rootGroup.getParent();
    }

    ThreadGroup topGroups[] = getTopThreadGroups();
    if (topGroups == null || topGroups.length != 1 || topGroups[0] != rootGroup) {
      System.out.println(Arrays.toString(topGroups));
      throw new RuntimeException("Unexpected topGroups");
    }

    printThreadGroupInfo(curGroup);
    printThreadGroupInfo(rootGroup);

    waitGroupChildren(rootGroup, 5 /* # daemons */, 30 /* timeout in seconds */);

    checkChildren(curGroup);
  }

  private static void printThreadGroupInfo(ThreadGroup tg) {
    Object[] threadGroupInfo = getThreadGroupInfo(tg);
    if (threadGroupInfo == null || threadGroupInfo.length != 4) {
      System.out.println(Arrays.toString(threadGroupInfo));
      throw new RuntimeException("threadGroupInfo length wrong");
    }

    System.out.println(tg);
    System.out.println("  " + threadGroupInfo[0]);  // Parent
    System.out.println("  " + threadGroupInfo[1]);  // Name
    System.out.println("  " + threadGroupInfo[2]);  // Priority
    System.out.println("  " + threadGroupInfo[3]);  // Daemon
  }

  private static void checkChildren(ThreadGroup tg) {
    Object[] data = getThreadGroupChildren(tg);
    Thread[] threads = (Thread[])data[0];
    ThreadGroup[] groups = (ThreadGroup[])data[1];

    List<Thread> threadList = new ArrayList<>(Arrays.asList(threads));

    // Filter out JIT thread. It may or may not be there depending on configuration.
    Iterator<Thread> it = threadList.iterator();
    while (it.hasNext()) {
      Thread t = it.next();
      if (t.getName().startsWith("Jit thread pool worker")) {
        it.remove();
        break;
      }
    }

    Collections.sort(threadList, THREAD_COMP);

    Arrays.sort(groups, THREADGROUP_COMP);
    System.out.println(tg.getName() + ":");
    System.out.println("  " + threadList);
    System.out.println("  " + Arrays.toString(groups));

    if (tg.getParent() != null) {
      checkChildren(tg.getParent());
    }
  }

  private static void waitGroupChildren(ThreadGroup tg, int expectedChildCount, int timeoutS)
      throws Exception {
    for (int i = 0; i <  timeoutS; i++) {
      Object[] data = getThreadGroupChildren(tg);
      Thread[] threads = (Thread[])data[0];
      if (threads.length == expectedChildCount) {
        return;
      }
      Thread.sleep(1000);
    }

    Object[] data = getThreadGroupChildren(tg);
    Thread[] threads = (Thread[])data[0];
    System.out.println(Arrays.toString(threads));
    throw new RuntimeException("Waited unsuccessfully for " + expectedChildCount + " children.");
  }

  private final static Comparator<Thread> THREAD_COMP = new Comparator<Thread>() {
    public int compare(Thread o1, Thread o2) {
      return o1.getName().compareTo(o2.getName());
    }
  };

  private final static Comparator<ThreadGroup> THREADGROUP_COMP = new Comparator<ThreadGroup>() {
    public int compare(ThreadGroup o1, ThreadGroup o2) {
      return o1.getName().compareTo(o2.getName());
    }
  };

  private static native ThreadGroup[] getTopThreadGroups();
  private static native Object[] getThreadGroupInfo(ThreadGroup tg);
  // Returns an array where element 0 is an array of threads and element 1 is an array of groups.
  private static native Object[] getThreadGroupChildren(ThreadGroup tg);
}
