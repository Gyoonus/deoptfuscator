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

.class public LArrayGet;
.super Ljava/lang/Object;


# Test phi with fixed-type ArrayGet as an input and a matching second input.
# The phi should be typed accordingly.

## CHECK-START: void ArrayGet.matchingFixedType(float[], float) builder (after)
## CHECK-NOT: Phi

## CHECK-START-DEBUGGABLE: void ArrayGet.matchingFixedType(float[], float) builder (after)
## CHECK-DAG:  <<Arg1:f\d+>> ParameterValue
## CHECK-DAG:  <<Aget:f\d+>> ArrayGet
## CHECK-DAG:  {{f\d+}}      Phi [<<Aget>>,<<Arg1>>] reg:0
.method public static matchingFixedType([FF)V
  .registers 8

  const v0, 0x0
  const v1, 0x1

  aget v0, p0, v0       # read value
  add-float v2, v0, v1  # float use fixes type

  float-to-int v2, p1
  if-eqz v2, :after
  move v0, p1
  :after
  # v0 = Phi [ArrayGet, Arg1] => float

  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method


# Test phi with fixed-type ArrayGet as an input and a conflicting second input.
# The phi should be eliminated due to the conflict.

## CHECK-START: void ArrayGet.conflictingFixedType(float[], int) builder (after)
## CHECK-NOT: Phi

## CHECK-START-DEBUGGABLE: void ArrayGet.conflictingFixedType(float[], int) builder (after)
## CHECK-NOT: Phi
.method public static conflictingFixedType([FI)V
  .registers 8

  const v0, 0x0
  const v1, 0x1

  aget v0, p0, v0       # read value
  add-float v2, v0, v1  # float use fixes type

  if-eqz p1, :after
  move v0, p1
  :after
  # v0 = Phi [ArrayGet, Arg1] => conflict

  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method


# Same test as the one above, only this time tests that type of ArrayGet is not
# changed.

## CHECK-START: void ArrayGet.conflictingFixedType2(int[], float) builder (after)
## CHECK-NOT: Phi

## CHECK-START-DEBUGGABLE: void ArrayGet.conflictingFixedType2(int[], float) builder (after)
## CHECK-NOT: Phi

## CHECK-START-DEBUGGABLE: void ArrayGet.conflictingFixedType2(int[], float) builder (after)
## CHECK:     {{i\d+}} ArrayGet
.method public static conflictingFixedType2([IF)V
  .registers 8

  const v0, 0x0
  const v1, 0x1

  aget v0, p0, v0       # read value
  add-int v2, v0, v1    # int use fixes type

  float-to-int v2, p1
  if-eqz v2, :after
  move v0, p1
  :after
  # v0 = Phi [ArrayGet, Arg1] => conflict

  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method


# Test phi with free-type ArrayGet as an input and a matching second input.
# The phi should be typed accordingly.

## CHECK-START: void ArrayGet.matchingFreeType(float[], float) builder (after)
## CHECK-NOT: Phi

## CHECK-START-DEBUGGABLE: void ArrayGet.matchingFreeType(float[], float) builder (after)
## CHECK-DAG:  <<Arg1:f\d+>> ParameterValue
## CHECK-DAG:  <<Aget:f\d+>> ArrayGet
## CHECK-DAG:                ArraySet [{{l\d+}},{{i\d+}},<<Aget>>]
## CHECK-DAG:  {{f\d+}}      Phi [<<Aget>>,<<Arg1>>] reg:0
.method public static matchingFreeType([FF)V
  .registers 8

  const v0, 0x0
  const v1, 0x1

  aget v0, p0, v0       # read value, should be float but has no typed use
  aput v0, p0, v1       # aput does not disambiguate the type

  float-to-int v2, p1
  if-eqz v2, :after
  move v0, p1
  :after
  # v0 = Phi [ArrayGet, Arg1] => float

  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method


# Test phi with free-type ArrayGet as an input and a conflicting second input.
# The phi will be kept and typed according to the second input despite the
# conflict.

## CHECK-START: void ArrayGet.conflictingFreeType(int[], float) builder (after)
## CHECK-NOT: Phi

## CHECK-START-DEBUGGABLE: void ArrayGet.conflictingFreeType(int[], float) builder (after)
## CHECK-NOT: Phi

.method public static conflictingFreeType([IF)V
  .registers 8

  const v0, 0x0
  const v1, 0x1

  aget v0, p0, v0       # read value, should be int but has no typed use
  aput v0, p0, v1

  float-to-int v2, p1
  if-eqz v2, :after
  move v0, p1
  :after
  # v0 = Phi [ArrayGet, Arg1] => float

  invoke-static {}, Ljava/lang/System;->nanoTime()J  # create an env use
  return-void
.end method


# Test that real use of ArrayGet is propagated through phis. The following test
# case uses ArrayGet indirectly through two phis. It also creates an unused
# conflicting phi which should not be preserved.

## CHECK-START: void ArrayGet.conflictingPhiUses(int[], float, boolean, boolean, boolean) builder (after)
## CHECK:         InvokeStaticOrDirect env:[[{{i\d+}},{{i\d+}},_,{{i\d+}},{{.*}}

.method public static conflictingPhiUses([IFZZZ)V
  .registers 10

  const v0, 0x0

  # Create v1 = Phi [0x0, int ArrayGet]
  move v1, v0
  if-eqz p2, :else1
  aget v1, p0, v0
  :else1

  # Create v2 = Phi [v1, float]
  move v2, v1
  if-eqz p3, :else2
  move v2, p1
  :else2

  # Create v3 = Phi [v1, int]
  move v3, v1
  if-eqz p4, :else3
  move v3, v0
  :else3

  # Use v3 as int.
  add-int/lit8 v4, v3, 0x2a

  # Create env uses.
  invoke-static {}, Ljava/lang/System;->nanoTime()J

  return-void
.end method

# Test that the right ArrayGet equivalent is always selected. The following test
# case uses ArrayGet as float through one phi and as an indeterminate type through
# another. The situation needs to be resolved so that only one instruction
# remains.

## CHECK-START: void ArrayGet.typedVsUntypedPhiUse(float[], float, boolean, boolean) builder (after)
## CHECK:         {{f\d+}} ArrayGet

## CHECK-START: void ArrayGet.typedVsUntypedPhiUse(float[], float, boolean, boolean) builder (after)
## CHECK-NOT:     {{i\d+}} ArrayGet

.method public static typedVsUntypedPhiUse([FFZZ)V
  .registers 10

  const v0, 0x0

  # v1 = float ArrayGet
  aget v1, p0, v0

  # Create v2 = Phi [v1, 0.0f]
  move v2, v1
  if-eqz p2, :else1
  move v2, v0
  :else1

  # Use v2 as float
  cmpl-float v2, v2, p1

  # Create v3 = Phi [v1, 0.0f]
  move v3, v1
  if-eqz p3, :else2
  move v3, v0
  :else2

  # Use v3 without a determinate type.
  aput v3, p0, v0

  return-void
.end method
