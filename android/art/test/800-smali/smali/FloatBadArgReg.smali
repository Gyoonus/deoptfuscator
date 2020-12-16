.class public LFloatBadArgReg;

.super Ljava/lang/Object;

.method public static getInt(I)I
    .registers 2
    const/4 v0, 0x0
    if-ne v0, v0, :after
    float-to-int v0, v0
    :exit
    add-int/2addr v0, v1
    return v0
    :after
    move v1, v0
    goto :exit
.end method
