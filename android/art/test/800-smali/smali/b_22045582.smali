.class public LB22045582;

# Fail verification of a method that returns an undefined register even if the return type
# is unresolved.

.super Ljava/lang/Object;

.method public static run()La/b/c/d/e/nonexistent;
    .registers 4
    # v1 is undefined, and the return type cannot be resolved. The Undefined should take
    # precedence here.
    return-object v1
.end method
