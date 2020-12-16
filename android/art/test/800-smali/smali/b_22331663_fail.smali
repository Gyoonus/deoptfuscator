.class public LB22331663Fail;
.super Ljava/lang/Object;


.method public static run(Z)V
.registers 6
       if-eqz v5, :Label1

       # Construct a java.lang.Object completely. This makes v4 of reference type.
       new-instance v4, Ljava/lang/Object;
       invoke-direct {v4}, Ljava/lang/Object;-><init>()V

:Label1
       # At this point, v4 is the merge of Undefined and ReferenceType. The verifier should
       # reject any use of this, even a copy. Previously this was a conflict. Conflicts must
       # be movable now, so ensure that we do not get a conflict (and then allow the move).
       move-object v0, v4

       return-void
.end method
