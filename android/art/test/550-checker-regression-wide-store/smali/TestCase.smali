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

.method public static $noinline$throw()V
  .registers 1
  new-instance v0, Ljava/lang/Exception;
  invoke-direct {v0}, Ljava/lang/Exception;-><init>()V
  throw v0
.end method

# Test storing into the high vreg of a wide pair. This scenario has runtime
# behaviour implications so we run it from Main.main.

## CHECK-START: int TestCase.invalidateLow(long) builder (after)
## CHECK-DAG: <<Cst0:i\d+>> IntConstant 0
## CHECK-DAG: <<Arg:j\d+>>  ParameterValue
## CHECK-DAG: <<Cast:i\d+>> TypeConversion [<<Arg>>]
## CHECK-DAG: InvokeStaticOrDirect method_name:java.lang.System.nanoTime env:[[_,<<Cst0>>,<<Arg>>,_]]
## CHECK-DAG: InvokeStaticOrDirect method_name:TestCase.$noinline$throw  env:[[_,<<Cast>>,<<Arg>>,_]]

.method public static invalidateLow(J)I
  .registers 4

  const/4 v1, 0x0

  :try_start
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  move-wide v0, p0
  long-to-int v1, v0
  invoke-static {}, LTestCase;->$noinline$throw()V
  :try_end
  .catchall {:try_start .. :try_end} :catchall

  :catchall
  return v1

.end method

# Test that storing a wide invalidates the value in the high vreg. This
# cannot be detected from runtime so we only test the environment with Checker.

## CHECK-START: void TestCase.invalidateHigh1(long) builder (after)
## CHECK-DAG: <<Arg:j\d+>>  ParameterValue
## CHECK-DAG: InvokeStaticOrDirect method_name:java.lang.System.nanoTime env:[[<<Arg>>,_,<<Arg>>,_]]

.method public static invalidateHigh1(J)V
  .registers 4

  const/4 v1, 0x0
  move-wide v0, p0
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  return-void

.end method

## CHECK-START: void TestCase.invalidateHigh2(long) builder (after)
## CHECK-DAG: <<Arg:j\d+>>  ParameterValue
## CHECK-DAG: InvokeStaticOrDirect method_name:java.lang.System.nanoTime env:[[<<Arg>>,_,_,<<Arg>>,_]]

.method public static invalidateHigh2(J)V
  .registers 5

  move-wide v1, p0
  move-wide v0, p0
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  return-void

.end method
