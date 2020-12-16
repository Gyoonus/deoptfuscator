#
# Copyright (C) 2014 The Android Open Source Project
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
#

.class public abstract LNonInf;
.super Ljava/lang/Object;
.source "NonInf.java"


# static fields
.field static staticField:I


# instance fields
.field final finalField:I

.field private privateField:I

.field protected protectedField:I

.field public publicField:I

.field transient transientField:I

.field volatile volatileField:I


# direct methods
.method public constructor <init>()V
    .registers 2

    .prologue
    .line 11
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 12
    const/4 v0, 0x0

    iput v0, p0, LNonInf;->publicField:I

    .line 13
    const/4 v0, 0x1

    iput v0, p0, LNonInf;->privateField:I

    .line 14
    const/4 v0, 0x2

    iput v0, p0, LNonInf;->protectedField:I

    .line 15
    const/4 v0, 0x3

    sput v0, LNonInf;->staticField:I

    .line 16
    const/4 v0, 0x4

    iput v0, p0, LNonInf;->transientField:I

    .line 17
    const/4 v0, 0x5

    iput v0, p0, LNonInf;->volatileField:I

    .line 18
    const/4 v0, 0x6

    iput v0, p0, LNonInf;->finalField:I

    .line 19
    return-void
.end method

.method private privateMethod()I
    .registers 2

    .prologue
    .line 24
    const/4 v0, 0x0

    return v0
.end method

.method public static staticMethod()I
    .registers 1

    .prologue
    .line 42
    const/4 v0, 0x0

    return v0
.end method


# virtual methods
.method public abstract abstractMethod()I
.end method

.method public final finalMethod()I
    .registers 2

    .prologue
    .line 54
    const/4 v0, 0x0

    return v0
.end method

.method public native nativeMethod()V
.end method

.method protected protectedMethod()I
    .registers 2

    .prologue
    .line 28
    const/4 v0, 0x0

    return v0
.end method

.method public publicMethod()I
    .registers 2

    .prologue
    .line 32
    const/4 v0, 0x0

    return v0
.end method

.method public strictfp strictfpMethod()D
    .registers 3

    .prologue
    .line 46
    const-wide/16 v0, 0x0

    return-wide v0
.end method

.method public declared-synchronized synchronizedMethod()I
    .registers 2

    .prologue
    monitor-enter p0

    .line 38
    const/4 v0, 0x0

    monitor-exit p0

    return v0
.end method

.method public varargs varargsMethod([Ljava/lang/Object;)I
    .registers 3

    .prologue
    .line 50
    const/4 v0, 0x0

    return v0
.end method
