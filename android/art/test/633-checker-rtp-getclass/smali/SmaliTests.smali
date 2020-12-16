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

.class public LSmaliTests;
.super Ljava/lang/Object;

# Checker test to make sure the only inlined instruction is SubMain.bar.

## CHECK-START: int SmaliTests.$opt$noinline$foo(Main) inliner (after)
## CHECK-DAG:                InvokeVirtual method_name:Main.foo
## CHECK-DAG: <<Const:i\d+>> IntConstant 3
## CHECK:                    begin_block
## CHECK:                    BoundType klass:SubMain
## CHECK:                    Return [<<Const>>]
## CHECK-NOT:                begin_block
## CHECK:                    end_block
.method public static $opt$noinline$foo(LMain;)I
    .registers 3
    .param p0, "o"    # LMain;
    .prologue

    # if (doThrow) { throw new Error(); }
    sget-boolean v0, LMain;->doThrow:Z
    if-eqz v0, :doThrow_false
    new-instance v0, Ljava/lang/Error;
    invoke-direct {v0}, Ljava/lang/Error;-><init>()V
    throw v0

  :doThrow_false
    # if (o.getClass() == Main.class || o.getClass() != SubMain.class)
    invoke-virtual {p0}, LMain;->getClass()Ljava/lang/Class;
    move-result-object v0
    const-class v1, LMain;
    if-eq v0, v1, :class_is_Main_and_not_SubMain

    invoke-virtual {p0}, LMain;->getClass()Ljava/lang/Class;
    move-result-object v0
    const-class v1, LSubMain;
    if-eq v0, v1, :else

  :class_is_Main_and_not_SubMain
    # return o.foo()
    invoke-virtual {p0}, LMain;->foo()I
    move-result v0
    return v0

  :else
    # return o.bar()
    invoke-virtual {p0}, LMain;->bar()I
    move-result v0
    return v0
.end method


