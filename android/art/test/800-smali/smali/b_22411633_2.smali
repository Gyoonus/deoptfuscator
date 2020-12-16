.class public LB22411633_2;
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

       # Create a non-precise object reference. We can do this by merging to objects together
       # that only have Object as a common ancestor.

       # Allocate a java.lang.Object and initialize it.
       new-instance v4, Ljava/lang/Object;
       invoke-direct {v4}, Ljava/lang/Object;-><init>()V

       if-nez v5, :LabelMergeObject

       new-instance v4, Ljava/lang/Integer;
       invoke-direct {v4}, Ljava/lang/Integer;-><init>()V

:LabelMergeObject

       # Dummy work to separate blocks. At this point, v4 is of type Reference<Object>.
       add-int/lit16 v3, v3, 1

:LabelMerge
       # Merge the uninitialized Object from line 12 with the reference to Object from 31. Older
       # rules set any reference merged with Object to Object. This is wrong in the case of the
       # other reference being an uninitialized reference, as we'd suddenly allow calling on it.

       # Test whether it's some initialized reference by calling hashCode. This should fail, as we
       # merged initialized and uninitialized.
       invoke-virtual {v4}, Ljava/lang/Object;->hashCode()I

       return-void

.end method
