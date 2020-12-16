.class public LB20843113;
.super Ljava/lang/Object;


.method public constructor <init>(I)V
.registers 2

:Label1
       # An instruction that may throw, so as to pass UninitializedThis to the handler
       div-int v1, v1, v1

       # Call the super-constructor
       invoke-direct {v0}, Ljava/lang/Object;-><init>()V

       # Return normally.
       return-void

:Label2


:Handler
       move-exception v0                    # Overwrite the (last) "this" register. This should be
                                            # allowed as we will terminate abnormally below.

       throw v0                             # Terminate abnormally

.catchall {:Label1 .. :Label2} :Handler
.end method

# Just a dummy.
.method public static run()V
.registers 1
       return-void
.end method
