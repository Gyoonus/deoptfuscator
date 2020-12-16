.class public LCmpLong;
.super Ljava/lang/Object;


.method public constructor <init>()V
.registers 1
       invoke-direct {p0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public static run()I
.registers 5000
       const-wide v100, 5678233453L
       move-wide/from16 v101, v100
       const-wide v4, 5678233453L
       cmp-long v0, v101, v4
       return v0
.end method
