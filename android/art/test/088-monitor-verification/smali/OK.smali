.class public LOK;

.super Ljava/lang/Object;

.method public static run(Ljava/lang/Object;Ljava/lang/Object;)V
   .registers 3

   invoke-static {v1, v2}, LOK;->runNoMonitors(Ljava/lang/Object;Ljava/lang/Object;)V

   invoke-static {v1, v2}, LOK;->runStraightLine(Ljava/lang/Object;Ljava/lang/Object;)V

   invoke-static {v1, v2}, LOK;->runBalancedJoin(Ljava/lang/Object;Ljava/lang/Object;)V

   return-void

.end method



.method public static runNoMonitors(Ljava/lang/Object;Ljava/lang/Object;)V
   .registers 3

   invoke-static {}, LMain;->assertIsManaged()V

   return-void

.end method

.method public static runStraightLine(Ljava/lang/Object;Ljava/lang/Object;)V
   .registers 3

   invoke-static {}, LMain;->assertIsManaged()V

   monitor-enter v1      # 1
   monitor-enter v2      # 2

   monitor-exit v2       # 2
   monitor-exit v1       # 1

   return-void

.end method

.method public static runBalancedJoin(Ljava/lang/Object;Ljava/lang/Object;)V
   .registers 3

   invoke-static {}, LMain;->assertIsManaged()V

   monitor-enter v1      # 1

   if-eqz v2, :Lnull

:LnotNull

   monitor-enter v2      # 2
   goto :Lend

:Lnull
   monitor-enter v2      # 2

:Lend

   monitor-exit v2       # 2
   monitor-exit v1       # 1

   return-void

.end method
