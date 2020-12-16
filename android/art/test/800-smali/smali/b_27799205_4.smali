.class public LB27799205_4;
.super Ljava/lang/Object;

# A class with an unresolved array type should not fail hard (unless it's a primitive-type access).
# Make sure that merging is pro-active.

.method public static run()V
.registers 1
       return-void
.end method

# Use some primitive-type array input.
.method public static test([I[Ldo/not/resolve/K;Z)V
.registers 6
       # Make v0, v1 and v2 null. We'll use v0 as a merge of the inputs, v1 as null, and v2 as 0.
       const v0, 0
       const v1, 0
       const v2, 0

       # Conditional jump so we have a merge point.
       if-eqz v5, :LabelSelectUnresolved

:LabelSelectResolved
       move-object v0, v3
       goto :LabelMerged

:LabelSelectUnresolved
       move-object v0, v4
       goto :LabelMerged

:LabelMerged
       # At this point, v0 should be Object.

       # Test aput-object: v0[v2] = v1. Should fail for v0 not being an array.
       aput-object v1, v0, v2

       return-void

.end method
