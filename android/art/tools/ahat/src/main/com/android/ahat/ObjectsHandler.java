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
import com.android.ahat.heapdump.Site;
import com.android.ahat.heapdump.Sort;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

class ObjectsHandler implements AhatHandler {
  private static final String OBJECTS_ID = "objects";

  private AhatSnapshot mSnapshot;

  public ObjectsHandler(AhatSnapshot snapshot) {
    mSnapshot = snapshot;
  }

  @Override
  public void handle(Doc doc, Query query) throws IOException {
    int id = query.getInt("id", 0);
    String className = query.get("class", null);
    String heapName = query.get("heap", null);
    Site site = mSnapshot.getSite(id);

    List<AhatInstance> insts = new ArrayList<AhatInstance>();
    site.getObjects(heapName, className, insts);
    Collections.sort(insts, Sort.defaultInstanceCompare(mSnapshot));

    doc.title("Objects");

    SizeTable.table(doc, mSnapshot.isDiffed(),
        new Column("Heap"),
        new Column("Object"));

    SubsetSelector<AhatInstance> selector = new SubsetSelector(query, OBJECTS_ID, insts);
    for (AhatInstance inst : selector.selected()) {
      AhatInstance base = inst.getBaseline();
      SizeTable.row(doc, inst.getSize(), base.getSize(),
          DocString.text(inst.getHeap().getName()),
          Summarizer.summarize(inst));
    }
    SizeTable.end(doc);
    selector.render(doc);
  }
}

