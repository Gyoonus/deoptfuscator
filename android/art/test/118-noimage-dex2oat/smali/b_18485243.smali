.class public LB18485243;
.super Ljava/lang/Object;
.source "b_18485243.smali"

.method public constructor <init>()V
    .registers 2
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method private static toInt()I
    .registers 1
    const v0, 0
    return v0
.end method

.method public run()I
    .registers 3
    invoke-direct {p0}, LB18485243;->toInt()I
    move-result v0
    return v0
.end method
