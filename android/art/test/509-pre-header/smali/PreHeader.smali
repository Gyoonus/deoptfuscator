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

.class public LPreHeader;

.super Ljava/lang/Object;

# Label names in this method are taken from the original apk
# that exposed the crash. The crash was due to fixing a critical
# edge and not preserving the invariant that the pre header of a loop
# is the first predecessor of the loop header.
.method public static method()V
   .registers 2
   const/4 v0, 0
   const/4 v1, 0
   goto :b31
   :b23
   if-eqz v0, :b25
   goto :b23
   :b25
   return-void
   :b31
   if-eqz v0, :b23
   if-eqz v1, :bexit
   goto :b31
   :bexit
   return-void
.end method
