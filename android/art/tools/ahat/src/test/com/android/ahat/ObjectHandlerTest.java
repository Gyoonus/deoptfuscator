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

import com.android.ahat.heapdump.AhatInstance;
import com.android.ahat.heapdump.AhatSnapshot;
import java.io.IOException;
import org.junit.Test;

import static org.junit.Assert.assertNotNull;

public class ObjectHandlerTest {
  @Test
  public void noCrashClassInstance() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatInstance object = dump.getDumpedAhatInstance("aPhantomReference");
    assertNotNull(object);

    AhatHandler handler = new ObjectHandler(dump.getAhatSnapshot());
    TestHandler.testNoCrash(handler, "http://localhost:7100/object?id=" + object.getId());
  }

  @Test
  public void noCrashClassObj() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatSnapshot snapshot = dump.getAhatSnapshot();
    AhatHandler handler = new ObjectHandler(snapshot);

    AhatInstance object = dump.findClass("Main");
    assertNotNull(object);

    TestHandler.testNoCrash(handler, "http://localhost:7100/object?id=" + object.getId());
  }

  @Test
  public void noCrashSystemClassObj() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatSnapshot snapshot = dump.getAhatSnapshot();
    AhatHandler handler = new ObjectHandler(snapshot);

    AhatInstance object = dump.findClass("java.lang.String");
    assertNotNull(object);

    TestHandler.testNoCrash(handler, "http://localhost:7100/object?id=" + object.getId());
  }

  @Test
  public void noCrashArrayInstance() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatInstance object = dump.getDumpedAhatInstance("gcPathArray");
    assertNotNull(object);

    AhatHandler handler = new ObjectHandler(dump.getAhatSnapshot());
    TestHandler.testNoCrash(handler, "http://localhost:7100/object?id=" + object.getId());
  }
}
