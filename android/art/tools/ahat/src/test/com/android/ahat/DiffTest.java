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

package com.android.ahat;

import com.android.ahat.heapdump.AhatHeap;
import com.android.ahat.heapdump.AhatInstance;
import java.io.IOException;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

public class DiffTest {
  @Test
  public void diffMatchedHeap() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatHeap a = dump.getAhatSnapshot().getHeap("app");
    assertNotNull(a);
    AhatHeap b = dump.getBaselineAhatSnapshot().getHeap("app");
    assertNotNull(b);
    assertEquals(a.getBaseline(), b);
    assertEquals(b.getBaseline(), a);
  }

  @Test
  public void diffUnchanged() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatInstance a = dump.getDumpedAhatInstance("unchangedObject");
    assertNotNull(a);

    AhatInstance b = dump.getBaselineDumpedAhatInstance("unchangedObject");
    assertNotNull(b);
    assertEquals(a, b.getBaseline());
    assertEquals(b, a.getBaseline());
    assertEquals(a.getSite(), b.getSite().getBaseline());
    assertEquals(b.getSite(), a.getSite().getBaseline());
  }

  @Test
  public void diffAdded() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatInstance a = dump.getDumpedAhatInstance("addedObject");
    assertNotNull(a);
    assertNull(dump.getBaselineDumpedAhatInstance("addedObject"));
    assertTrue(a.getBaseline().isPlaceHolder());
  }

  @Test
  public void diffRemoved() throws IOException {
    TestDump dump = TestDump.getTestDump();

    assertNull(dump.getDumpedAhatInstance("removedObject"));
    AhatInstance b = dump.getBaselineDumpedAhatInstance("removedObject");
    assertNotNull(b);
    assertTrue(b.getBaseline().isPlaceHolder());
  }

  @Test
  public void diffClassRemoved() throws IOException {
    TestDump dump = TestDump.getTestDump("O.hprof", "L.hprof", null);
    AhatHandler handler = new ObjectsHandler(dump.getAhatSnapshot());
    TestHandler.testNoCrash(handler, "http://localhost:7100/objects?class=java.lang.Class");
  }
}
