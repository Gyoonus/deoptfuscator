# Copyright (C) 2017 The Android Open Source Project
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

.method public static foo()V
  .registers 6
  new-instance v0, Ljava/lang/Exception;
  invoke-direct {v0}, Ljava/lang/Exception;-><init>()V
  const-string v1, "Zero"
  :try_start
  const-string v1, "One"
  const-string v1, "Two"
  const-string v1, "Three"
  throw v0
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  :catch_all
  sget-object v5, Ljava/lang/System;->out:Ljava/io/PrintStream;
  invoke-virtual {v5, v1}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
  throw v0
.end method
