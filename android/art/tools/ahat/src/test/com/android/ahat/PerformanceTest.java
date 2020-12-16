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

package com.android.ahat;

import com.android.ahat.heapdump.AhatInstance;
import com.android.ahat.heapdump.AhatSnapshot;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import org.junit.Test;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

public class PerformanceTest {
  private static class NullOutputStream extends OutputStream {
    public void write(int b) throws IOException {
    }
  }

  @Test
  public void bigArray() throws IOException {
    // It should not take more than 1 second to load the default object view
    // for any object, including big arrays.
    TestDump dump = TestDump.getTestDump();

    AhatInstance bigArray = dump.getDumpedAhatInstance("bigArray");
    assertNotNull(bigArray);

    AhatSnapshot snapshot = dump.getAhatSnapshot();
    AhatHandler handler = new ObjectHandler(snapshot);

    PrintStream ps = new PrintStream(new NullOutputStream());
    HtmlDoc doc = new HtmlDoc(ps, DocString.text("bigArray test"), DocString.uri("style.css"));
    String uri = "http://localhost:7100/object?id=" + bigArray.getId();
    Query query = new Query(DocString.uri(uri));

    long start = System.currentTimeMillis();
    handler.handle(doc, query);
    long time = System.currentTimeMillis() - start;
    assertTrue("bigArray took too long: " + time + "ms", time < 1000);
  }
}
