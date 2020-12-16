#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.class public LTest;

.super Ljava/lang/Object;

.method public static run()V
   .registers 3
   new-instance v2, Ljava/lang/String;
   invoke-direct {v2}, Ljava/lang/String;-><init>()V
   const/4 v0, 0
   move v1, v0
   :start
   invoke-static {}, LMain;->blowup()V
   if-ne v1, v0, :end
   const/4 v2, 1
   invoke-static {v2}, Ljava/lang/Integer;->toString(I)Ljava/lang/String;
   move v2, v0
   # The call makes v2 float type.
   invoke-static {v2}, Ljava/lang/Float;->isNaN(F)Z
   const/4 v1, 1
   goto :start
   :end
   return-void
.end method

.method public static run2()V
   .registers 4
   new-instance v2, Ljava/lang/String;
   invoke-direct {v2}, Ljava/lang/String;-><init>()V
   const/4 v0, 0
   move v1, v0
   :start
   invoke-static {}, LMain;->blowup()V
   if-ne v1, v0, :end
   const/4 v2, 1
   invoke-static {v2}, Ljava/lang/Integer;->toString(I)Ljava/lang/String;
   move-result-object v3
   if-nez v3, :skip
   const/4 v0, 0
   :skip
   # The Phi merging 0 with 0 hides the constant from the Quick compiler.
   move v2, v0
   # The call makes v2 float type.
   invoke-static {v2}, Ljava/lang/Float;->isNaN(F)Z
   const/4 v1, 1
   goto :start
   :end
   return-void
.end method
