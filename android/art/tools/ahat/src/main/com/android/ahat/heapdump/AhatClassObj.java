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

import java.util.AbstractList;
import java.util.Arrays;
import java.util.List;

/**
 * A class from a parsed heap dump.
 * In addition to those methods inherited from {@link AhatInstance}, the class
 * provides methods for accessing information about the class object, such as
 * the class loader, superclass, static field values and instance field
 * descriptors.
 */
public class AhatClassObj extends AhatInstance {
  private String mClassName;
  private AhatClassObj mSuperClassObj;
  private AhatInstance mClassLoader;
  private FieldValue[] mStaticFieldValues;
  private Field[] mInstanceFields;
  private long mStaticFieldsSize;
  private long mInstanceSize;

  AhatClassObj(long id, String className) {
    super(id);
    mClassName = className;
  }

  void initialize(AhatClassObj superClass,
                  long instanceSize,
                  Field[] instanceFields,
                  long staticFieldsSize) {
    mSuperClassObj = superClass;
    mInstanceSize = instanceSize;
    mInstanceFields = instanceFields;
    mStaticFieldsSize = staticFieldsSize;
  }

  void initialize(AhatInstance classLoader, FieldValue[] staticFields) {
    mClassLoader = classLoader;
    mStaticFieldValues = staticFields;
  }

  @Override
  long getExtraJavaSize() {
    return mStaticFieldsSize;
  }

  /**
   * Returns the name of the class this is a class object for.
   * For example, "java.lang.String".
   *
   * @return the name of the class
   */
  public String getName() {
    return mClassName;
  }

  /**
   * Returns the superclass of this class object.
   *
   * @return the superclass object
   */
  public AhatClassObj getSuperClassObj() {
    return mSuperClassObj;
  }

  /**
   * Returns the class loader of this class object.
   *
   * @return the class loader object
   */
  public AhatInstance getClassLoader() {
    return mClassLoader;
  }

  /**
   * Returns the size of instances of this object.
   * The size returned is as reported in the heap dump.
   *
   * @return the class instance size
   */
  public long getInstanceSize() {
    return mInstanceSize;
  }

  /**
   * Returns the static field values for this class object.
   *
   * @return the static field values
   */
  public List<FieldValue> getStaticFieldValues() {
    return Arrays.asList(mStaticFieldValues);
  }

  /**
   * Returns the fields of instances of this class.
   * Does not include fields from the super class of this class.
   *
   * @return the instance fields
   */
  public Field[] getInstanceFields() {
    return mInstanceFields;
  }

  @Override
  Iterable<Reference> getReferences() {
    List<Reference> refs = new AbstractList<Reference>() {
      @Override
      public int size() {
        return mStaticFieldValues.length;
      }

      @Override
      public Reference get(int index) {
        FieldValue field = mStaticFieldValues[index];
        Value value = field.value;
        if (value != null && value.isAhatInstance()) {
          return new Reference(AhatClassObj.this, "." + field.name, value.asAhatInstance(), true);
        }
        return null;
      }
    };
    return new SkipNullsIterator(refs);
  }

  @Override public boolean isClassObj() {
    return true;
  }

  @Override public AhatClassObj asClassObj() {
    return this;
  }

  @Override public String toString() {
    return "class " + mClassName;
  }

  @Override AhatInstance newPlaceHolderInstance() {
    return new AhatPlaceHolderClassObj(this);
  }
}
