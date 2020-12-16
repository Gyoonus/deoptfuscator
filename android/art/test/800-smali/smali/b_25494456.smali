.class public LB25494456;

.super Ljava/lang/Object;

# Ensure that a type mismatch (integral/float vs reference) overrides a soft failure (because of
# an unresolvable type) in return-object.

.method public static run()Lwont/be/Resolvable;
    .registers 1

    const/4 v0, 1
    return-object v0

.end method
