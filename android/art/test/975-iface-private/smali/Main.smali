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
# class Main implements Iface {
#   public static void main(String[] args) {
#     Main m = new Main();
#     sayHiMain(m);
#     sayHiIface(m);
#   }
#   public static void sayHiMain(Main m) {
#     System.out.println("Saying hi from class");
#     m.sayHi();
#   }
#   public static void sayHiIface(Iface m) {
#     System.out.println("Saying hi from interface");
#     m.sayHi();
#   }
# }
.class public LMain;
.super Ljava/lang/Object;
.implements LIface;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public static main([Ljava/lang/String;)V
    .locals 2
    new-instance v0, LMain;
    invoke-direct {v0}, LMain;-><init>()V

    invoke-static {v0}, LMain;->sayHiMain(LMain;)V
    invoke-static {v0}, LMain;->sayHiIface(LIface;)V

    return-void
.end method

.method public static sayHiMain(LMain;)V
    .locals 2
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v1, "Saying hi from class"
    invoke-virtual {v0, v1}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    invoke-virtual {p0}, LMain;->sayHi()V
    return-void
.end method

.method public static sayHiIface(LIface;)V
    .locals 2
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v1, "Saying hi from interface"
    invoke-virtual {v0, v1}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    invoke-interface {p0}, LIface;->sayHi()V
    return-void
.end method
