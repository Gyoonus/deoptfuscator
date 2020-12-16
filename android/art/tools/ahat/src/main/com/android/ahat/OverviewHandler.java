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

import com.android.ahat.heapdump.AhatHeap;
import com.android.ahat.heapdump.AhatSnapshot;
import com.android.ahat.heapdump.Size;
import java.io.File;
import java.io.IOException;

class OverviewHandler implements AhatHandler {

  private AhatSnapshot mSnapshot;
  private File mHprof;
  private File mBaseHprof;

  public OverviewHandler(AhatSnapshot snapshot, File hprof, File basehprof) {
    mSnapshot = snapshot;
    mHprof = hprof;
    mBaseHprof = basehprof;
  }

  @Override
  public void handle(Doc doc, Query query) throws IOException {
    doc.title("Overview");

    doc.section("General Information");
    doc.descriptions();
    doc.description(
        DocString.text("ahat version"),
        DocString.format("ahat-%s", OverviewHandler.class.getPackage().getImplementationVersion()));
    doc.description(DocString.text("hprof file"), DocString.text(mHprof.toString()));
    if (mBaseHprof != null) {
      doc.description(DocString.text("baseline hprof file"), DocString.text(mBaseHprof.toString()));
    }
    doc.end();

    doc.section("Bytes Retained by Heap");
    printHeapSizes(doc);

    doc.big(Menu.getMenu());
  }

  private void printHeapSizes(Doc doc) {
    SizeTable.table(doc, new Column("Heap"), mSnapshot.isDiffed());
    Size totalSize = Size.ZERO;
    Size totalBase = Size.ZERO;
    for (AhatHeap heap : mSnapshot.getHeaps()) {
      Size size = heap.getSize();
      Size base = heap.getBaseline().getSize();
      if (!size.isZero() || !base.isZero()) {
        SizeTable.row(doc, DocString.text(heap.getName()), size, base);
        totalSize = totalSize.plus(size);
        totalBase = totalBase.plus(base);
      }
    }
    SizeTable.row(doc, DocString.text("Total"), totalSize, totalBase);
    SizeTable.end(doc);
  }
}

