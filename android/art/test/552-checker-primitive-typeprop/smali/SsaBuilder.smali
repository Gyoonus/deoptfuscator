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

.class public LSsaBuilder;
.super Ljava/lang/Object;

# Check that a dead phi with a live equivalent is replaced in an environment. The
# following test case throws an exception and uses v0 afterwards. However, v0
# contains a phi that is interpreted as int for the environment, and as float for
# instruction use. SsaBuilder must substitute the int variant before removing it,
# otherwise running the code with an array short enough to throw will crash at
# runtime because v0 is undefined.

## CHECK-START: int SsaBuilder.environmentPhi(boolean, int[]) builder (after)
## CHECK-DAG:     <<Cst0:f\d+>>  FloatConstant 0
## CHECK-DAG:     <<Cst2:f\d+>>  FloatConstant 2
## CHECK-DAG:     <<Phi:f\d+>>   Phi [<<Cst0>>,<<Cst2>>]
## CHECK-DAG:                    BoundsCheck env:[[<<Phi>>,{{i\d+}},{{z\d+}},{{l\d+}}]]

.method public static environmentPhi(Z[I)I
  .registers 4

  const v0, 0x0
  if-eqz p0, :else
  const v0, 0x40000000
  :else
  # v0 = phi that can be both int and float

  :try_start
  const v1, 0x3
  aput v1, p1, v1
  const v0, 0x1     # generate catch phi for v0
  const v1, 0x4
  aput v1, p1, v1
  :try_end
  .catchall {:try_start .. :try_end} :use_as_float

  :use_as_float
  float-to-int v0, v0
  return v0
.end method