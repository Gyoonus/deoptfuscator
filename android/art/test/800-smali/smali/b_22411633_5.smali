.class public LB22411633_5;
.super Ljava/lang/Object;


.method public static run(Z)V
.registers 6
       # Do not merge into the backward branch target.
       goto :LabelEntry

:LabelBwd
       # At this point v4 is an uninitialized reference. We should be able to initialize here
       # and call a method afterwards.
       invoke-direct {v4}, Ljava/lang/Object;-><init>()V
       invoke-virtual {v4}, Ljava/lang/Object;->hashCode()I

       # Make sure this is not an infinite loop.
       const v5, 1

:LabelEntry
       # Allocate a java.lang.Object (do not initialize).
       new-instance v4, Ljava/lang/Object;

       # Branch backward.
       if-eqz v5, :LabelBwd

       return-void

.end method
