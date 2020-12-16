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

.class public LArraySet;
.super Ljava/lang/Object;

# Test ArraySet on int[] and float[] arrays. The input should be typed accordingly.
# Note that the input is a Phi to make sure primitive type propagation is re-run
# on the replaced inputs.

## CHECK-START: void ArraySet.ambiguousSet(int[], float[], boolean) builder (after)
## CHECK-DAG:     <<IntArray:l\d+>>    ParameterValue klass:int[]
## CHECK-DAG:     <<IntA:i\d+>>        IntConstant 0
## CHECK-DAG:     <<IntB:i\d+>>        IntConstant 1073741824
## CHECK-DAG:     <<IntPhi:i\d+>>      Phi [<<IntA>>,<<IntB>>] reg:0
## CHECK-DAG:     <<IntNC:l\d+>>       NullCheck [<<IntArray>>]
## CHECK-DAG:                          ArraySet [<<IntNC>>,{{i\d+}},<<IntPhi>>]

## CHECK-DAG:     <<FloatArray:l\d+>>  ParameterValue klass:float[]
## CHECK-DAG:     <<FloatA:f\d+>>      FloatConstant 0
## CHECK-DAG:     <<FloatB:f\d+>>      FloatConstant 2
## CHECK-DAG:     <<FloatPhi:f\d+>>    Phi [<<FloatA>>,<<FloatB>>] reg:0
## CHECK-DAG:     <<FloatNC:l\d+>>     NullCheck [<<FloatArray>>]
## CHECK-DAG:                          ArraySet [<<FloatNC>>,{{i\d+}},<<FloatPhi>>]

.method public static ambiguousSet([I[FZ)V
  .registers 8

  const v0, 0x0
  if-eqz p2, :else
  const v0, 0x40000000
  :else
  # v0 = Phi [0.0f, 2.0f]

  const v1, 0x1
  aput v0, p0, v1
  aput v0, p1, v1

  return-void
.end method
