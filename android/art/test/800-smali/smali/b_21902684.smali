.class public LB21902684;
.super Ljava/lang/Object;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public test()V
    .registers 1
    goto :end
    new-instance v0, Ljava/lang/String;
    invoke-direct {v0}, Ljava/lang/String;-><init>()V
    :end
    return-void
.end method
