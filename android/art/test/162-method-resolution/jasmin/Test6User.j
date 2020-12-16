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

.class public Test6User
.super java/lang/Object

.method public static test()V
    .limit stack 3
    .limit locals 0
    getstatic java/lang/System/out Ljava/io/PrintStream;
    new Test6Derived
    dup
    invokespecial Test6Derived.<init>()V
    invokeinterface Test6Interface.toString()Ljava/lang/String; 1
    invokestatic Main.normalizeToString(Ljava/lang/String;)Ljava/lang/String;
    invokevirtual java/io/PrintStream.println(Ljava/lang/String;)V
    return
.end method
