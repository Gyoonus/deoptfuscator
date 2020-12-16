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
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

class SitePrinter {
  public static void printSite(AhatSnapshot snapshot, Doc doc, Query query, String id, Site site) {
    List<Site> path = new ArrayList<Site>();
    for (Site parent = site; parent != null; parent = parent.getParent()) {
      path.add(parent);
    }
    Collections.reverse(path);


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
            return "Stack Frame";
          }

          public DocString render(Site element) {
            DocString str = new DocString();
            if (element.getParent() != null) {
              str.append("→ ");
            }
            return str.append(Summarizer.summarize(element));
          }
        };
        return Collections.singletonList(value);
      }
    };
    HeapTable.render(doc, query, id, table, snapshot, path);
  }
}
