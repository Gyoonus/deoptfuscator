# Make sure we accept non-abstract classes with abstract members.

.class public LB26143249;

.super Ljava/lang/Object;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public run()V
    .registers 1
    invoke-virtual {p0}, LB26143249;->abs()V
    return-void
.end method

.method public abstract abs()V
.end method
