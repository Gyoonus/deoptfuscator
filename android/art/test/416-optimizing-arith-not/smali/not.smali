.class public LTestNot;

.super Ljava/lang/Object;

.method public static $opt$NotInt(I)I
   .registers 2
   not-int v0, v1
   return v0
.end method

.method public static $opt$NotLong(J)J
   .registers 4
   not-long v0, v2
   return-wide v0
.end method
