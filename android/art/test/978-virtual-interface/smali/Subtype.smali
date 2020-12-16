# /*
#  * Copyright (C) 2015 The Android Open Source Project
#  *
#  * Licensed under the Apache License, Version 2.0 (the "License");
#  * you may not use this file except in compliance with the License.
#  * You may obtain a copy of the License at
#  *
#  *      http://www.apache.org/licenses/LICENSE-2.0
#  *
#  * Unless required by applicable law or agreed to in writing, software
#  * distributed under the License is distributed on an "AS IS" BASIS,
#  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  * See the License for the specific language governing permissions and
#  * limitations under the License.
#  */
#
#  public class Subtype extends pkg.Target implements Iface{
#    public void callPackage() {
#      // Fake into a virtual call.
#      // ((Iface)this).fakeMethod_Target();
#    }
#  }

.class public LSubtype;

.super Lpkg/Target;

.implements LIface;

.method public constructor <init>()V
    .locals 0
    invoke-direct {p0}, Lpkg/Target;-><init>()V
    return-void
.end method

.method public callPackage()V
    .locals 0
    invoke-virtual {p0}, LIface;->fakeMethod_Target()V
    return-void
.end method
