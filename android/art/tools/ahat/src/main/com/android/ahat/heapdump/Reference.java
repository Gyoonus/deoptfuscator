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
 * Reference represents a reference from 'src' to 'ref' through 'field'.
 * Field is a string description for human consumption. This is typically
 * either "." followed by the field name or an array subscript such as "[4]".
 * 'strong' is true if this is a strong reference, false if it is a
 * weak/soft/other reference.
 */
class Reference {
  public final AhatInstance src;
  public final String field;
  public final AhatInstance ref;
  public final boolean strong;

  public Reference(AhatInstance src, String field, AhatInstance ref, boolean strong) {
    this.src = src;
    this.field = field;
    this.ref = ref;
    this.strong = strong;
  }
}
