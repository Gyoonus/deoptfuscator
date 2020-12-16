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

package com.android.ahat;

import com.android.ahat.heapdump.AhatInstance;
import com.android.ahat.heapdump.AhatSnapshot;
import com.android.ahat.heapdump.Site;
import java.io.IOException;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;

public class SiteTest {
  @Test
  public void objectsAllocatedAtKnownSites() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatSnapshot snapshot = dump.getAhatSnapshot();

    AhatInstance oKnownSite = dump.getDumpedAhatInstance("objectAllocatedAtKnownSite");
    Site sKnownSite = oKnownSite.getSite();
    assertEquals("DumpedStuff.java", sKnownSite.getFilename());
    assertEquals("allocateObjectAtKnownSite", sKnownSite.getMethodName());
    assertEquals(29, sKnownSite.getLineNumber());
    assertSame(sKnownSite, snapshot.getSite(sKnownSite.getId()));

    AhatInstance oKnownSubSite = dump.getDumpedAhatInstance("objectAllocatedAtKnownSubSite");
    Site sKnownSubSite = oKnownSubSite.getSite();
    assertEquals("DumpedStuff.java", sKnownSubSite.getFilename());
    assertEquals("allocateObjectAtKnownSubSite", sKnownSubSite.getMethodName());
    assertEquals(37, sKnownSubSite.getLineNumber());
    assertSame(sKnownSubSite, snapshot.getSite(sKnownSubSite.getId()));

    Site sKnownSubSiteParent = sKnownSubSite.getParent();
    assertEquals("DumpedStuff.java", sKnownSubSiteParent.getFilename());
    assertEquals("allocateObjectAtKnownSite", sKnownSubSiteParent.getMethodName());
    assertEquals(30, sKnownSubSiteParent.getLineNumber());
    assertSame(sKnownSubSiteParent, snapshot.getSite(sKnownSubSiteParent.getId()));

    assertNotSame(sKnownSite, sKnownSubSiteParent);
    assertSame(sKnownSite.getParent(), sKnownSubSiteParent.getParent());

    Site sKnownSiteParent = sKnownSite.getParent();
    assertEquals("DumpedStuff.java", sKnownSiteParent.getFilename());
    assertEquals("<init>", sKnownSiteParent.getMethodName());
    assertEquals(45, sKnownSiteParent.getLineNumber());
    assertSame(sKnownSiteParent, snapshot.getSite(sKnownSiteParent.getId()));

    AhatInstance oObfSuperSite = dump.getDumpedAhatInstance("objectAllocatedAtObfSuperSite");
    Site sObfSuperSite = oObfSuperSite.getSite();
    assertEquals("SuperDumpedStuff.java", sObfSuperSite.getFilename());
    assertEquals("allocateObjectAtObfSuperSite", sObfSuperSite.getMethodName());
    assertEquals(22, sObfSuperSite.getLineNumber());
    assertSame(sObfSuperSite, snapshot.getSite(sObfSuperSite.getId()));

    AhatInstance oUnObfSuperSite = dump.getDumpedAhatInstance("objectAllocatedAtUnObfSuperSite");
    Site sUnObfSuperSite = oUnObfSuperSite.getSite();
    assertEquals("SuperDumpedStuff.java", sUnObfSuperSite.getFilename());
    assertEquals("allocateObjectAtUnObfSuperSite", sUnObfSuperSite.getMethodName());
    assertEquals(26, sUnObfSuperSite.getLineNumber());
    assertSame(sUnObfSuperSite, snapshot.getSite(sUnObfSuperSite.getId()));

    AhatInstance oOverriddenSite = dump.getDumpedAhatInstance("objectAllocatedAtOverriddenSite");
    Site sOverriddenSite = oOverriddenSite.getSite();
    assertEquals("DumpedStuff.java", sOverriddenSite.getFilename());
    assertEquals("allocateObjectAtOverriddenSite", sOverriddenSite.getMethodName());
    assertEquals(41, sOverriddenSite.getLineNumber());
    assertSame(sOverriddenSite, snapshot.getSite(sOverriddenSite.getId()));
  }
}
