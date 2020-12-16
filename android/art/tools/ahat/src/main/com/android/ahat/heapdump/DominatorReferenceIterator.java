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

package com.android.ahat.heapdump;

import java.util.Iterator;
import java.util.NoSuchElementException;

/**
 * Reference iterator used for the dominators computation.
 * This visits only strong references.
 */
class DominatorReferenceIterator implements Iterator<AhatInstance>,
                                            Iterable<AhatInstance> {
  private Iterator<Reference> mIter;
  private AhatInstance mNext;

  public DominatorReferenceIterator(Iterable<Reference> iter) {
    mIter = iter.iterator();
    mNext = null;
  }

  @Override
  public boolean hasNext() {
    while (mNext == null && mIter.hasNext()) {
      Reference ref = mIter.next();
      if (ref.strong) {
        mNext = ref.ref;
      }
    }
    return mNext != null;
  }

  @Override
  public AhatInstance next() {
    if (hasNext()) {
      AhatInstance next = mNext;
      mNext = null;
      return next;
    }
    throw new NoSuchElementException();
  }

  @Override
  public Iterator<AhatInstance> iterator() {
    return this;
  }
}
