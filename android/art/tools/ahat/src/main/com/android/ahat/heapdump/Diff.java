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

package com.android.ahat.heapdump;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Deque;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * Provides a static method to diff two heap dumps.
 */
public class Diff {
  private Diff() {
  }

  /**
   * Performs a diff between two heap lists.
   *
   * Heaps are diffed based on heap name. PlaceHolder heaps will be added to
   * the given lists as necessary so that every heap in A has a corresponding
   * heap in B and vice-versa.
   */
  private static void heaps(List<AhatHeap> a, List<AhatHeap> b) {
    int asize = a.size();
    int bsize = b.size();
    for (int i = 0; i < bsize; i++) {
      // Set the B heap's baseline as null to mark that we have not yet
      // matched it with an A heap.
      b.get(i).setBaseline(null);
    }

    for (int i = 0; i < asize; i++) {
      AhatHeap aheap = a.get(i);
      aheap.setBaseline(null);
      for (int j = 0; j < bsize; j++) {
        AhatHeap bheap = b.get(j);
        if (bheap.getBaseline() == null && aheap.getName().equals(bheap.getName())) {
          // We found a match between aheap and bheap.
          aheap.setBaseline(bheap);
          bheap.setBaseline(aheap);
          break;
        }
      }

      if (aheap.getBaseline() == null) {
        // We did not find any match for aheap in snapshot B.
        // Create a placeholder heap in snapshot B to use as the baseline.
        b.add(AhatHeap.newPlaceHolderHeap(aheap.getName(), aheap));
      }
    }

    // Make placeholder heaps in snapshot A for any unmatched heaps in
    // snapshot B.
    for (int i = 0; i < bsize; i++) {
      AhatHeap bheap = b.get(i);
      if (bheap.getBaseline() == null) {
        a.add(AhatHeap.newPlaceHolderHeap(bheap.getName(), bheap));
      }
    }
  }

  /**
   * Key represents an equivalence class of AhatInstances that are allowed to
   * be considered for correspondence between two different snapshots.
   */
  private static class Key {
    // Corresponding objects must belong to classes of the same name.
    private final String mClass;

    // Corresponding objects must belong to heaps of the same name.
    private final String mHeapName;

    // Corresponding string objects must have the same value.
    // mStringValue is set to the empty string for non-string objects.
    private final String mStringValue;

    // Corresponding class objects must have the same class name.
    // mClassName is set to the empty string for non-class objects.
    private final String mClassName;

    // Corresponding array objects must have the same length.
    // mArrayLength is set to 0 for non-array objects.
    private final int mArrayLength;


    private Key(AhatInstance inst) {
      mClass = inst.getClassName();
      mHeapName = inst.getHeap().getName();
      mClassName = inst.isClassObj() ? inst.asClassObj().getName() : "";
      String string = inst.asString();
      mStringValue = string == null ? "" : string;
      AhatArrayInstance array = inst.asArrayInstance();
      mArrayLength = array == null ? 0 : array.getLength();
    }

    /**
     * Return the key for the given instance.
     */
    public static Key keyFor(AhatInstance inst) {
      return new Key(inst);
    }

    @Override
    public boolean equals(Object other) {
      if (!(other instanceof Key)) {
        return false;
      }
      Key o = (Key)other;
      return mClass.equals(o.mClass)
          && mHeapName.equals(o.mHeapName)
          && mStringValue.equals(o.mStringValue)
          && mClassName.equals(o.mClassName)
          && mArrayLength == o.mArrayLength;
    }

    @Override
    public int hashCode() {
      return Objects.hash(mClass, mHeapName, mStringValue, mClassName, mArrayLength);
    }
  }

  private static class InstanceListPair {
    public final List<AhatInstance> a;
    public final List<AhatInstance> b;

    public InstanceListPair() {
      this.a = new ArrayList<AhatInstance>();
      this.b = new ArrayList<AhatInstance>();
    }

    public InstanceListPair(List<AhatInstance> a, List<AhatInstance> b) {
      this.a = a;
      this.b = b;
    }
  }

  /**
   * Recursively create place holder instances for the given instance and
   * every instance dominated by that instance.
   * Returns the place holder instance created for the given instance.
   * Adds all allocated placeholders to the given placeholders list.
   */
  private static AhatInstance createPlaceHolders(AhatInstance inst,
      List<AhatInstance> placeholders) {
    // Don't actually use recursion, because we could easily smash the stack.
    // Instead we iterate.
    AhatInstance result = inst.newPlaceHolderInstance();
    placeholders.add(result);
    Deque<AhatInstance> deque = new ArrayDeque<AhatInstance>();
    deque.push(inst);
    while (!deque.isEmpty()) {
      inst = deque.pop();

      for (AhatInstance child : inst.getDominated()) {
        placeholders.add(child.newPlaceHolderInstance());
        deque.push(child);
      }
    }
    return result;
  }

