.class public LB22411633_1;
.super Ljava/lang/Object;


.method public static run(Z)V
.registers 6
       # Make v3 & v4 defined, just use null.
       const v3, 0
       const v4, 0

       # Allocate a java.lang.Object (do not initialize).
       new-instance v4, Ljava/lang/Object;

       # Branch forward.
       if-eqz v5, :LabelMerge

       # Just some random work.
       add-int/lit16 v3, v3, 1

       # Another branch forward.
       if-nez v5, :LabelMerge

       # Some more random work, technically dead, but reachable.
       add-int/lit16 v3, v3, 1

:LabelMerge
       # v4 is still an uninitialized reference here. Initialize it.
       invoke-direct {v4}, Ljava/lang/Object;-><init>()V

       # And test whether it's initialized by calling hashCode.
       invoke-virtual {v4}, Ljava/lang/Object;->hashCode()I

       return-void

.end method
