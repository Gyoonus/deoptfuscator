.class public LB22045582Int;

# Fail verification of a method that returns an undefined integral register.

.super Ljava/lang/Object;

.method public static run()I
    .registers 4
    # v1 is undefined here.
    return v1
.end method