  /**
   * Recursively diff two dominator trees of instances.
   * PlaceHolder objects are appended to the lists as needed to ensure every
   * object has a corresponding baseline in the other list. All PlaceHolder
   * objects are also appended to the given placeholders list, so their Site
   * info can be updated later on.
   */
  private static void instances(List<AhatInstance> a, List<AhatInstance> b,
      List<AhatInstance> placeholders) {
    // Don't actually use recursion, because we could easily smash the stack.
    // Instead we iterate.
    Deque<InstanceListPair> deque = new ArrayDeque<InstanceListPair>();
    deque.push(new InstanceListPair(a, b));
    while (!deque.isEmpty()) {
      InstanceListPair p = deque.pop();

      // Group instances of the same equivalence class together.
      Map<Key, InstanceListPair> byKey = new HashMap<Key, InstanceListPair>();
      for (AhatInstance inst : p.a) {
        Key key = Key.keyFor(inst);
        InstanceListPair pair = byKey.get(key);
        if (pair == null) {
          pair = new InstanceListPair();
          byKey.put(key, pair);
        }
        pair.a.add(inst);
      }
      for (AhatInstance inst : p.b) {
        Key key = Key.keyFor(inst);
        InstanceListPair pair = byKey.get(key);
        if (pair == null) {
          pair = new InstanceListPair();
          byKey.put(key, pair);
        }
        pair.b.add(inst);
      }

      // diff objects from the same key class.
      for (InstanceListPair pair : byKey.values()) {
        // Sort by retained size and assume the elements at the top of the lists
        // correspond to each other in that order. This could probably be
        // improved if desired, but it gives good enough results for now.
        Collections.sort(pair.a, Sort.INSTANCE_BY_TOTAL_RETAINED_SIZE);
        Collections.sort(pair.b, Sort.INSTANCE_BY_TOTAL_RETAINED_SIZE);

        int common = Math.min(pair.a.size(), pair.b.size());
        for (int i = 0; i < common; i++) {
          AhatInstance ainst = pair.a.get(i);
          AhatInstance binst = pair.b.get(i);
          ainst.setBaseline(binst);
          binst.setBaseline(ainst);
          deque.push(new InstanceListPair(ainst.getDominated(), binst.getDominated()));
        }

        // Add placeholder objects for anything leftover.
        for (int i = common; i < pair.a.size(); i++) {
          p.b.add(createPlaceHolders(pair.a.get(i), placeholders));
        }

        for (int i = common; i < pair.b.size(); i++) {
          p.a.add(createPlaceHolders(pair.b.get(i), placeholders));
        }
      }
    }
  }

  /**
   * Sets the baseline for root and all its descendants to baseline.
   */
  private static void setSitesBaseline(Site root, Site baseline) {
    root.setBaseline(baseline);
    for (Site child : root.getChildren()) {
      setSitesBaseline(child, baseline);
    }
  }

  /**
   * Recursively diff the two sites, setting them and their descendants as
   * baselines for each other as appropriate.
   *
   * This requires that instances have already been diffed. In particular, we
   * require all AhatClassObjs in one snapshot have corresponding (possibly
   * place-holder) AhatClassObjs in the other snapshot.
   */
  private static void sites(Site a, Site b) {
    // Set the sites as baselines of each other.
    a.setBaseline(b);
    b.setBaseline(a);

    // Set the site's ObjectsInfos as baselines of each other. This implicitly
    // adds new empty ObjectsInfo as needed.
    for (Site.ObjectsInfo ainfo : a.getObjectsInfos()) {
      AhatClassObj baseClassObj = null;
      if (ainfo.classObj != null) {
        baseClassObj = (AhatClassObj) ainfo.classObj.getBaseline();
      }
      ainfo.setBaseline(b.getObjectsInfo(ainfo.heap.getBaseline(), baseClassObj));
    }
    for (Site.ObjectsInfo binfo : b.getObjectsInfos()) {
      AhatClassObj baseClassObj = null;
      if (binfo.classObj != null) {
        baseClassObj = (AhatClassObj) binfo.classObj.getBaseline();
      }
      binfo.setBaseline(a.getObjectsInfo(binfo.heap.getBaseline(), baseClassObj));
    }

    // Set B children's baselines as null to mark that we have not yet matched
    // them with A children.
    for (Site bchild : b.getChildren()) {
      bchild.setBaseline(null);
    }

    for (Site achild : a.getChildren()) {
      achild.setBaseline(null);
      for (Site bchild : b.getChildren()) {
        if (achild.getLineNumber() == bchild.getLineNumber()
            && achild.getMethodName().equals(bchild.getMethodName())
            && achild.getSignature().equals(bchild.getSignature())
            && achild.getFilename().equals(bchild.getFilename())) {
          // We found a match between achild and bchild.
          sites(achild, bchild);
          break;
        }
      }

      if (achild.getBaseline() == null) {
        // We did not find any match for achild in site B.
        // Use B for the baseline of achild and its descendants.
        setSitesBaseline(achild, b);
      }
    }

    for (Site bchild : b.getChildren()) {
      if (bchild.getBaseline() == null) {
        setSitesBaseline(bchild, a);
      }
    }
  }

  /**
   * Performs a diff of two snapshots.
   * Each snapshot will be set as the baseline for the other snapshot.
   * <p>
   * The diff algorithm attempts to match instances in snapshot <code>a</code>
   * to corresponding instances in snapshot <code>b</code>. The snapshots need
   * not come from the same running process, application version, or platform
   * version.
   *
   * @param a one of the snapshots to diff
   * @param b the other of the snapshots to diff
   */
  public static void snapshots(AhatSnapshot a, AhatSnapshot b) {
    a.setBaseline(b);
    b.setBaseline(a);

    // Diff the heaps of each snapshot.
    heaps(a.getHeaps(), b.getHeaps());

    // Diff the instances of each snapshot.
    List<AhatInstance> placeholders = new ArrayList<AhatInstance>();
    instances(a.getRooted(), b.getRooted(), placeholders);

    // Diff the sites of each snapshot.
    // This requires the instances have already been diffed.
    sites(a.getRootSite(), b.getRootSite());

    // Add placeholders to their corresponding sites.
    // This requires the sites have already been diffed.
    for (AhatInstance placeholder : placeholders) {
      placeholder.getBaseline().getSite().getBaseline().addInstance(placeholder);
    }
  }
}
