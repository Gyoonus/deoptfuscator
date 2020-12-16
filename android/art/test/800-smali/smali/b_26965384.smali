.class public LB26965384;
.super LB26965384Super;

.method public constructor <init>()V
    .locals 1
    const v0, 0
    iput v0, p0, LB26965384;->a:I
    invoke-direct {p0}, LB26965384Super;-><init>()V
    return-void
.end method


# Just by loading this class we should fail. It doesn't really matter what's in
# this method.
.method public static run()V
    .registers 4
    new-instance v0, LB26965384;
    invoke-direct {v0}, LB26965384;-><init>()V
    return-void
.end method
