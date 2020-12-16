.class public LB23300986;

.super Ljava/lang/Object;

.method public static runAliasAfterEnter(Ljava/lang/Object;)V
   .registers 3
   monitor-enter v2        # Lock on parameter
   move-object v1, v2      # Copy parameter into v1, establishing an alias.
   monitor-exit v1         # Unlock on alias
   monitor-enter v2        # Do it again.
   monitor-exit v1
   return-void
.end method

.method public static runAliasBeforeEnter(Ljava/lang/Object;)V
   .registers 3
   move-object v1, v2      # Copy parameter into v1, establishing an alias.
   monitor-enter v2        # Lock on parameter
   monitor-exit v1         # Unlock on alias
   monitor-enter v2        # Do it again.
   monitor-exit v1
   return-void
.end method
