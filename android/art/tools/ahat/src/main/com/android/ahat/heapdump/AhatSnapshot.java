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

import com.android.ahat.dominators.DominatorsComputation;
import java.util.List;

/**
 * A parsed heap dump.
 * It contains methods to access the heaps, allocation sites, roots, classes,
 * and instances from the parsed heap dump.
 */
public class AhatSnapshot implements Diffable<AhatSnapshot> {
  private final Site mRootSite;

  private final SuperRoot mSuperRoot;

  // List of all ahat instances.
  private final Instances<AhatInstance> mInstances;

  private List<AhatHeap> mHeaps;

  private AhatSnapshot mBaseline = this;

  AhatSnapshot(SuperRoot root,
               Instances<AhatInstance> instances,
               List<AhatHeap> heaps,
               Site rootSite) {
    mSuperRoot = root;
    mInstances = instances;
    mHeaps = heaps;
    mRootSite = rootSite;

    // Update registered native allocation size.
    for (AhatInstance cleaner : mInstances) {
      AhatInstance.RegisteredNativeAllocation nra = cleaner.asRegisteredNativeAllocation();
      if (nra != null) {
        nra.referent.addRegisteredNativeSize(nra.size);
      }
    }

    AhatInstance.computeReverseReferences(mSuperRoot);
    DominatorsComputation.computeDominators(mSuperRoot);
    AhatInstance.computeRetainedSize(mSuperRoot, mHeaps.size());

    for (AhatHeap heap : mHeaps) {
      heap.addToSize(mSuperRoot.getRetainedSize(heap));
    }

    mRootSite.prepareForUse(0, mHeaps.size());
  }

  /**
   * Returns the instance with the given id in this snapshot.
   * Where the id of an instance x is x.getId().
   * Returns null if no instance with the given id is found.
   *
   * @param id the id of the instance to find
   * @return the instance with the given id
   */
  public AhatInstance findInstance(long id) {
    return mInstances.get(id);
  }

  /**
   * Returns the AhatClassObj with the given id in this snapshot.
   * Where the id of a class object x is x.getId().
   * Returns null if no class object with the given id is found.
   *
   * @param id the id of the class object to find
   * @return the class object with the given id
   */
  public AhatClassObj findClassObj(long id) {
    AhatInstance inst = findInstance(id);
    return inst == null ? null : inst.asClassObj();
  }

  /**
   * Returns the heap with the given name.
   * Where the name of a heap x is x.getName().
   * Returns null if no heap with the given name could be found.
   *
   * @param name the name of the heap to get
   * @return the heap with the given name
   */
  public AhatHeap getHeap(String name) {
    // We expect a small number of heaps (maybe 3 or 4 total), so a linear
    // search should be acceptable here performance wise.
    for (AhatHeap heap : getHeaps()) {
      if (heap.getName().equals(name)) {
        return heap;
      }
    }
    return null;
  }

  /**
   * Returns a list of heaps in the snapshot in canonical order.
   * <p>
   * Note: modifications to the returned list are visible to this
   * AhatSnapshot, which is used by diff to insert place holder heaps.
   *
   * @return list of heaps
   */
  public List<AhatHeap> getHeaps() {
    return mHeaps;
  }

  /**
   * Returns a collection of "rooted" instances.
   * An instance is "rooted" if it is a GC root, or if it is retained by more
   * than one GC root. These are reachable instances that are not immediately
   * dominated by any other instance in the heap.
   *
   * @return collection of rooted instances
   */
  public List<AhatInstance> getRooted() {
    return mSuperRoot.getDominated();
  }

  /**
   * Returns the root allocation site for this snapshot.
   *
   * @return the root allocation site
   */
  public Site getRootSite() {
    return mRootSite;
  }

  /**
   * Returns the site associated with the given id.
   * Where the id of a site x is x.getId().
   * Returns the root site if no site with the given id is found.
   *
   * @param id the id of the site to get
   * @return the site with the given id
   */
  public Site getSite(long id) {
    Site site = mRootSite.findSite(id);
    return site == null ? mRootSite : site;
  }

  void setBaseline(AhatSnapshot baseline) {
    mBaseline = baseline;
  }

  /**
   * Returns true if this snapshot has been diffed against a different
   * snapshot.
   *
   * @return true if the snapshot has been diffed
   */
  public boolean isDiffed() {
    return mBaseline != this;
  }

  @Override public AhatSnapshot getBaseline() {
    return mBaseline;
  }

  @Override public boolean isPlaceHolder() {
    return false;
  }
}
