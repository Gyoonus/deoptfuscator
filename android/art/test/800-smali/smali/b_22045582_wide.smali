.class public LB22045582Wide;

# Fail verification of a method that returns an undefined wide register.

.super Ljava/lang/Object;

.method public static run()J
    .registers 4
    # v0/v1 is undefined here.
    return-wide v0
.end method
