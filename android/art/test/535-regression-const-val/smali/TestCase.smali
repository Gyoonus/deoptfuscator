# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.class public LTestCase;
.super Ljava/lang/Object;

.method public static testCase(ZZ)I
  .registers 5

  # Create Phi [ 0.0f, -0.25f ].
  # Binary representation of -0.25f has the most significant bit set.
  if-eqz p0, :else
  :then
    const v0, 0x0
    goto :merge
  :else
    const/high16 v0, 0xbe800000
  :merge

  # Now use as either float or int.
  if-eqz p1, :return
  float-to-int v0, v0
  :return
  return v0
.end method
