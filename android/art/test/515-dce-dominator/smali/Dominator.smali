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

.class public LDominator;

.super Ljava/lang/Object;

.method public static method(I)I
   .registers 2
   const/4 v0, 0
   :b1
   if-ne v0, v0, :b3
   :b2
   if-eq v0, p0, :b4
   :b5
   if-eq v0, p0, :b2
   goto :b6
   :b4
   goto :b7
   :b3
   goto :b6
   :b6
   goto :b7
   :b7
   return v1
.end method
