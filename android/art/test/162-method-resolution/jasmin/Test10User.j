; Copyright (C) 2017 The Android Open Source Project
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;      http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

.class public Test10User
.super java/lang/Object

.method public static test()V
    .limit stack 3
    .limit locals 3
    new Test10Base
    dup
    invokespecial Test10Base.<init>()V
    invokestatic  Test10User.doInvoke(LTest10Interface;)V
    return
.end method

.method public static doInvoke(LTest10Interface;)V
    .limit stack 3
    .limit locals 3
    aload_0
    invokeinterface Test10Interface.clone()Ljava.lang.Object; 1
    pop
    return
.end method

