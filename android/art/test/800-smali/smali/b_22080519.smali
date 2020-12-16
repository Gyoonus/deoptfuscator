.class public LB22080519;
.super Ljava/lang/Object;

.method public static run()V
.registers 6
:Label1
       const v1, 15
       const v2, 0
       # Have a branch to reach both the aget-object and something else.
       if-eqz v1, :Label2

       # This instruction will be marked runtime-throw.
       aget-object v3, v2, v1

:Label2
       # This should *not* be flagged as a runtime throw
       goto :Label4

:Label3
       move-exception v3
       throw v3

:Label4
       return-void

.catchall {:Label1 .. :Label3} :Label3
.end method