.class public LB29778499_1;
.super Ljava/lang/Object;

# Test returning an object that doesn't implement the declared output interface.

.method public static run()V
.registers 2
       invoke-static {}, LB29778499_1;->test()Ljava/lang/Runnable;
       move-result-object v0
       invoke-interface {v0}, Ljava/lang/Runnable;->run()V
       return-void
.end method

.method public static test()Ljava/lang/Runnable;
.registers 1
       new-instance v0, LB29778499_1;
       invoke-direct {v0}, LB29778499_1;-><init>()V
       return-object v0
.end method
