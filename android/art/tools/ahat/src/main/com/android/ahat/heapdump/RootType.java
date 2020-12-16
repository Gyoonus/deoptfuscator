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

/**
 * Enumeration representing object root types as defined in the binary heap
 * dump format specification.
 */
public enum RootType {
  /**
   * There is a JNI Global Reference for the object in question.
   */
  JNI_GLOBAL      (1 <<  0),

  /**
   * There is a JNI Local Reference for the object in question.
   */
  JNI_LOCAL       (1 <<  1),

  /**
   * The object in question is a parameter or local variable of a running
   * method.
   */
  JAVA_FRAME      (1 <<  2),

  /**
   * The object in question is a parameter of a running JNI method.
   */
  NATIVE_STACK    (1 <<  3),

  /**
   * The object is a class object that cannot be unloaded.
   */
  STICKY_CLASS    (1 <<  4),

  /**
   * The object is referenced from an active thread block.
   */
  THREAD_BLOCK    (1 <<  5),

  /**
   * The object's monitor is currently in use.
   */
  MONITOR         (1 <<  6),

  /**
   * The object is a running thread.
   */
  THREAD          (1 <<  7),

  /**
   * The object is an interned string.
   */
  INTERNED_STRING (1 <<  8),

  /**
   * The object is being used by the debugger.
   */
  DEBUGGER        (1 <<  9),

  /**
   * The object is being used by the VM internally.
   */
  VM_INTERNAL     (1 << 10),

  /**
   * The object has no given reason for being considered a root.
   */
  UNKNOWN         (1 << 11),

  /**
   * The object's monitor is currently in use from JNI.
   */
  JNI_MONITOR     (1 << 12),

  /**
   * The object is waiting to be finalized.
   */
  FINALIZING      (1 << 13);

  final int mask;

  RootType(int mask) {
    this.mask = mask;
  }
}
