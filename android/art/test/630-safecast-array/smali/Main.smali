# Copyright 2016 The Android Open Source Project
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

.class LMain;
.super Ljava/lang/Object;

.method public static main([Ljava/lang/String;)V
.registers 1
    return-void
.end method

.method public static testPrimitiveDestination([Ljava/lang/String;)V
.registers 1
    check-cast p0, [B
    return-void
.end method

.method public static testPrimitiveSource([B)V
.registers 1
    check-cast p0, [Ljava/lang/String;
    return-void
.end method
