# /*
#  * Copyright 2016 The Android Open Source Project
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

.class public LConcreteClass;
.super Ljava/lang/Object;
.implements LDefaultInterface;
.implements LConflictInterface;
.implements LAbstractInterface;

# public class ConcreteClass implements DefaultInterface, ConflictInterface, AbstractInterface {
#     public void JniCallOverridenAbstractMethod() {
#         System.out.println("ConcreteClass.JniCallOverridenAbstractMethod");
#     }
#
#     public void JniCallOverridenDefaultMethod() {
#         System.out.println("ConcreteClass.JniCallOverridenDefaultMethod");
#     }
#
#     public void JniCallOverridenDefaultMethodWithSuper() {
#         System.out.println("ConcreteClass.JniCallOverridenDefaultMethodWithSuper");
#         DefaultInterface.super.JniCallOverridenDefaultMethod();
#     }
# }

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public JniCallOverridenAbstractMethod()V
    .locals 2

    const-string v0, "ConcreteClass.JniCallOverridenAbstractMethod"
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method

.method public JniCallOverridenDefaultMethod()V
    .locals 2

    const-string v0, "ConcreteClass.JniCallOverridenDefaultMethod"
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method

.method public JniCallOverridenDefaultMethodWithSuper()V
    .locals 2

    const-string v0, "ConcreteClass.JniCallOverridenDefaultMethodWithSuper"
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    invoke-super {p0}, LDefaultInterface;->JniCallOverridenDefaultMethod()V

    return-void
.end method
