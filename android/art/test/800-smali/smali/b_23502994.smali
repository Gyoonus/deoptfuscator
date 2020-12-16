.class public LB23502994;

.super Ljava/lang/Object;

.method public static runIF_EQZ(Ljava/lang/Object;)V
   .registers 3
   monitor-enter v2        # Lock on parameter

   # Sharpen, and try to unlock (in both branches). We should not lose the lock info when we make
   # the register type more precise.

   instance-of v0, v2, Ljava/lang/String;
   if-eqz v0, :LnotString

   # At this point v2 is of type Ljava/lang/String;
   monitor-exit v2

   goto :Lend

:LnotString
   monitor-exit v2         # Unlock the else branch

   # Fall-through.

:Lend
   return-void

.end method


.method public static runCHECKCAST(Ljava/lang/Object;)V
   .registers 3
   monitor-enter v2        # Lock on parameter

   # Sharpen, and try to unlock. We should not lose the lock info when we make the register type
   # more precise.

   check-cast v2, Ljava/lang/String;

   # At this point v2 is of type Ljava/lang/String;
   monitor-exit v2

   return-void

.end method
