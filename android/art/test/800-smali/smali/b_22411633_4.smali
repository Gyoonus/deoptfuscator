.class public LB22411633_4;
.super Ljava/lang/Object;


.method public static run(Z)V
.registers 6
       # Do not merge into the backward branch target.
       goto :LabelEntry

:LabelBwd
       # At this point v4 is an uninitialized reference. This should fail to verify.
       # Note: we make sure that it is an uninitialized reference and not a conflict in sister
       #       file b_22411633_bwdok.smali.
       invoke-virtual {v4}, Ljava/lang/Object;->hashCode()I

:LabelEntry
       # Allocate a java.lang.Object (do not initialize).
       new-instance v4, Ljava/lang/Object;

       # Branch backward.
       if-eqz v5, :LabelBwd

       return-void

.end method
