.class public LB22881413;
.super Ljava/lang/Object;

# A couple of fields to allow "loading" resolved and unresolved types. Use non-final classes to
# avoid automatically getting precise reference types.
.field private static res1:Ljava/lang/Number;
.field private static res2:Ljava/lang/ClassLoader;
.field private static res3:Ljava/lang/Package;
.field private static res4:Ljava/lang/RuntimeException;
.field private static res5:Ljava/lang/Exception;
.field private static res6:Ljava/util/ArrayList;
.field private static res7:Ljava/util/LinkedList;
.field private static res8:Ljava/lang/Thread;
.field private static res9:Ljava/lang/ThreadGroup;
.field private static res10:Ljava/lang/Runtime;

.field private static unres1:La/b/c/d1;
.field private static unres2:La/b/c/d2;
.field private static unres3:La/b/c/d3;
.field private static unres4:La/b/c/d4;
.field private static unres5:La/b/c/d5;
.field private static unres6:La/b/c/d6;
.field private static unres7:La/b/c/d7;
.field private static unres8:La/b/c/d8;
.field private static unres9:La/b/c/d9;
.field private static unres10:La/b/c/d10;

.field private static unresBase0:La/b/c/dBase0;
.field private static unresBase1:La/b/c/dBase1;
.field private static unresBase2:La/b/c/dBase2;
.field private static unresBase3:La/b/c/dBase3;
.field private static unresBase4:La/b/c/dBase4;
.field private static unresBase5:La/b/c/dBase5;
.field private static unresBase6:La/b/c/dBase6;
.field private static unresBase7:La/b/c/dBase7;
.field private static unresBase8:La/b/c/dBase8;

# Empty, ignore this. We want to see if the other method can be verified in a reasonable amount of
# time.
.method public static run()V
.registers 2
       return-void
.end method

.method public static foo(IZZ) V
.registers 11
       # v8 = int, v9 = boolean, v10 = boolean

       sget-object v0, LB22881413;->unresBase0:La/b/c/dBase0;

# Test an UnresolvedUninitializedReference type.
       new-instance v0, La/b/c/dBaseInit;

       const v1, 0
       const v2, 0

# We're trying to create something like this (with more loops to amplify things).
#
# v0 = Unresolved1
# while (something) {
#
#   [Repeatedly]
#   if (cond) {
#     v0 = ResolvedX;
#   } else {
#     v0 = UnresolvedX;
#   }
#
#   v0 = Unresolved2
# };
#
# Important points:
#   1) Use a while, so that the end of the loop is a goto. That way, the merging of outer-loop
#      unresolved classes is postponed.
#   2) Put the else cases after all if cases. That way there are backward gotos that will lead
#      to stabilization loops in the body.
#

:Loop1

       const v6, 0
       add-int/lit16 v8, v8, -1
       if-ge v8, v6, :Loop1End

:Loop2

       const v6, 0
       add-int/lit16 v8, v8, -1
       if-ge v8, v6, :Loop2End

:Loop3

       const v6, 0
       add-int/lit16 v8, v8, -1
       if-ge v8, v6, :Loop3End

:Loop4

       const v6, 0
       add-int/lit16 v8, v8, -1
       if-ge v8, v6, :Loop4End

:Loop5

       const v6, 0
       add-int/lit16 v8, v8, -1
       if-ge v8, v6, :Loop5End

:Loop6

       const v6, 0
       add-int/lit16 v8, v8, -1
       if-ge v8, v6, :Loop6End

:Loop7

       const v6, 0
       add-int/lit16 v8, v8, -1
       if-ge v8, v6, :Loop7End

:Loop8

       const v6, 0
       add-int/lit16 v8, v8, -1
       if-ge v8, v6, :Loop8End

# Prototype:
#
#       if-eqz v9, :ElseX
#       sget-object v0, LB22881413;->res1:Ljava/lang/Number;
#:JoinX
#
# And somewhere at the end
#
#:ElseX
#       sget-object v0, LB22881413;->unresX:La/b/c/dX;
#       goto :JoinX
#
#

       if-eqz v10, :Join1
       if-eqz v9, :Else1
       sget-object v0, LB22881413;->res1:Ljava/lang/Number;
