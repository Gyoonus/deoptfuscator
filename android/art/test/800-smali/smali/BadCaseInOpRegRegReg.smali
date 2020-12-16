.class public LBadCaseInOpRegRegReg;

.super Ljava/lang/Object;

.method public static getInt()I
    .registers 2
    const/4 v0, 0x0
    const/4 v1, 0x1
    add-int/2addr v0, v1
    add-int/lit8 v1, v0, 0x1
    mul-int v0, v1, v0
    return v0
.end method
