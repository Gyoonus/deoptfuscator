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

.class public LTypePropagation;

.super Ljava/lang/Object;

.method public static method([I)V
   .registers 2
   const/4 v0, 0
   # When building the SSA graph, we will create a phi for v0, which will be of type
   # integer. Only when we get rid of that phi in the redundant phi elimination will
   # we realize it's just null.
   :start
   if-eq v1, v0, :end
     if-eq v1, v0, :start
   :end
   return-void
.end method
