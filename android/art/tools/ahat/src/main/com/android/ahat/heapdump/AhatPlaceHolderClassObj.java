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

/**
 * PlaceHolder instance to take the place of a real AhatClassObj for
 * the purposes of displaying diffs.
 *
 * This should be created through a call to newPlaceHolder();
 */
class AhatPlaceHolderClassObj extends AhatClassObj {
  AhatPlaceHolderClassObj(AhatClassObj baseline) {
    super(-1, baseline.getClassName());
    setBaseline(baseline);
    baseline.setBaseline(this);
  }

  @Override public Size getSize() {
    return Size.ZERO;
  }

  @Override public Size getRetainedSize(AhatHeap heap) {
    return Size.ZERO;
  }

  @Override public Size getTotalRetainedSize() {
    return Size.ZERO;
  }

  @Override public AhatHeap getHeap() {
    return getBaseline().getHeap().getBaseline();
  }

  @Override public String getClassName() {
    return getBaseline().getClassName();
  }

  @Override public String toString() {
    return getBaseline().toString();
  }

  @Override public boolean isPlaceHolder() {
    return true;
  }

  @Override public String getName() {
    return getBaseline().asClassObj().getName();
  }

  @Override public AhatClassObj getSuperClassObj() {
    return getBaseline().asClassObj().getSuperClassObj().getBaseline().asClassObj();
  }

  @Override public AhatInstance getClassLoader() {
    return getBaseline().asClassObj().getClassLoader().getBaseline();
  }

  @Override public Field[] getInstanceFields() {
    return getBaseline().asClassObj().getInstanceFields();
  }
}
