.class public LFillArrayData;

.super Ljava/lang/Object;

.method public static emptyIntArray([I)V
   .registers 1

   fill-array-data v0, :ArrayData
   return-void

:ArrayData
    .array-data 4
    .end array-data

.end method

.method public static intArray([I)V
   .registers 1

   fill-array-data v0, :ArrayData
   return-void

:ArrayData
    .array-data 4
        1 2 3 4 5
    .end array-data

.end method

.method public static intArrayFillInstructionAfterData([I)V
   .registers 1
   goto :FillInstruction

:ArrayData
    .array-data 4
        1 2 3 4 5
    .end array-data

:FillInstruction
   fill-array-data v0, :ArrayData
   return-void

.end method

.method public static shortArray([S)V
   .registers 1

   fill-array-data v0, :ArrayData
   return-void

:ArrayData
    .array-data 2
        1 2 3 4 5
    .end array-data

.end method

.method public static charArray([C)V
   .registers 1

   fill-array-data v0, :ArrayData
   return-void

:ArrayData
    .array-data 2
        1 2 3 4 5
    .end array-data

.end method

.method public static byteArray([B)V
   .registers 1

   fill-array-data v0, :ArrayData
   return-void

:ArrayData
    .array-data 1
        1 2 3 4 5
    .end array-data

.end method

.method public static booleanArray([Z)V
   .registers 1

   fill-array-data v0, :ArrayData
   return-void

:ArrayData
    .array-data 1
        0 1 1
    .end array-data

.end method

.method public static longArray([J)V
   .registers 1

   fill-array-data v0, :ArrayData
   return-void

:ArrayData
    .array-data 8
        1 2 3 4 5
    .end array-data

.end method
