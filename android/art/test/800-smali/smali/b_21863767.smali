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

.class public LB21863767;

.super Ljava/lang/Object;

.method public static run()V
   .registers 2
   return-void
   goto :start
   :start
   # The following is dead code but used to crash the compiler.
   const/4 v0, 0
   return-wide v0
   return v0
   return-object v0
.end method
