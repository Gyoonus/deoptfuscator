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

.class public Test8Derived
.super Test8Base

.method public <init>()V
   .limit stack 1
   .limit locals 1
   aload_0
   invokespecial Test8Base.<init>()V
   return
.end method

.method public foo()V
    .limit stack 2
    .limit locals 1
    getstatic java/lang/System/out Ljava/io/PrintStream;
    ldc "Test8Derived.foo()"
    invokevirtual java/io/PrintStream.println(Ljava/lang/String;)V
    return
.end method