:Join1


       if-eqz v10, :Join2
       if-eqz v9, :Else2
       sget-object v0, LB22881413;->res2:Ljava/lang/ClassLoader;
:Join2


       if-eqz v10, :Join3
       if-eqz v9, :Else3
       sget-object v0, LB22881413;->res3:Ljava/lang/Package;
:Join3


       if-eqz v10, :Join4
       if-eqz v9, :Else4
       sget-object v0, LB22881413;->res4:Ljava/lang/RuntimeException;
:Join4


       if-eqz v10, :Join5
       if-eqz v9, :Else5
       sget-object v0, LB22881413;->res5:Ljava/lang/Exception;
:Join5


       if-eqz v10, :Join6
       if-eqz v9, :Else6
       sget-object v0, LB22881413;->res6:Ljava/util/ArrayList;
:Join6


       if-eqz v10, :Join7
       if-eqz v9, :Else7
       sget-object v0, LB22881413;->res7:Ljava/util/LinkedList;
:Join7


       if-eqz v10, :Join8
       if-eqz v9, :Else8
       sget-object v0, LB22881413;->res8:Ljava/lang/Thread;
:Join8


       if-eqz v10, :Join9
       if-eqz v9, :Else9
       sget-object v0, LB22881413;->res9:Ljava/lang/ThreadGroup;
:Join9


       if-eqz v10, :Join10
       if-eqz v9, :Else10
       sget-object v0, LB22881413;->res10:Ljava/lang/Runtime;
:Join10


       goto :InnerMostLoopEnd

:Else1
       sget-object v0, LB22881413;->unres1:La/b/c/d1;
       goto :Join1

:Else2
       sget-object v0, LB22881413;->unres2:La/b/c/d2;
       goto :Join2

:Else3
       sget-object v0, LB22881413;->unres3:La/b/c/d3;
       goto :Join3

:Else4
       sget-object v0, LB22881413;->unres4:La/b/c/d4;
       goto :Join4

:Else5
       sget-object v0, LB22881413;->unres5:La/b/c/d5;
       goto :Join5

:Else6
       sget-object v0, LB22881413;->unres6:La/b/c/d6;
       goto :Join6

:Else7
       sget-object v0, LB22881413;->unres7:La/b/c/d7;
       goto :Join7

:Else8
       sget-object v0, LB22881413;->unres8:La/b/c/d8;
       goto :Join8

:Else9
       sget-object v0, LB22881413;->unres9:La/b/c/d9;
       goto :Join9

:Else10
       sget-object v0, LB22881413;->unres10:La/b/c/d10;
       goto :Join10

:InnerMostLoopEnd

       # Loop 8 end of body.
       sget-object v0, LB22881413;->unresBase8:La/b/c/dBase8;
       goto :Loop8

:Loop8End

       # Loop 7 end of body.
       sget-object v0, LB22881413;->unresBase7:La/b/c/dBase7;
       goto :Loop7

:Loop7End

       # Loop 6 end of body.
       sget-object v0, LB22881413;->unresBase6:La/b/c/dBase6;
       goto :Loop6

:Loop6End

       # Loop 5 end of body
       sget-object v0, LB22881413;->unresBase5:La/b/c/dBase5;
       goto :Loop5

:Loop5End

       # Loop 4 end of body
       sget-object v0, LB22881413;->unresBase4:La/b/c/dBase4;
       goto :Loop4

:Loop4End

       # Loop 3 end of body
       sget-object v0, LB22881413;->unresBase3:La/b/c/dBase3;
       goto :Loop3

:Loop3End

       # Loop 2 end of body
       sget-object v0, LB22881413;->unresBase2:La/b/c/dBase2;
       goto :Loop2

:Loop2End

       # Loop 1 end of body
       sget-object v0, LB22881413;->unresBase1:La/b/c/dBase1;
       goto :Loop1

:Loop1End

       return-void

.end method
