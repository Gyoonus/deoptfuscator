# Copyright (C) 2016 The Android Open Source Project
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

.class public LSubClassUsingInaccessibleMethod;

.super Lother/PublicClass;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Lother/PublicClass;-><init>()V
    return-void
.end method

# Regression test for compiler DCHECK() failure (bogus check) when referencing
# a package-private method from an indirectly inherited package-private class,
# using this very class as the declaring class in the MethodId, bug: 28771056.
.method public test()I
    .registers 2
    invoke-virtual {p0}, LSubClassUsingInaccessibleMethod;->otherProtectedClassPackageIntInstanceMethod()I
    move-result v0
    return v0
.end method
