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

.class public LEquivalentPhi;

.super Ljava/lang/Object;

.method public static equivalentPhi([F)F
   .registers 5
   const/4 v0, 0x0
   # aget is initally expected to be an int, but will
   # rightly become a float after type propagation.
   aget v1, p0, v0
   move v2, v1
   if-eq v0, v0, :else
   move v2, v0
   :else
   # v2 will be a phi with (int, int) as input
   move v3, v2
   if-eq v0, v0, :else2
   move v3, v0
   # v3 will be a phi with (int, int) as input.
   : else2
   # This instruction will lead to creating a phi equivalent
   # for v3 with float type, which in turn will lead to creating
   # a phi equivalent for v2 of type float. We used to forget to
   # delete the old phi, which ends up having incompatible input
   # types.
   return v3
.end method
