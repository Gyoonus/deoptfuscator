.class public LFilledNewArray;

.super Ljava/lang/Object;

.method public static newInt(III)[I
   .registers 4
   filled-new-array {v1, v2, v3}, [I
   move-result-object v0
   return-object v0
.end method

.method public static newRef(Ljava/lang/Object;Ljava/lang/Object;)[Ljava/lang/Object;
   .registers 3
   filled-new-array {v1, v2}, [Ljava/lang/Object;
   move-result-object v0
   return-object v0
.end method

.method public static newArray([I[I)[[I
   .registers 3
   filled-new-array {v1, v2}, [[I
   move-result-object v0
   return-object v0
.end method

.method public static newIntRange(III)[I
   .registers 4
   filled-new-array/range {v1 .. v3}, [I
   move-result-object v0
   return-object v0
.end method

.method public static newRefRange(Ljava/lang/Object;Ljava/lang/Object;)[Ljava/lang/Object;
   .registers 3
   filled-new-array/range {v1 .. v2}, [Ljava/lang/Object;
   move-result-object v0
   return-object v0
.end method

.method public static newArrayRange([I[I)[[I
   .registers 3
   filled-new-array/range {v1 .. v2}, [[I
   move-result-object v0
   return-object v0
.end method
