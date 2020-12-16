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

package com.android.ahat.heapdump;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

/**
 * Provides Comparators and helper functions for sorting Instances, Sites, and
 * other things.
 * <p>
 * Note: The Comparators defined here impose orderings that are inconsistent
 * with equals. They should not be used for element lookup or search. They
 * should only be used for showing elements to the user in different orders.
 */
public class Sort {
  /**
   * Compares sizes by their total size.
   * This sorts sizes from smaller total size to larger total size.
   */
  public static final Comparator<Size> SIZE_BY_SIZE = new Comparator<Size>() {
    @Override
    public int compare(Size a, Size b) {
      return Long.compare(a.getSize(), b.getSize());
    }
  };

  /**
   * Compares instances by their total retained size.
   * Different instances with the same total retained size are considered
   * equal for the purposes of comparison.
   * This sorts instances from larger retained size to smaller retained size.
   */
  public static final Comparator<AhatInstance> INSTANCE_BY_TOTAL_RETAINED_SIZE
    = new Comparator<AhatInstance>() {
    @Override
    public int compare(AhatInstance a, AhatInstance b) {
      return SIZE_BY_SIZE.compare(b.getTotalRetainedSize(), a.getTotalRetainedSize());
    }
  };

  /**
   * Compares instances by their retained size for a given heap index.
   * Different instances with the same total retained size are considered
   * equal for the purposes of comparison.
   * This sorts instances from larger retained size to smaller retained size.
   */
  private static class InstanceByHeapRetainedSize implements Comparator<AhatInstance> {
    private AhatHeap mHeap;

    public InstanceByHeapRetainedSize(AhatHeap heap) {
      mHeap = heap;
    }

    @Override
    public int compare(AhatInstance a, AhatInstance b) {
      return SIZE_BY_SIZE.compare(b.getRetainedSize(mHeap), a.getRetainedSize(mHeap));
    }
  }

  /**
   * Compares objects based on a list of comparators, giving priority to the
   * earlier comparators in the list.
   */
  private static class WithPriority<T> implements Comparator<T> {
    private List<Comparator<T>> mComparators;

    /**
     * Constructs a comparator giving sort priority to earlier comparators in
     * the list.
     *
     * @param comparators the list of comparators to use for sorting
     */
    public WithPriority(Comparator<T>... comparators) {
      mComparators = Arrays.asList(comparators);
    }

    /**
     * Constructs a comparator giving sort priority to earlier comparators in
     * the list.
     *
     * @param comparators the list of comparators to use for sorting
     */
    public WithPriority(List<Comparator<T>> comparators) {
      mComparators = comparators;
    }

    @Override
    public int compare(T a, T b) {
      int res = 0;
      Iterator<Comparator<T>> iter = mComparators.iterator();
      while (res == 0 && iter.hasNext()) {
        res = iter.next().compare(a, b);
      }
      return res;
    }
  }

  /**
   * Returns a comparator that gives sort priority to earlier comparators in
   * the list.
   *
   * @param <T> the type of object being sorted
   * @param comparators the list of comparators to use for sorting
   * @return the composite comparator
   */
  public static <T> Comparator<T> withPriority(Comparator<T>... comparators) {
    return new WithPriority(comparators);
  }

  /**
   * Returns a comparator that gives a default instance sort for the given
   * snapshot.
   * Objects are sorted by retained size, with priority given to the "app"
   * heap if present.
   *
   * @param snapshot the snapshot to use the comparator with
   * @return the default instance comparator
   */
  public static Comparator<AhatInstance> defaultInstanceCompare(AhatSnapshot snapshot) {
    List<Comparator<AhatInstance>> comparators = new ArrayList<Comparator<AhatInstance>>();

    // Priority goes to the app heap, if we can find one.
    AhatHeap appHeap = snapshot.getHeap("app");
    if (appHeap != null) {
      comparators.add(new InstanceByHeapRetainedSize(appHeap));
    }

    // Next is by total retained size.
    comparators.add(INSTANCE_BY_TOTAL_RETAINED_SIZE);
    return new WithPriority<AhatInstance>(comparators);
  }

