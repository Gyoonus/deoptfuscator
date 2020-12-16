.class public LB21614284;
.super Ljava/lang/Object;

.field private a:I

.method public constructor <init>()V
    .registers 2
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    const v0, 42
    iput v0, p0, LB21614284;->a:I
    return-void
.end method

.method public static test(LB21614284;)I
    .registers 2
    # Empty if, testing p0.
    if-nez p0, :label
    :label
    # p0 still needs a null check.
    iget v0, p0, LB21614284;->a:I
    return v0
.end method
