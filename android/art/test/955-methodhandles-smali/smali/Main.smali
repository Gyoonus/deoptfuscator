# Copyright 2016 The Android Open Source Project
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

.class LMain;
.super Ljava/lang/Object;

# MethodHandle Main.getHandleForVirtual(Class<?> defc, String name, MethodType type);
#
# Returns a handle to a virtual method on |defc| named name with type |type| using
# the public lookup object.
.method public static getHandleForVirtual(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;
.registers 5

    # Get a reference to the public lookup object (MethodHandles.publicLookup()).
    invoke-static {}, Ljava/lang/invoke/MethodHandles;->publicLookup()Ljava/lang/invoke/MethodHandles$Lookup;
    move-result-object v0

    # Call Lookup.findVirtual(defc, name, type);
    invoke-virtual {v0, p0, p1, p2}, Ljava/lang/invoke/MethodHandles$Lookup;->findVirtual(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;
    move-result-object v1
    return-object v1
.end method

# MethodHandle Main.getHandleForStatic(Class<?> defc, String name, MethodType type);
#
# Returns a handle to a static method on |defc| named name with type |type| using
# the public lookup object.
.method public static getHandleForStatic(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;
.registers 5

    # Get a reference to the public lookup object (MethodHandles.publicLookup()).
    invoke-static {}, Ljava/lang/invoke/MethodHandles;->publicLookup()Ljava/lang/invoke/MethodHandles$Lookup;
    move-result-object v0

    # Call Lookup.findStatic(defc, name, type);
    invoke-virtual {v0, p0, p1, p2}, Ljava/lang/invoke/MethodHandles$Lookup;->findStatic(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;
    move-result-object v1
    return-object v1
.end method

# Returns a method handle to String java.lang.String.concat(String);
.method public static getStringConcatHandle()Ljava/lang/invoke/MethodHandle;
.registers 3
    const-string v0, "concat"
    invoke-virtual {v0}, Ljava/lang/Object;->getClass()Ljava/lang/Class;
    move-result-object v1

    # Call MethodType.methodType(rtype=String.class, ptype[0] = String.class)
    invoke-static {v1, v1}, Ljava/lang/invoke/MethodType;->methodType(Ljava/lang/Class;Ljava/lang/Class;)Ljava/lang/invoke/MethodType;
    move-result-object v2

    # Call Main.getHandleForVirtual(String.class, "concat", methodType);
    invoke-static {v1, v0, v2}, LMain;->getHandleForVirtual(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;
    move-result-object v0
    return-object v0
.end method

# Returns a method handle to boolean java.lang.Long.compareTo(java.lang.Long other).
.method public static getLongCompareToHandle()Ljava/lang/invoke/MethodHandle;
.registers 4
    new-instance v0, Ljava/lang/Long;
    const-wide v1, 0
    invoke-direct {v0, v1, v2}, Ljava/lang/Long;-><init>(J)V
    invoke-virtual {v0}, Ljava/lang/Object;->getClass()Ljava/lang/Class;
    move-result-object v0

    # set v0 to Integer.TYPE aka. int.class
    sget-object v1, Ljava/lang/Integer;->TYPE:Ljava/lang/Class;

    # Call MethodType.methodType(rtype=int.class, ptype[0] = Long.class)
    invoke-static {v1, v0}, Ljava/lang/invoke/MethodType;->methodType(Ljava/lang/Class;Ljava/lang/Class;)Ljava/lang/invoke/MethodType;
    move-result-object v2

    const-string v3, "compareTo"
    # Call Main.getHandleForVirtual(Long.class, "compareTo", methodType);
    invoke-static {v0, v3, v2}, LMain;->getHandleForVirtual(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;
    move-result-object v0
    return-object v0
.end method

# Returns a method handle to static String java.lang.String.valueOf(Object);
.method public static getStringValueOfObjectHandle()Ljava/lang/invoke/MethodHandle;
.registers 4
    # set v0 to java.lang.Object.class
    new-instance v0, Ljava/lang/Object;
    invoke-direct {v0}, Ljava/lang/Object;-><init>()V
    invoke-virtual {v0}, Ljava/lang/Object;->getClass()Ljava/lang/Class;
    move-result-object v0

    # set v1 to the name of the method ("valueOf") and v2 to java.lang.String.class;
    const-string v1, "valueOf"
    invoke-virtual {v1}, Ljava/lang/Object;->getClass()Ljava/lang/Class;
    move-result-object v2

    # Call MethodType.methodType(rtype=String.class, ptype[0]=Object.class)
    invoke-static {v2, v0}, Ljava/lang/invoke/MethodType;->methodType(Ljava/lang/Class;Ljava/lang/Class;)Ljava/lang/invoke/MethodType;
    move-result-object v3

    # Call Main.getHandleForStatic(String.class, "valueOf", methodType);
    invoke-static {v2, v1, v3}, LMain;->getHandleForStatic(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;
    move-result-object v0
    return-object v0
.end method

# Returns a method handle to static String java.lang.String.valueOf(String);
.method public static getStringValueOfLongHandle()Ljava/lang/invoke/MethodHandle;
.registers 4
    # set v0 to Long.TYPE aka. long.class
    sget-object v0, Ljava/lang/Long;->TYPE:Ljava/lang/Class;

    # set v1 to the name of the method ("valueOf") and v2 to java.lang.String.class;
    const-string v1, "valueOf"
    invoke-virtual {v1}, Ljava/lang/Object;->getClass()Ljava/lang/Class;
    move-result-object v2

    # Call MethodType.methodType(rtype=String.class, ptype[0]=Long.class)
    invoke-static {v2, v0}, Ljava/lang/invoke/MethodType;->methodType(Ljava/lang/Class;Ljava/lang/Class;)Ljava/lang/invoke/MethodType;
    move-result-object v3

    # Call Main.getHandleForStatic(String.class, "valueOf", methodType);
    invoke-static {v2, v1, v3}, LMain;->getHandleForStatic(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;
    move-result-object v0
    return-object v0
.end method

.method public static main([Ljava/lang/String;)V
.registers 5

    # Test case 1: Exercise String.concat(String, String) which is a virtual method.
    invoke-static {}, LMain;->getStringConcatHandle()Ljava/lang/invoke/MethodHandle;
    move-result-object v0
    const-string v1, "[String1]"
    const-string v2, "+[String2]"
    invoke-polymorphic {v0, v1, v2}, Ljava/lang/invoke/MethodHandle;->invokeExact([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v3
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v4, v3}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    # Test case 2: Exercise String.valueOf(Object);
    invoke-static {}, LMain;->getStringValueOfObjectHandle()Ljava/lang/invoke/MethodHandle;
    move-result-object v0
    const-string v1, "[String1]"
    invoke-polymorphic {v0, v1}, Ljava/lang/invoke/MethodHandle;->invokeExact([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/Object;)Ljava/lang/String;
    move-result-object v3
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v4, v3}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    # Test case 3: Exercise String.concat(String, String) with an inexact invoke.
    # Note that the callsite type here is String type(Object, Object); so the runtime
    # will generate dynamic type checks for the input arguments.
    invoke-static {}, LMain;->getStringConcatHandle()Ljava/lang/invoke/MethodHandle;
    move-result-object v0
    const-string v1, "[String1]"
    const-string v2, "+[String2]"
    invoke-polymorphic {v0, v1, v2}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/String;
    move-result-object v3
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v4, v3}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    # Test case 4: Exercise String.valueOf(long);
    #
    # We exercise it with various types of unboxing / widening conversions
    invoke-static {}, LMain;->getStringValueOfLongHandle()Ljava/lang/invoke/MethodHandle;
    move-result-object v0

    # First use a long, this is an invokeExact because the callsite type matches
    # the function type precisely.
    const-wide v1, 42
    invoke-polymorphic {v0, v1, v2}, Ljava/lang/invoke/MethodHandle;->invokeExact([Ljava/lang/Object;)Ljava/lang/Object;, (J)Ljava/lang/String;
    move-result-object v3
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v4, v3}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    # Then use an int, should perform a widening conversion.
    const v1, 40
    invoke-polymorphic {v0, v1}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (I)Ljava/lang/String;
    move-result-object v3
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v4, v3}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    # Then use a java/lang/Long; - should perform an unboxing conversion.
    new-instance v1, Ljava/lang/Long;
    const-wide v2, 43
    invoke-direct {v1, v2, v3}, Ljava/lang/Long;-><init>(J)V
    invoke-polymorphic {v0, v1}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/Long;)Ljava/lang/String;
    move-result-object v3
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v4, v3}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    # Then use a java/lang/Integer; - should perform an unboxing in addition to a widening conversion.
    new-instance v1, Ljava/lang/Integer;
    const v2, 44
    invoke-direct {v1, v2}, Ljava/lang/Integer;-><init>(I)V
    invoke-polymorphic {v0, v1}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/Integer;)Ljava/lang/String;
    move-result-object v3
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v4, v3}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    # Test case 5: Exercise int Long.compareTo(Long)
    invoke-static {}, LMain;->getLongCompareToHandle()Ljava/lang/invoke/MethodHandle;
    move-result-object v0
    new-instance v1, Ljava/lang/Long;
    const-wide v2, 43
    invoke-direct {v1, v2, v3}, Ljava/lang/Long;-><init>(J)V

    # At this point, v0 is our MethodHandle and v1 is the instance we're going to call compareTo on.

    # Call compareTo(Long) - this is invokeExact semantics.
    invoke-polymorphic {v0, v1, v1}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/Long;Ljava/lang/Long;)I
    move-result v3
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v4, v3}, Ljava/io/PrintStream;->println(I)V

    # Call compareTo(long) - this is an implicit box.
    const-wide v2, 44
    invoke-polymorphic {v0, v1, v2, v3}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/Long;J)I
    move-result v3
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v4, v3}, Ljava/io/PrintStream;->println(I)V

    # Call compareTo(int) - this is an implicit box.
# This throws WrongMethodTypeException as it's a two step conversion int->long->Long or int->Integer->Long.
#    const v2, 40
#    invoke-polymorphic {v0, v1, v2}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/Long;I)I
#    move-result v3
#    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
#    invoke-virtual {v4, v3}, Ljava/io/PrintStream;->print(I)V

    return-void
.end method
