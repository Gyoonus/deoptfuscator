#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


.class public LConcreteClass;
.super Ljava/lang/Object;
.implements LSuperInterface;
.implements LConflictInterface;

# public class ConcreteClass implements SuperInterface, ConflictInterface {
#     public String runReal() {
#         return SuperInterface.super.runDefault();
#     }
#     public String runConflict() {
#         return ConflictInterface.super.runDefault();
#     }
#     public String runDefault() {
#         return "This is the wrong class to invoke";
#     }
# }

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public runConflict()Ljava/lang/String;
.registers 2
    # Do an invoke super on this class, to confuse runtime/compiler.
    invoke-super {p0}, LConflictInterface;->runDefault()Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method



.method public runReal()Ljava/lang/String;
.registers 2
    # Do an invoke super on this class, to confuse runtime/compiler.
    invoke-super {p0}, LSuperInterface;->runDefault()Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method

.method public runDefault()Ljava/lang/String;
.registers 2
    const-string v0, "This is the wrong class to invoke!"
    return-object v0
.end method
