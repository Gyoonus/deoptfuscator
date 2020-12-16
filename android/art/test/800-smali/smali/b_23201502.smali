.class public LB23201502;

.super Ljava/lang/Object;

.method public static runFloat()V
   .registers 3
   const v0, 0             # Null array.
   const v1, 0             # 0 index into array.
   const v2, 0             # 0 value, will be turned into float.
   int-to-float v2, v2     # Definitely make v2 float.
   aput v2 , v0, v1        # Put into null array.
   return-void
.end method

.method public static runDouble()V
   .registers 4
   const v0, 0             # Null array.
   const v1, 0             # 0 index into array.
   const v2, 0             # 0 value, will be turned into double.
   int-to-double v2, v2    # Definitely make v2+v3 double.
   aput-wide v2 , v0, v1   # Put into null array.
   return-void
.end method
