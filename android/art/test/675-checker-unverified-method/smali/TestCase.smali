# Copyright (C) 2018 The Android Open Source Project
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

#
# Ensure foo() does not analyze unverified bad() always-throws property.
#
## CHECK-START: void TestCase.foo(java.lang.Object) inliner (after)
## CHECK-DAG: InvokeStaticOrDirect method_name:TestCase.bar always_throws:true
## CHECK-DAG: InvokeStaticOrDirect method_name:TestCase.bad always_throws:false
.method public static foo(Ljava/lang/Object;)V
  .registers 1
  if-nez v0, :Skip1
  invoke-static {}, LTestCase;->bar()V
:Skip1
  if-nez v0, :Skip2
  invoke-static {}, LTestCase;->bad()Lwont/be/Resolvable;
:Skip2
  return-void
.end method

#
# Method bar() that verifies (but is never called).
#
.method public static bar()V
  .registers 1
  new-instance v0, Ljava/lang/RuntimeException;
  invoke-direct {v0}, Ljava/lang/RuntimeException;-><init>()V
  throw v0
.end method

#
# Method bad() that fails to verify (but is never called).
#
.method public static bad()Lwont/be/Resolvable;
  .registers 1
  invoke-static {}, LTestCase;->bar()Lwont/be/Resolvable;
  move-result-object v0
  throw v0
.end method

