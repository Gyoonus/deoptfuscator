.class public LUnbalancedJoin;

.super Ljava/lang/Object;

.method public static run(Ljava/lang/Object;Ljava/lang/Object;)V
   .registers 3

   invoke-static {}, LMain;->assertIsInterpreted()V

   if-eqz v2, :Lnull

:LnotNull

   monitor-enter v1      # 1
   monitor-enter v2      # 2
   goto :Lend

:Lnull
   monitor-enter v2      # 1
   monitor-enter v1      # 2

:Lend

   # Lock levels are "opposite" for the joined flows.

   monitor-exit v2       # 2
   monitor-exit v1       # 1

   return-void

.end method
