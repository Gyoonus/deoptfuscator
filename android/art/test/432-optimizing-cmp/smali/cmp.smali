.class public LTestCmp;

.super Ljava/lang/Object;

.method public static $opt$CmpLong(JJ)I
   .registers 5
   cmp-long v0, v1, v3
   return v0
.end method

.method public static $opt$CmpGtFloat(FF)I
   .registers 3
   cmpg-float v0, v1, v2
   return v0
.end method

.method public static $opt$CmpLtFloat(FF)I
   .registers 3
   cmpl-float v0, v1, v2
   return v0
.end method

.method public static $opt$CmpGtDouble(DD)I
   .registers 5
   cmpg-double v0, v1, v3
   return v0
.end method

.method public static $opt$CmpLtDouble(DD)I
   .registers 5
   cmpl-double v0, v1, v3
   return v0
.end method
