.class public LB27799205Helper;
.super Ljava/lang/Object;

# Helper for B27799205. Reflection tries to resolve all types. That's bad for intentionally
# unresolved types. It makes it harder to distinguish what kind of error we got.

.method public static run1()V
.registers 1
       invoke-static {}, LB27799205_1;->run()V

       return-void
.end method

.method public static run2()V
.registers 1
       invoke-static {}, LB27799205_2;->run()V

       return-void
.end method

.method public static run3()V
.registers 1
       invoke-static {}, LB27799205_3;->run()V

       return-void
.end method

.method public static run4()V
.registers 1
       invoke-static {}, LB27799205_4;->run()V

       return-void
.end method

.method public static run5()V
.registers 1
       invoke-static {}, LB27799205_5;->run()V

       return-void
.end method

.method public static run6()V
.registers 1
       invoke-static {}, LB27799205_6;->run()V

       return-void
.end method
