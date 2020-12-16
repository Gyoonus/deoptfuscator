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
import com.android.ahat.heapdump.Site;
import com.android.ahat.heapdump.Sort;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

class SiteHandler implements AhatHandler {
  private static final String ALLOCATION_SITE_ID = "frames";
  private static final String SITES_CALLED_ID = "called";
  private static final String OBJECTS_ALLOCATED_ID = "objects";

  private AhatSnapshot mSnapshot;

  public SiteHandler(AhatSnapshot snapshot) {
    mSnapshot = snapshot;
  }

  @Override
  public void handle(Doc doc, Query query) throws IOException {
    int id = query.getInt("id", 0);
    Site site = mSnapshot.getSite(id);

    doc.title("Site");
    doc.big(Summarizer.summarize(site));

    doc.section("Allocation Site");
    SitePrinter.printSite(mSnapshot, doc, query, ALLOCATION_SITE_ID, site);

    doc.section("Sites Called from Here");
    List<Site> children = new ArrayList<Site>(site.getChildren());

    if (children.isEmpty()) {
      doc.println(DocString.text("(none)"));
    } else {
      Collections.sort(children, Sort.defaultSiteCompare(mSnapshot));
      HeapTable.TableConfig<Site> table = new HeapTable.TableConfig<Site>() {
        public String getHeapsDescription() {
          return "Reachable Bytes Allocated on Heap";
        }

        public long getSize(Site element, AhatHeap heap) {
          return element.getSize(heap).getSize();
        }

        public List<HeapTable.ValueConfig<Site>> getValueConfigs() {
          HeapTable.ValueConfig<Site> value = new HeapTable.ValueConfig<Site>() {
            public String getDescription() {
              return "Child Site";
            }

            public DocString render(Site element) {
              return Summarizer.summarize(element);
            }
          };
          return Collections.singletonList(value);
        }
      };
      HeapTable.render(doc, query, SITES_CALLED_ID, table, mSnapshot, children);
    }

    doc.section("Objects Allocated");
    SizeTable.table(doc, mSnapshot.isDiffed(),
        new Column("Instances", Column.Align.RIGHT),
        new Column("Δ", Column.Align.RIGHT, mSnapshot.isDiffed()),
        new Column("Heap"),
        new Column("Class"));

    List<Site.ObjectsInfo> infos = site.getObjectsInfos();
    Comparator<Site.ObjectsInfo> compare = Sort.withPriority(
        Sort.OBJECTS_INFO_BY_HEAP_NAME,
        Sort.OBJECTS_INFO_BY_SIZE,
        Sort.OBJECTS_INFO_BY_CLASS_NAME);
    Collections.sort(infos, compare);
    SubsetSelector<Site.ObjectsInfo> selector
      = new SubsetSelector(query, OBJECTS_ALLOCATED_ID, infos);
    for (Site.ObjectsInfo info : selector.selected()) {
      Site.ObjectsInfo baseinfo = info.getBaseline();
      String className = info.getClassName();
      SizeTable.row(doc, info.numBytes, baseinfo.numBytes,
          DocString.link(
            DocString.formattedUri("objects?id=%d&heap=%s&class=%s",
              site.getId(), info.heap.getName(), className),
            DocString.format("%,14d", info.numInstances)),
          DocString.delta(false, false, info.numInstances, baseinfo.numInstances),
          DocString.text(info.heap.getName()),
          Summarizer.summarize(info.classObj));
    }
    SizeTable.end(doc);
    selector.render(doc);
  }
}

