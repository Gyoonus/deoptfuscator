.class public LB17790197;

.super Ljava/lang/Object;

.method public static getInt()I
    .registers 4
    const/16 v0, 100
    const/4 v1, 1
    const/4 v2, 7
    :loop
    if-eq v2, v0, :done
    add-int v2, v2, v1
    goto :loop
    :done
    add-float v3, v0, v1
    return v2
.end method
