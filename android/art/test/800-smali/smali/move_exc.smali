.class public LMoveExc;
.super Ljava/lang/Object;


.method public constructor <init>()V
.registers 1
       invoke-direct {p0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public static run()V
.registers 6
:Label1
       const v1, 15
       const v2, 0
       div-int v0, v1, v2

:Label2
       goto :Label4

:Label3
       move-exception v3
       throw v3

:Label4
       return-void

.catchall {:Label1 .. :Label2} :Label3
.end method
