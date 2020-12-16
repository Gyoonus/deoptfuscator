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

.class public LTestCase;

.super Ljava/lang/Object;

.method public static setByteArray([B)V
  .registers 3
  const/16 v0, 0x0
  const/16 v1, 0x0101
  aput-byte v1, p0, v0
  return-void
.end method

.method public static setByteStaticField()V
  .registers 1
  const/16 v0, 0x0101
  sput-byte v0, LTestCase;->staticByteField:B
  return-void
.end method

.method public static setByteInstanceField(LTestCase;)V
  .registers 2
  const/16 v0, 0x0101
  iput-byte v0, p0, LTestCase;->instanceByteField:B
  return-void
.end method

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public static setShortArray([S)V
  .registers 3
  const/16 v0, 0x0
  const v1, 0x10101
  aput-short v1, p0, v0
  return-void
.end method

.method public static setShortStaticField()V
  .registers 1
  const v0, 0x10101
  sput-short v0, LTestCase;->staticShortField:S
  return-void
.end method

.method public static setShortInstanceField(LTestCase;)V
  .registers 2
  const v0, 0x10101
  iput-short v0, p0, LTestCase;->instanceShortField:S
  return-void
.end method

.method public static setCharArray([C)V
  .registers 3
  const/16 v0, 0x0
  const v1, 0x10101
  aput-char v1, p0, v0
  return-void
.end method

.method public static setCharStaticField()V
  .registers 1
  const v0, 0x10101
  sput-char v0, LTestCase;->staticCharField:C
  return-void
.end method

.method public static setCharInstanceField(LTestCase;)V
  .registers 2
  const v0, 0x10101
  iput-char v0, p0, LTestCase;->instanceCharField:C
  return-void
.end method

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.field public static staticByteField:B
.field public instanceByteField:B
.field public static staticShortField:S
.field public instanceShortField:S
.field public static staticCharField:C
.field public instanceCharField:C
