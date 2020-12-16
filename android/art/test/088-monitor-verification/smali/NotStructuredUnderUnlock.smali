.class public LNotStructuredUnderUnlock;

.super Ljava/lang/Object;

.method public static run(Ljava/lang/Object;)V
   .registers 3

   invoke-static {}, LMain;->assertIsInterpreted()V

   # Lock thrice, but only unlock twice.

   monitor-enter v2        #  1
   monitor-enter v2        #  2
   monitor-enter v2        #  3

   monitor-exit v2         #  1
   monitor-exit v2         #  2

   return-void

.end method
