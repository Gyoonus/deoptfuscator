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
.implements LIface;

# public class ConcreteClass implements Iface {
#     public String getString() {
#         return "STRING!!!";
#     }
#     public String callMethod() {
#         return this.getTwice();
#     }
# }

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public getString()Ljava/lang/String;
.registers 2
    const-string v0, "STRING!!!"
    return-object v0
.end method

.method public callMethod()Ljava/lang/String;
.registers 2
    invoke-virtual {p0}, LConcreteClass;->getTwice()Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method
