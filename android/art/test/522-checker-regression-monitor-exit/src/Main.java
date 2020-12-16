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

import java.lang.reflect.Method;
import java.lang.reflect.Type;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.CancellationException;
import java.util.concurrent.TimeoutException;

public class Main {

  // Workaround for b/18051191.
  class InnerClass {}

  private static class HashCodeQuery implements Callable<Integer> {
    public HashCodeQuery(Object obj) {
      m_obj = obj;
    }

    public Integer call() {
      Integer result;
      try {
        Class<?> c = Class.forName("Test");
        Method m = c.getMethod("synchronizedHashCode", Object.class);
        result = (Integer) m.invoke(null, m_obj);
      } catch (Exception e) {
        System.out.println("Hash code query exception");
        e.printStackTrace(System.out);
        result = -1;
      }
      return result;
    }

    private Object m_obj;
    private int m_index;
  }

  public static void main(String args[]) throws Exception {
    Object obj = new Object();
    int numThreads = 10;

    ExecutorService pool = Executors.newFixedThreadPool(numThreads);

    List<HashCodeQuery> queries = new ArrayList<HashCodeQuery>(numThreads);
    for (int i = 0; i < numThreads; ++i) {
      queries.add(new HashCodeQuery(obj));
    }

    try {
      List<Future<Integer>> results = pool.invokeAll(queries);

      int hash = obj.hashCode();
      for (int i = 0; i < numThreads; ++i) {
        int result = results.get(i).get();
        if (hash != result) {
          throw new Error("Query #" + i + " wrong. Expected " + hash + ", got " + result);
        }
      }
      pool.shutdown();
    } catch (CancellationException ex) {
      System.out.println("Job timeout");
      System.exit(1);
    }
  }
}
