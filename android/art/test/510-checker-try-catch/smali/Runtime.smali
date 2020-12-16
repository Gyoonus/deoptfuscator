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

.class public LRuntime;
.super Ljava/lang/Object;

# The following tests all share the same structure, signature and return values:
#  - foo(false, false):  normal path,         returns 42
#  - foo(true, false):   exceptional path #1, returns 3
#  - foo(false, true):   exceptional path #2, returns 8
#  - foo(true, true):    undefined


# Test register allocation of 32-bit core intervals crossing catch block positions.

## CHECK-START: int Runtime.testUseAfterCatch_int(boolean, boolean) register (after)
## CHECK-NOT:     Phi is_catch_phi:true

.method public static testUseAfterCatch_int(ZZ)I
  .registers 6

  sget-object v0, LRuntime;->intArray:[I
  const/4 v1, 0
  aget v1, v0, v1
  const/4 v2, 1
  aget v2, v0, v2
  const/4 v3, 2
  aget v3, v0, v3

  :try_start
  invoke-static {p0}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  invoke-static {p1}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  return v3  # Normal path return.

  :catch_all
  if-eqz p0, :second_throw
  return v1  # Exceptional path #1 return.

  :second_throw
  return v2  # Exceptional path #2 return.
.end method


# Test register allocation of 64-bit core intervals crossing catch block positions.

# The sum of the low and high 32 bits treated as integers is returned to prove
# that both vregs allocated correctly.

## CHECK-START: int Runtime.testUseAfterCatch_long(boolean, boolean) register (after)
## CHECK-NOT:     Phi is_catch_phi:true

.method public static testUseAfterCatch_long(ZZ)I
  .registers 10

  sget-object v0, LRuntime;->longArray:[J
  const/4 v1, 0
  aget-wide v1, v0, v1
  const/4 v3, 1
  aget-wide v3, v0, v3
  const/4 v5, 2
  aget-wide v5, v0, v5

  :try_start
  invoke-static {p0}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  invoke-static {p1}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  const v0, 32
  ushr-long v7, v5, v0
  long-to-int v5, v5
  long-to-int v7, v7
  add-int/2addr v5, v7
  return v5  # Normal path return.

  :catch_all
  const v0, 32
  if-eqz p0, :second_throw

  ushr-long v7, v1, v0
  long-to-int v1, v1
  long-to-int v7, v7
  add-int/2addr v1, v7
  return v1  # Exceptional path #1 return.

  :second_throw
  ushr-long v7, v3, v0
  long-to-int v3, v3
  long-to-int v7, v7
  add-int/2addr v3, v7
  return v3  # Exceptional path #2 return.
.end method


# Test register allocation of 32-bit floating-point intervals crossing catch block positions.

## CHECK-START: int Runtime.testUseAfterCatch_float(boolean, boolean) register (after)
## CHECK-NOT:     Phi is_catch_phi:true

.method public static testUseAfterCatch_float(ZZ)I
  .registers 6

  sget-object v0, LRuntime;->floatArray:[F
  const/4 v1, 0
  aget v1, v0, v1
  const/4 v2, 1
  aget v2, v0, v2
  const/4 v3, 2
  aget v3, v0, v3

  :try_start
  invoke-static {p0}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  invoke-static {p1}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  float-to-int v3, v3
  return v3  # Normal path return.

  :catch_all
  if-eqz p0, :second_throw
  float-to-int v1, v1
  return v1  # Exceptional path #1 return.

  :second_throw
  float-to-int v2, v2
  return v2  # Exceptional path #2 return.
.end method


# Test register allocation of 64-bit floating-point intervals crossing catch block positions.

## CHECK-START: int Runtime.testUseAfterCatch_double(boolean, boolean) register (after)
## CHECK-NOT:     Phi is_catch_phi:true

.method public static testUseAfterCatch_double(ZZ)I
  .registers 10

  sget-object v0, LRuntime;->doubleArray:[D
  const/4 v1, 0
  aget-wide v1, v0, v1
  const/4 v3, 1
  aget-wide v3, v0, v3
  const/4 v5, 2
  aget-wide v5, v0, v5

  :try_start
  invoke-static {p0}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  invoke-static {p1}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  double-to-int v5, v5
  return v5  # Normal path return.

  :catch_all
  if-eqz p0, :second_throw
  double-to-int v1, v1
  return v1  # Exceptional path #1 return.

  :second_throw
  double-to-int v3, v3
  return v3  # Exceptional path #2 return.
.end method


# Test catch-phi runtime support for constant values.

# Register v0 holds different constants at two throwing instructions. Runtime is
# expected to load them from stack map and copy to the catch phi's location.

## CHECK-START: int Runtime.testCatchPhi_const(boolean, boolean) register (after)
## CHECK-DAG:     <<Const3:i\d+>> IntConstant 3
## CHECK-DAG:     <<Const8:i\d+>> IntConstant 8
## CHECK-DAG:                     Phi [<<Const3>>,<<Const8>>] is_catch_phi:true

.method public static testCatchPhi_const(ZZ)I
  .registers 3

  :try_start
  const v0, 3
  invoke-static {p0}, LRuntime;->$noinline$ThrowIfTrue(Z)V

  const v0, 8
  invoke-static {p1}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  const v0, 42
  return v0  # Normal path return.

  :catch_all
  return v0  # Exceptional path #1/#2 return.
.end method


# Test catch-phi runtime support for 32-bit values stored in core registers.

# Register v0 holds different integer values at two throwing instructions.
# Runtime is expected to find their location in the stack map and copy the value
# to the location of the catch phi.

## CHECK-START: int Runtime.testCatchPhi_int(boolean, boolean) register (after)
## CHECK-DAG:     <<Val1:i\d+>> ArrayGet
## CHECK-DAG:     <<Val2:i\d+>> ArrayGet
## CHECK-DAG:                   Phi [<<Val1>>,<<Val2>>] is_catch_phi:true

.method public static testCatchPhi_int(ZZ)I
  .registers 6

  sget-object v0, LRuntime;->intArray:[I
  const/4 v1, 0
  aget v1, v0, v1
  const/4 v2, 1
  aget v2, v0, v2
  const/4 v3, 2
  aget v3, v0, v3

  :try_start
  move v0, v1  # Set catch phi value
  invoke-static {p0}, LRuntime;->$noinline$ThrowIfTrue(Z)V

  move v0, v2  # Set catch phi value
  invoke-static {p1}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  return v3  # Normal path return.

  :catch_all
  return v0  # Exceptional path #1/#2 return.
.end method


# Test catch-phi runtime support for 64-bit values stored in core registers.

# Register pair (v0, v1) holds different long values at two throwing instructions.
# Runtime is expected to find their location in the stack map and copy the value
# to the location of the catch phi. The sum of the low and high 32 bits treated
# as integers is returned to prove that both vregs were copied.

# Note: values will be spilled on x86 because of too few callee-save core registers.

## CHECK-START: int Runtime.testCatchPhi_long(boolean, boolean) register (after)
## CHECK-DAG:     <<Val1:j\d+>> ArrayGet
## CHECK-DAG:     <<Val2:j\d+>> ArrayGet
## CHECK-DAG:                   Phi [<<Val1>>,<<Val2>>] is_catch_phi:true

.method public static testCatchPhi_long(ZZ)I
  .registers 10

  sget-object v0, LRuntime;->longArray:[J
  const/4 v2, 0
  aget-wide v2, v0, v2
  const/4 v4, 1
  aget-wide v4, v0, v4
  const/4 v6, 2
  aget-wide v6, v0, v6

  :try_start
  move-wide v0, v2  # Set catch phi value
  invoke-static {p0}, LRuntime;->$noinline$ThrowIfTrue(Z)V

  move-wide v0, v4  # Set catch phi value
  invoke-static {p1}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  const v2, 32
  ushr-long v2, v6, v2
  long-to-int v2, v2
  long-to-int v6, v6
  add-int/2addr v6, v2
  return v6  # Normal path return.

  :catch_all
  const v2, 32
  ushr-long v2, v0, v2
  long-to-int v2, v2
  long-to-int v0, v0
  add-int/2addr v0, v2
  return v0  # Exceptional path #1/#2 return.
.end method


# Test catch-phi runtime support for 32-bit values stored in FPU registers.

# Register v0 holds different float values at two throwing instructions. Runtime
# is expected to find their location in the stack map and copy the value to the
# location of the catch phi. The value is converted to int and returned.

# Note: values will be spilled on x86 as there are no callee-save FPU registers.

## CHECK-START: int Runtime.testCatchPhi_float(boolean, boolean) register (after)
## CHECK-DAG:     <<Val1:f\d+>> ArrayGet
## CHECK-DAG:     <<Val2:f\d+>> ArrayGet
## CHECK-DAG:                   Phi [<<Val1>>,<<Val2>>] is_catch_phi:true

.method public static testCatchPhi_float(ZZ)I
  .registers 6

  sget-object v0, LRuntime;->floatArray:[F
  const/4 v1, 0
  aget v1, v0, v1
  const/4 v2, 1
  aget v2, v0, v2
  const/4 v3, 2
  aget v3, v0, v3

  :try_start
  move v0, v1  # Set catch phi value
  invoke-static {p0}, LRuntime;->$noinline$ThrowIfTrue(Z)V

  move v0, v2  # Set catch phi value
  invoke-static {p1}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  float-to-int v3, v3
  return v3  # Normal path return.

  :catch_all
  float-to-int v0, v0
  return v0  # Exceptional path #1/#2 return.
.end method


# Test catch-phi runtime support for 64-bit values stored in FPU registers.

# Register pair (v0, v1) holds different double values at two throwing instructions.
# Runtime is expected to find their location in the stack map and copy the value
# to the location of the catch phi. The value is converted to int and returned.
# Values were chosen so that all 64 bits are used.

# Note: values will be spilled on x86 as there are no callee-save FPU registers.

## CHECK-START: int Runtime.testCatchPhi_double(boolean, boolean) register (after)
## CHECK-DAG:     <<Val1:d\d+>> ArrayGet
## CHECK-DAG:     <<Val2:d\d+>> ArrayGet
## CHECK-DAG:                   Phi [<<Val1>>,<<Val2>>] is_catch_phi:true

.method public static testCatchPhi_double(ZZ)I
  .registers 10

  sget-object v0, LRuntime;->doubleArray:[D
  const/4 v2, 0
  aget-wide v2, v0, v2
  const/4 v4, 1
  aget-wide v4, v0, v4
  const/4 v6, 2
  aget-wide v6, v0, v6

  :try_start
  move-wide v0, v2  # Set catch phi value
  invoke-static {p0}, LRuntime;->$noinline$ThrowIfTrue(Z)V

  move-wide v0, v4  # Set catch phi value
  invoke-static {p1}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  double-to-int v6, v6
  return v6

  :catch_all
  double-to-int v0, v0
  return v0
.end method

# Test catch-phi runtime support for 32-bit values stored on the stack.

# Register v0 holds different integer values at two throwing instructions.
# These values were forced to spill by an always-throwing try/catch after their
# definition. Runtime is expected to find their location in the stack map and
# copy the value to the location of the catch phi. The value is then returned.

## CHECK-START: int Runtime.testCatchPhi_singleSlot(boolean, boolean) register (after)
## CHECK:         <<Val1:i\d+>> ArrayGet
## CHECK-NEXT:                  ParallelMove moves:[{{.*->}}{{\d+}}(sp)]
## CHECK:         <<Val2:i\d+>> ArrayGet
## CHECK-NEXT:                  ParallelMove moves:[{{.*->}}{{\d+}}(sp)]
## CHECK:                       Phi [<<Val1>>,<<Val2>>] is_catch_phi:true

.method public static testCatchPhi_singleSlot(ZZ)I
  .registers 6

  sget-object v0, LRuntime;->intArray:[I
  const/4 v1, 0
  aget v1, v0, v1
  const/4 v2, 1
  aget v2, v0, v2
  const/4 v3, 2
  aget v3, v0, v3

  # Insert a try/catch to force v1,v2,v3 to spill.
  :try_start_spill
  const/4 v0, 1
  invoke-static {v0}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end_spill
  .catchall {:try_start_spill .. :try_end_spill} :catch_all_spill
  return v0         # Unreachable
  :catch_all_spill  # Catch and continue

  :try_start
  move v0, v1  # Set catch phi value
  invoke-static {p0}, LRuntime;->$noinline$ThrowIfTrue(Z)V

  move v0, v2  # Set catch phi value
  invoke-static {p1}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  return v3  # Normal path return.

  :catch_all
  return v0  # Exceptional path #1/#2 return.
.end method

# Test catch-phi runtime support for 64-bit values stored on the stack.

# Register pair (v0, v1) holds different double values at two throwing instructions.
# These values were forced to spill by an always-throwing try/catch after their
# definition. Runtime is expected to find their location in the stack map and
# copy the value to the location of the catch phi. The value is converted to int
# and returned. Values were chosen so that all 64 bits are used.

## CHECK-START: int Runtime.testCatchPhi_doubleSlot(boolean, boolean) register (after)
## CHECK:         <<Val1:d\d+>> ArrayGet
## CHECK-NEXT:                  ParallelMove moves:[{{.*->}}2x{{\d+}}(sp)]
## CHECK:         <<Val2:d\d+>> ArrayGet
## CHECK-NEXT:                  ParallelMove moves:[{{.*->}}2x{{\d+}}(sp)]
## CHECK:                       Phi [<<Val1>>,<<Val2>>] is_catch_phi:true

.method public static testCatchPhi_doubleSlot(ZZ)I
  .registers 10

  sget-object v0, LRuntime;->doubleArray:[D
  const/4 v2, 0
  aget-wide v2, v0, v2
  const/4 v4, 1
  aget-wide v4, v0, v4
  const/4 v6, 2
  aget-wide v6, v0, v6

  # Insert a try/catch to force (v2, v3), (v4, v5), (v6, v7) to spill.
  :try_start_spill
  const/4 v0, 1
  invoke-static {v0}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end_spill
  .catchall {:try_start_spill .. :try_end_spill} :catch_all_spill
  return v0         # Unreachable
  :catch_all_spill  # Catch and continue

  :try_start
  move-wide v0, v2  # Set catch phi value
  invoke-static {p0}, LRuntime;->$noinline$ThrowIfTrue(Z)V

  move-wide v0, v4  # Set catch phi value
  invoke-static {p1}, LRuntime;->$noinline$ThrowIfTrue(Z)V
  :try_end
  .catchall {:try_start .. :try_end} :catch_all

  double-to-int v6, v6
  return v6  # Normal path return.

  :catch_all
  double-to-int v0, v0
  return v0  # Exceptional path #1/#2 return.
.end method



# Helper methods and initialization.

.method public static $noinline$ThrowIfTrue(Z)V
  .registers 2
  if-nez p0, :throw
  return-void

  :throw
  new-instance v0, Ljava/lang/Exception;
  invoke-direct {v0}, Ljava/lang/Exception;-><init>()V
  throw v0
.end method

.method public static constructor <clinit>()V
  .registers 2

  const/4 v1, 4

  new-array v0, v1, [I
  fill-array-data v0, :array_int
  sput-object v0, LRuntime;->intArray:[I

  new-array v0, v1, [J
  fill-array-data v0, :array_long
  sput-object v0, LRuntime;->longArray:[J

  new-array v0, v1, [F
  fill-array-data v0, :array_float
  sput-object v0, LRuntime;->floatArray:[F

  new-array v0, v1, [D
  fill-array-data v0, :array_double
  sput-object v0, LRuntime;->doubleArray:[D

  return-void

:array_int
.array-data 4
  0x03  # int 3
  0x08  # int 8
  0x2a  # int 42
.end array-data

:array_long
.array-data 8
  0x0000000100000002L # long (1 << 32) + 2
  0x0000000500000003L # long (5 << 32) + 3
  0x0000001e0000000cL # long (30 << 32) + 12
.end array-data

:array_float
.array-data 4
  0x40400000  # float 3
  0x41000000  # float 8
  0x42280000  # float 42
.end array-data

:array_double
.array-data 8
  0x400b333333333333L  # double 3.4
  0x4020cccccccccccdL  # double 8.4
  0x4045333333333333L  # double 42.4
.end array-data
.end method

.field public static intArray:[I
.field public static longArray:[J
.field public static floatArray:[F
.field public static doubleArray:[D
