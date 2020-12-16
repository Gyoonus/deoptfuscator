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

.class public LDCE;

.super Ljava/lang/Object;

.method public static method([I)LDCE;
   .registers 2
   const/4 v0, 0
   # Jump over the code that requires the null constant
   # so that the compiler sees the null constant as dead code.
   if-eq v0, v0, :end
   invoke-static {v0}, LDCE;->method([I)LDCE;
   :end
   invoke-static {}, LDCE;->$inline$returnNull()LDCE;
   move-result-object v0
   return-object v0
.end method

.method public static $inline$returnNull()LDCE;
   .registers 2
   const/4 v0, 0
   # Return null to make `method` call GetConstantNull again.
   return-object v0
.end method
