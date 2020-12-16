.class public LNotStructuredOverUnlock;

.super Ljava/lang/Object;

.method public static run(Ljava/lang/Object;)V
   .registers 3

   invoke-static {}, LMain;->assertIsInterpreted()V

   # Lock twice, but unlock thrice.

   monitor-enter v2        #  1
   monitor-enter v2        #  2

   monitor-exit v2         #  1
   monitor-exit v2         #  2
   monitor-exit v2         #  3

   return-void

.end method
