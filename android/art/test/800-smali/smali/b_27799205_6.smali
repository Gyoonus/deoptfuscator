.class public LB27799205_6;
.super Ljava/lang/Object;

# A class with an unresolved array type should not fail hard (unless it's a primitive-type access).
# Make sure that non-merged types still work.

.method public static run()V
.registers 1
       return-void
.end method

# Use some non-resolvable array type.
.method public static test([Ldo/not/resolve/K;)Ldo/not/resolve/K;
.registers 3
       const v0, 0
       const v1, 0
       # v2 = p0

       # v0 := v2[v1]
       aget-object v0, v2, v1

       return-object v0

.end method