  /**
   * Compares Sites by the size of objects allocated on a given heap.
   * Different object infos with the same size on the given heap are
   * considered equal for the purposes of comparison.
   * This sorts sites from larger size to smaller size.
   */
  private static class SiteByHeapSize implements Comparator<Site> {
    AhatHeap mHeap;

    /**
     * Constructs a SiteByHeapSize comparator.
     *
     * @param heap the heap to use when comparing sizes
     */
    public SiteByHeapSize(AhatHeap heap) {
      mHeap = heap;
    }

    @Override
    public int compare(Site a, Site b) {
      return SIZE_BY_SIZE.compare(b.getSize(mHeap), a.getSize(mHeap));
    }
  }

  /**
   * Compares Sites by the total size of objects allocated.
   * This sorts sites from larger size to smaller size.
   */
  public static final Comparator<Site> SITE_BY_TOTAL_SIZE = new Comparator<Site>() {
    @Override
    public int compare(Site a, Site b) {
      return SIZE_BY_SIZE.compare(b.getTotalSize(), a.getTotalSize());
    }
  };

  /**
   * Compares Sites using a default comparison order.
   * This sorts sites from larger size to smaller size, giving preference to
   * sites with more allocation on the "app" heap, if present.
   *
   * @param snapshot the snapshot to use the comparator with
   * @return the default site comparator
   */
  public static Comparator<Site> defaultSiteCompare(AhatSnapshot snapshot) {
    List<Comparator<Site>> comparators = new ArrayList<Comparator<Site>>();

    // Priority goes to the app heap, if we can find one.
    AhatHeap appHeap = snapshot.getHeap("app");
    if (appHeap != null) {
      comparators.add(new SiteByHeapSize(appHeap));
    }

    // Next is by total size.
    comparators.add(SITE_BY_TOTAL_SIZE);
    return new WithPriority<Site>(comparators);
  }

  /**
   * Compare Site.ObjectsInfo by their size.
   * Different object infos with the same total retained size are considered
   * equal for the purposes of comparison.
   * This sorts object infos from larger retained size to smaller size.
   */
  public static final Comparator<Site.ObjectsInfo> OBJECTS_INFO_BY_SIZE
    = new Comparator<Site.ObjectsInfo>() {
    @Override
    public int compare(Site.ObjectsInfo a, Site.ObjectsInfo b) {
      return SIZE_BY_SIZE.compare(b.numBytes, a.numBytes);
    }
  };

  /**
   * Compares Site.ObjectsInfo by heap name.
   * Different object infos with the same heap name are considered equal for
   * the purposes of comparison.
   */
  public static final Comparator<Site.ObjectsInfo> OBJECTS_INFO_BY_HEAP_NAME
    = new Comparator<Site.ObjectsInfo>() {
    @Override
    public int compare(Site.ObjectsInfo a, Site.ObjectsInfo b) {
      return a.heap.getName().compareTo(b.heap.getName());
    }
  };

  /**
   * Compares Site.ObjectsInfo by class name.
   * Different object infos with the same class name are considered equal for
   * the purposes of comparison.
   */
  public static final Comparator<Site.ObjectsInfo> OBJECTS_INFO_BY_CLASS_NAME
    = new Comparator<Site.ObjectsInfo>() {
    @Override
    public int compare(Site.ObjectsInfo a, Site.ObjectsInfo b) {
      String aName = a.getClassName();
      String bName = b.getClassName();
      return aName.compareTo(bName);
    }
  };

  /**
   * Compares FieldValue by field name.
   */
  public static final Comparator<FieldValue> FIELD_VALUE_BY_NAME
    = new Comparator<FieldValue>() {
    @Override
    public int compare(FieldValue a, FieldValue b) {
      return a.name.compareTo(b.name);
    }
  };

  /**
   * Compares FieldValue by type name.
   */
  public static final Comparator<FieldValue> FIELD_VALUE_BY_TYPE
    = new Comparator<FieldValue>() {
    @Override
    public int compare(FieldValue a, FieldValue b) {
      return a.type.compareTo(b.type);
    }
  };
}

