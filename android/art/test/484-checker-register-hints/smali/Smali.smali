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
.class public LSmali;
.super Ljava/lang/Object;

##  CHECK-START: void Smali.test3Order1(boolean, int, int, int, int, int) register (after)
##  CHECK:       name "B0"
##  CHECK-NOT:     ParallelMove
##  CHECK:       name "B1"
##  CHECK-NOT:   end_block
##  CHECK:         If
##  CHECK-NOT:     ParallelMove
##  CHECK:       name "B6"
##  CHECK-NOT:   end_block
##  CHECK:         InstanceFieldSet
#    We could check here that there is a parallel move, but it's only valid
#    for some architectures (for example x86), as other architectures may
#    not do move at all.
##  CHECK:       end_block
##  CHECK-NOT:     ParallelMove
.method public static test3Order1(ZIIIII)V
    .registers 14

    sget v0, LMain;->live1:I
    sget v1, LMain;->live2:I
    sget v2, LMain;->live3:I
    sget v5, LMain;->live0:I
    if-eqz p0, :cond_13

    sput v0, LMain;->live1:I

    :goto_c
    add-int v6, v0, v1
    add-int/2addr v6, v2
    add-int/2addr v6, v5
    sput v6, LMain;->live1:I

    return-void

    :cond_13
    sget-boolean v6, LMain;->y:Z

    if-eqz v6, :cond_1a
    sput v0, LMain;->live1:I
    goto :goto_c

    :cond_1a
    sget v3, LMain;->live4:I

    sget v4, LMain;->live5:I
    sget-object v6, LMain;->foo:LMain$Foo;
    add-int v7, v0, v4
    add-int/2addr v7, v3
    iput v7, v6, LMain$Foo;->field2:I
    sget-object v6, LMain;->foo:LMain$Foo;
    add-int v7, v1, v4
    add-int/2addr v7, v3
    iput v7, v6, LMain$Foo;->field3:I
    sget-object v6, LMain;->foo:LMain$Foo;
    add-int v7, v2, v4
    add-int/2addr v7, v3
    iput v7, v6, LMain$Foo;->field4:I
    sget-object v6, LMain;->foo:LMain$Foo;
    iput v3, v6, LMain$Foo;->field0:I
    sget-object v6, LMain;->foo:LMain$Foo;
    add-int v7, v4, v3
    iput v7, v6, LMain$Foo;->field1:I
    goto :goto_c
.end method

##  CHECK-START: void Smali.test3Order2(boolean, int, int, int, int, int) register (after)
##  CHECK:       name "B0"
##  CHECK-NOT:     ParallelMove
##  CHECK:       name "B1"
##  CHECK-NOT:   end_block
##  CHECK:         If
##  CHECK-NOT:     ParallelMove
##  CHECK:       name "B5"
##  CHECK-NOT:   end_block
##  CHECK:         InstanceFieldSet
#    We could check here that there is a parallel move, but it's only valid
#    for some architectures (for example x86), as other architectures may
#    not do move at all.
##  CHECK:       end_block
##  CHECK-NOT:     ParallelMove
.method public static test3Order2(ZIIIII)V
    .registers 14

    sget v0, LMain;->live1:I
    sget v1, LMain;->live2:I
    sget v2, LMain;->live3:I
    sget v3, LMain;->live0:I
    if-eqz p0, :cond_d

    sput v0, LMain;->live1:I
    goto :goto_37

    :cond_d
    sget-boolean v4, LMain;->y:Z
    if-eqz v4, :cond_14

    sput v0, LMain;->live1:I
    goto :goto_37

    :cond_14
    sget v4, LMain;->live4:I
    sget v5, LMain;->live5:I
    sget-object v6, LMain;->foo:LMain$Foo;
    add-int v7, v0, v5
    add-int/2addr v7, v4
    iput v7, v6, LMain$Foo;->field2:I
    sget-object v6, LMain;->foo:LMain$Foo;
    add-int v7, v1, v5
    add-int/2addr v7, v4
    iput v7, v6, LMain$Foo;->field3:I
    sget-object v6, LMain;->foo:LMain$Foo;
    add-int v7, v2, v5
    add-int/2addr v7, v4
    iput v7, v6, LMain$Foo;->field4:I
    sget-object v6, LMain;->foo:LMain$Foo;
    iput v4, v6, LMain$Foo;->field0:I
    sget-object v6, LMain;->foo:LMain$Foo;
    add-int v7, v5, v4
    iput v7, v6, LMain$Foo;->field1:I
    :goto_37

    add-int v4, v0, v1
    add-int/2addr v4, v2
    add-int/2addr v4, v3
    sput v4, LMain;->live1:I
    return-void
.end method
