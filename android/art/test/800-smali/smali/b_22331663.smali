.class public LB22331663;
.super Ljava/lang/Object;


.method public static run(Z)V
.registers 6
       if-eqz v5, :if_eqz_target

       # Construct a java.lang.Object completely, and throw a new exception.
       new-instance v4, Ljava/lang/Object;
       invoke-direct {v4}, Ljava/lang/Object;-><init>()V

       new-instance v3, Ljava/lang/RuntimeException;
       invoke-direct {v3}, Ljava/lang/RuntimeException;-><init>()V
:throw1_begin
       throw v3
:throw1_end

:if_eqz_target
       # Allocate a java.lang.Object (do not initialize), and throw a new exception.
       new-instance v4, Ljava/lang/Object;

       new-instance v3, Ljava/lang/RuntimeException;
       invoke-direct {v3}, Ljava/lang/RuntimeException;-><init>()V
:throw2_begin
       throw v3
:throw2_end

:catch_entry
       # Catch handler. Here we had to merge the uninitialized with the initialized reference,
       # which creates a conflict. Copy the conflict, and then return. This should not make the
       # verifier fail the method.
       move-object v0, v4

       return-void

.catchall {:throw1_begin .. :throw1_end} :catch_entry
.catchall {:throw2_begin .. :throw2_end} :catch_entry
.end method
