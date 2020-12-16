.class public LB21873167;
.super Ljava/lang/Object;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public test()V
    .registers 1
    :start
    monitor-enter p0
    monitor-exit  p0
    :end
    return-void
    .catchall {:start .. :end} :end
.end method
