.class public LUnbalancedStraight;

.super Ljava/lang/Object;

.method public static run(Ljava/lang/Object;Ljava/lang/Object;)V
   .registers 3

   invoke-static {}, LMain;->assertIsInterpreted()V

   monitor-enter v1      # 1
   monitor-enter v2      # 2

   monitor-exit v1       # 1     Unbalanced unlock.
   monitor-exit v2       # 2

   return-void

.end method
