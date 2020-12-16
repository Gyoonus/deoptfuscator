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


.class public abstract interface LIface;
.super Ljava/lang/Object;

# public interface Iface {
#     public default String getTwice() {
#         return getString() + getString();
#     }
#     public String getString();
# }

.method public getTwice()Ljava/lang/String;
.locals 2
    invoke-static {p0}, Ljava/util/Objects;->requireNonNull(Ljava/lang/Object;)Ljava/lang/Object;
    invoke-interface {p0}, LIface;->getString()Ljava/lang/String;
    move-result-object v0
    invoke-interface {p0}, LIface;->getString()Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method

.method public abstract getString()Ljava/lang/String;
.end method
