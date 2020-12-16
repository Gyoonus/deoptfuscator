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

##  CHECK-START: double Smali.calcCircleAreaOrCircumference(double, boolean) load_store_elimination (before)
##  CHECK: NewInstance
##  CHECK: InstanceFieldSet
##  CHECK: ConstructorFence
##  CHECK: InstanceFieldGet

##  CHECK-START: double Smali.calcCircleAreaOrCircumference(double, boolean) load_store_elimination (after)
##  CHECK: NewInstance
##  CHECK-NOT: ConstructorFence

#   The object allocation will not be eliminated by LSE because of aliased stores.
#   However the object is still a singleton, so it never escapes the current thread.
#   There should not be a constructor fence here after LSE.

.method public static calcCircleAreaOrCircumference(DZ)D
    .registers 7

    # CalcCircleAreaOrCircumference calc =
    #   new CalcCircleAreaOrCircumference(
    #       area_or_circumference ? CalcCircleAreaOrCircumference.TYPE_AREA :
    #       CalcCircleAreaOrCircumference.TYPE_CIRCUMFERENCE);

    # if (area_or_circumference) {
    #   // Area
    #   calc.value = Math.PI * Math.PI * radius;
    # } else {
    #   // Circumference
    #   calc.value = 2 * Math.PI * radius;
    # }

    # Please note that D8 would merge the iput togother which looks like :

    # if (area_or_circumference) {
    #   // Area
    #   tmp = Math.PI * Math.PI * radius;
    # } else {
    #   // Circumference
    #   tmp = 2 * Math.PI * radius;
    # }
    # calc.value = tmp;

    # which makes the LSE valid and defeat the purpose of this test.

    new-instance v0, LCalcCircleAreaOrCircumference;

    if-eqz p2, :cond_15

    const/4 v1, 0x0

    :goto_5
    invoke-direct {v0, v1}, LCalcCircleAreaOrCircumference;-><init>(I)V

    if-eqz p2, :cond_17

    const-wide v2, 0x4023bd3cc9be45deL    # 9.869604401089358

    mul-double/2addr v2, p0

    iput-wide v2, v0, LCalcCircleAreaOrCircumference;->value:D

    :goto_12
    iget-wide v2, v0, LCalcCircleAreaOrCircumference;->value:D

    return-wide v2

    :cond_15
    const/4 v1, 0x1

    goto :goto_5

    :cond_17
    const-wide v2, 0x401921fb54442d18L    # 6.283185307179586

    mul-double/2addr v2, p0

    iput-wide v2, v0, LCalcCircleAreaOrCircumference;->value:D

    goto :goto_12
.end method

