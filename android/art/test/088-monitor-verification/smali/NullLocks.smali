.class public LNullLocks;

.super Ljava/lang/Object;

.method public static run(Z)V
   .registers 3

   invoke-static {}, LMain;->assertIsManaged()V

   if-eqz v2, :Lfalse

   const v0, 0           # Null.
   monitor-enter v0
   const v1, 0           # Another null. This should be detected as an alias, such that the exit
                         # will not fail verification.
   monitor-exit v1

   monitor-enter v0
   monitor-exit v1

   monitor-enter v1
   monitor-exit v0

:Lfalse

   return-void

.end method
