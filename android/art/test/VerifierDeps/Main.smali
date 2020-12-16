# Copyright (C) 2016 The Android Open Source Project
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

.class public LMain;
.super LMyThreadSet;

.method public static ArgumentType_ResolvedClass(Ljava/lang/Thread;)V
  .registers 1
  return-void
.end method

.method public static ArgumentType_ResolvedReferenceArray([Ljava/lang/Thread;)V
  .registers 1
  return-void
.end method

.method public static ArgumentType_ResolvedPrimitiveArray([B)V
  .registers 1
  return-void
.end method

.method public static ArgumentType_UnresolvedClass(LUnresolvedClass;)V
  .registers 1
  return-void
.end method

.method public static ArgumentType_UnresolvedSuper(LMySetWithUnresolvedSuper;)V
  .registers 1
  return-void
.end method

.method public static ReturnType_Reference(Ljava/lang/IllegalStateException;)Ljava/lang/Throwable;
  .registers 1
  return-object p0
.end method

.method public static ReturnType_Array([Ljava/lang/IllegalStateException;)[Ljava/lang/Integer;
  .registers 1
  return-object p0
.end method

.method public static InvokeArgumentType(Ljava/text/SimpleDateFormat;Ljava/util/SimpleTimeZone;)V
  .registers 2
  invoke-virtual {p0, p1}, Ljava/text/SimpleDateFormat;->setTimeZone(Ljava/util/TimeZone;)V
  return-void
.end method

.method public static MergeTypes_RegisterLines(Z)Ljava/lang/Object;
  .registers 2
  if-eqz p0, :else

  new-instance v0, LMySocketTimeoutException;
  invoke-direct {v0}, LMySocketTimeoutException;-><init>()V
  goto :merge

  :else
  new-instance v0, Ljava/util/concurrent/TimeoutException;
  invoke-direct {v0}, Ljava/util/concurrent/TimeoutException;-><init>()V
  goto :merge

  :merge
  return-object v0
.end method

.method public static MergeTypes_IfInstanceOf(Ljava/net/SocketTimeoutException;)V
  .registers 2
  instance-of v0, p0, Ljava/util/concurrent/TimeoutException;
  if-eqz v0, :else
  return-void
  :else
  return-void
.end method

.method public static MergeTypes_Unresolved(ZZLUnresolvedClassA;)Ljava/lang/Object;
  .registers 5
  if-eqz p0, :else1

  move-object v0, p2
  goto :merge

  :else1
  if-eqz p1, :else2

  new-instance v0, Ljava/util/concurrent/TimeoutException;
  invoke-direct {v0}, Ljava/util/concurrent/TimeoutException;-><init>()V
  goto :merge

  :else2
  new-instance v0, Ljava/net/SocketTimeoutException;
  invoke-direct {v0}, Ljava/net/SocketTimeoutException;-><init>()V
  goto :merge

  :merge
  return-object v0
.end method

.method public static ConstClass_Resolved()V
  .registers 1
  const-class v0, Ljava/lang/IllegalStateException;
  return-void
.end method

.method public static ConstClass_Unresolved()V
  .registers 1
  const-class v0, LUnresolvedClass;
  return-void
.end method

.method public static CheckCast_Resolved(Ljava/lang/Object;)V
  .registers 1
  check-cast p0, Ljava/lang/IllegalStateException;
  return-void
.end method

.method public static CheckCast_Unresolved(Ljava/lang/Object;)V
  .registers 1
  check-cast p0, LUnresolvedClass;
  return-void
.end method

.method public static InstanceOf_Resolved(Ljava/lang/Object;)Z
  .registers 1
  instance-of p0, p0, Ljava/lang/IllegalStateException;
  return p0
.end method

.method public static InstanceOf_Unresolved(Ljava/lang/Object;)Z
  .registers 1
  instance-of p0, p0, LUnresolvedClass;
  return p0
.end method

.method public static NewInstance_Resolved()V
  .registers 1
  new-instance v0, Ljava/lang/IllegalStateException;
  return-void
.end method

.method public static NewInstance_Unresolved()V
  .registers 1
  new-instance v0, LUnresolvedClass;
  return-void
.end method

.method public static NewArray_Resolved()V
  .registers 1
  const/4 v0, 0x1
  new-array v0, v0, [Ljava/lang/IllegalStateException;
  return-void
.end method

.method public static NewArray_Unresolved()V
  .registers 2
  const/4 v0, 0x1
  new-array v0, v0, [LUnresolvedClass;
  return-void
.end method

.method public static Throw(Ljava/lang/IllegalStateException;)V
  .registers 2
  throw p0
.end method

.method public static MoveException_Resolved()Ljava/lang/Object;
  .registers 1
  :try_start
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  :try_end
  .catch Ljava/net/SocketTimeoutException; {:try_start .. :try_end} :catch_block
  .catch Ljava/io/InterruptedIOException; {:try_start .. :try_end} :catch_block
  .catch Ljava/util/zip/ZipException; {:try_start .. :try_end} :catch_block
  const/4 v0, 0x0
  return-object v0

  :catch_block
  move-exception v0
  return-object v0
.end method

.method public static MoveException_Unresolved()Ljava/lang/Object;
  .registers 1
  :try_start
  invoke-static {}, Ljava/lang/System;->nanoTime()J
  :try_end
  .catch LUnresolvedException; {:try_start .. :try_end} :catch_block
  const/4 v0, 0x0
  return-object v0

  :catch_block
  move-exception v0
  return-object v0
.end method

.method public static StaticField_Resolved_DeclaredInReferenced()V
  .registers 1
  sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInSuperclass1()V
  .registers 1
  sget v0, Ljava/util/SimpleTimeZone;->LONG:I
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInSuperclass2()V
  .registers 1
  sget v0, LMySimpleTimeZone;->SHORT:I
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInInterface1()V
  .registers 1
  # Case 1: DOMResult implements Result
  sget-object v0, Ljavax/xml/transform/dom/DOMResult;->PI_ENABLE_OUTPUT_ESCAPING:Ljava/lang/String;
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInInterface2()V
  .registers 1
  # Case 2: MyDOMResult extends DOMResult, DOMResult implements Result
  sget-object v0, LMyDOMResult;->PI_ENABLE_OUTPUT_ESCAPING:Ljava/lang/String;
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInInterface3()V
  .registers 1
  # Case 3: MyResult implements Result
  sget-object v0, LMyResult;->PI_ENABLE_OUTPUT_ESCAPING:Ljava/lang/String;
  return-void
.end method

.method public static StaticField_Resolved_DeclaredInInterface4()V
  .registers 1
  # Case 4: MyDocument implements Document, Document extends Node
  sget-short v0, LMyDocument;->ELEMENT_NODE:S
  return-void
.end method

.method public static StaticField_Unresolved_ReferrerInBoot()V
  .registers 1
  sget v0, Ljava/util/TimeZone;->x:I
  return-void
.end method

.method public static StaticField_Unresolved_ReferrerInDex()V
  .registers 1
  sget v0, LMyThreadSet;->x:I
  return-void
.end method

.method public static InstanceField_Resolved_DeclaredInReferenced(LMySocketTimeoutException;)V
  .registers 1
  iget v0, p0, Ljava/io/InterruptedIOException;->bytesTransferred:I
  return-void
.end method

.method public static InstanceField_Resolved_DeclaredInSuperclass1(LMySocketTimeoutException;)V
  .registers 1
  iget v0, p0, Ljava/net/SocketTimeoutException;->bytesTransferred:I
  return-void
.end method

.method public static InstanceField_Resolved_DeclaredInSuperclass2(LMySocketTimeoutException;)V
  .registers 1
  iget v0, p0, LMySocketTimeoutException;->bytesTransferred:I
  return-void
.end method

.method public static InstanceField_Unresolved_ReferrerInBoot(LMySocketTimeoutException;)V
  .registers 1
  iget v0, p0, Ljava/io/InterruptedIOException;->x:I
  return-void
.end method

.method public static InstanceField_Unresolved_ReferrerInDex(LMyThreadSet;)V
  .registers 1
  iget v0, p0, LMyThreadSet;->x:I
  return-void
.end method

.method public static InvokeStatic_Resolved_DeclaredInReferenced()V
  .registers 1
  const v0, 0x0
  invoke-static {v0}, Ljava/net/Socket;->setSocketImplFactory(Ljava/net/SocketImplFactory;)V
  return-void
.end method

.method public static InvokeStatic_Resolved_DeclaredInSuperclass1()V
  .registers 1
  const v0, 0x0
  invoke-static {v0}, Ljavax/net/ssl/SSLSocket;->setSocketImplFactory(Ljava/net/SocketImplFactory;)V
  return-void
.end method

.method public static InvokeStatic_Resolved_DeclaredInSuperclass2()V
  .registers 1
  const v0, 0x0
  invoke-static {v0}, LMySSLSocket;->setSocketImplFactory(Ljava/net/SocketImplFactory;)V
  return-void
.end method

.method public static InvokeStatic_DeclaredInInterface1()V
  .registers 1
  invoke-static {}, Ljava/util/Map$Entry;->comparingByKey()Ljava/util/Comparator;
  return-void
.end method

.method public static InvokeStatic_DeclaredInInterface2()V
  .registers 1
  # AbstractMap$SimpleEntry implements Map$Entry
  # INVOKE_STATIC does not resolve to methods in superinterfaces. This will
  # therefore result in an unresolved method.
  invoke-static {}, Ljava/util/AbstractMap$SimpleEntry;->comparingByKey()Ljava/util/Comparator;
  return-void
.end method

.method public static InvokeStatic_Unresolved1()V
  .registers 1
  invoke-static {}, Ljavax/net/ssl/SSLSocket;->x()V
  return-void
.end method

.method public static InvokeStatic_Unresolved2()V
  .registers 1
  invoke-static {}, LMySSLSocket;->x()V
  return-void
.end method

.method public static InvokeDirect_Resolved_DeclaredInReferenced()V
  .registers 1
  new-instance v0, Ljava/net/Socket;
  invoke-direct {v0}, Ljava/net/Socket;-><init>()V
  return-void
.end method

.method public static InvokeDirect_Resolved_DeclaredInSuperclass1(LMySSLSocket;)V
  .registers 1
  invoke-direct {p0}, Ljavax/net/ssl/SSLSocket;->checkOldImpl()V
  return-void
.end method

.method public static InvokeDirect_Resolved_DeclaredInSuperclass2(LMySSLSocket;)V
  .registers 1
  invoke-direct {p0}, LMySSLSocket;->checkOldImpl()V
  return-void
.end method

.method public static InvokeDirect_Unresolved1(LMySSLSocket;)V
  .registers 1
  invoke-direct {p0}, Ljavax/net/ssl/SSLSocket;->x()V
  return-void
.end method

.method public static InvokeDirect_Unresolved2(LMySSLSocket;)V
  .registers 1
  invoke-direct {p0}, LMySSLSocket;->x()V
  return-void
.end method

.method public static InvokeVirtual_Resolved_DeclaredInReferenced(LMySocketTimeoutException;)V
  .registers 1
  invoke-virtual {p0}, Ljava/lang/Throwable;->getMessage()Ljava/lang/String;
  return-void
.end method

.method public static InvokeVirtual_Resolved_DeclaredInSuperclass1(LMySocketTimeoutException;)V
  .registers 1
  invoke-virtual {p0}, Ljava/io/InterruptedIOException;->getMessage()Ljava/lang/String;
  return-void
.end method

.method public static InvokeVirtual_Resolved_DeclaredInSuperclass2(LMySocketTimeoutException;)V
  .registers 1
  invoke-virtual {p0}, LMySocketTimeoutException;->getMessage()Ljava/lang/String;
  return-void
.end method

.method public static InvokeVirtual_Resolved_DeclaredInSuperinterface(LMyThreadSet;)V
  .registers 1
  invoke-virtual {p0}, LMyThreadSet;->size()I
  return-void
.end method

.method public static InvokeVirtual_Unresolved1(LMySocketTimeoutException;)V
  .registers 1
  invoke-virtual {p0}, Ljava/io/InterruptedIOException;->x()V
  return-void
.end method

.method public static InvokeVirtual_Unresolved2(LMySocketTimeoutException;)V
  .registers 1
  invoke-virtual {p0}, LMySocketTimeoutException;->x()V
  return-void
.end method

.method public static InvokeInterface_Resolved_DeclaredInReferenced(LMyThread;)V
  .registers 1
  invoke-interface {p0}, Ljava/lang/Runnable;->run()V
  return-void
.end method

.method public static InvokeInterface_Resolved_DeclaredInSuperclass(LMyThread;)V
  .registers 1
  # Method join() is declared in the superclass of MyThread. As such, it should
  # be called with invoke-virtual. However, the lookup type does not depend
  # on the invoke type, so it shall be resolved here anyway.
  # TODO: Maybe we should not record dependency if the invoke type does not match the lookup type.
  invoke-interface {p0}, LMyThread;->join()V
  return-void
.end method

.method public static InvokeInterface_Resolved_DeclaredInSuperinterface1(LMyThreadSet;)V
  .registers 1
  # Verification will fail because the referring class is not an interface.
  # However, the lookup type does not depend on the invoke type, so it shall be resolved here anyway.
  # TODO: Maybe we should not record dependency if the invoke type does not match the lookup type.
  invoke-interface {p0}, LMyThreadSet;->run()V
  return-void
.end method

.method public static InvokeInterface_Resolved_DeclaredInSuperinterface2(LMyThreadSet;)V
  .registers 1
  # Verification will fail because the referring class is not an interface.
  invoke-interface {p0}, LMyThreadSet;->isEmpty()Z
  return-void
.end method

.method public static InvokeInterface_Unresolved1(LMyThread;)V
  .registers 1
  invoke-interface {p0}, Ljava/lang/Runnable;->x()V
  return-void
.end method

.method public static InvokeInterface_Unresolved2(LMyThread;)V
  .registers 1
  invoke-interface {p0}, LMyThreadSet;->x()V
  return-void
.end method

.method public static InvokeSuper_ThisAssignable(Ljava/lang/Thread;)V
  .registers 1
  invoke-super {p0}, Ljava/lang/Runnable;->run()V
  return-void
.end method

.method public static InvokeSuper_ThisNotAssignable(Ljava/lang/Integer;)V
  .registers 1
  invoke-super {p0}, Ljava/lang/Integer;->intValue()I
  return-void
.end method
