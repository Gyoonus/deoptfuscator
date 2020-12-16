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

.class public interface LDefaultInterface;
.super Ljava/lang/Object;

# public interface DefaultInterface {
#     public default void JniCallNonOverridenDefaultMethod() {
#         System.out.println("DefaultInterface.JniCallNonOverridenDefaultMethod");
#     }
#
#     public default void JniCallOverridenDefaultMethod() {
#         System.out.println("DefaultInterface.JniCallOverridenDefaultMethod");
#     }
#
#     public void JniCallOverridenAbstractMethod();
#
#     public default void JniCallConflictDefaultMethod() {
#         System.out.println("DefaultInterface.JniCallConflictDefaultMethod");
#     }
#
#     public default void JniCallSoftConflictMethod() {
#         System.out.println("DefaultInterface.JniCallSoftConflictMethod");
#     }
# }

.method public JniCallNonOverridenDefaultMethod()V
    .locals 2

    const-string v0, "DefaultInterface.JniCallNonOverridenDefaultMethod"
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method

.method public JniCallOverridenDefaultMethod()V
    .locals 2

    const-string v0, "DefaultInterface.JniCallOverridenDefaultMethod"
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method

.method public abstract JniCallOverridenAbstractMethod()V
.end method

.method public JniCallConflictDefaultMethod()V
    .locals 2

    const-string v0, "DefaultInterface.JniCallConflictDefaultMethod"
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method

.method public JniCallSoftConflictMethod()V
    .locals 2

    const-string v0, "DefaultInterface.JniCallSoftConflictMethod"
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v1,v0}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method
