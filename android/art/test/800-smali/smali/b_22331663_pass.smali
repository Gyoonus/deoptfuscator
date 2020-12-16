.class public LB22331663Pass;
.super Ljava/lang/Object;


.method public static run(Z)V
.registers 6
       if-eqz v5, :Label1

       # Construct a java.lang.Object completely. This makes v4 of reference type.
       new-instance v4, Ljava/lang/Object;
       invoke-direct {v4}, Ljava/lang/Object;-><init>()V

:Label1
       # At this point, v4 is the merge of Undefined and ReferenceType. The verifier should not
       # reject this if it is unused.

       # Do an allocation here. This will force heap checking in gcstress mode.
       new-instance v0, Ljava/lang/Object;
       invoke-direct {v0}, Ljava/lang/Object;-><init>()V

       return-void
.end method
