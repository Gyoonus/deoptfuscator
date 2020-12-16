.class public LB21646347;

# If an invoke-virtual dispatches to a miranda method, ensure that we test for the receiver
# being a subclass of the abstract class, not postpone the check because the miranda method's
# declaring class is an interface.

.super Ljava/lang/Object;

.method public static run(LB21646347;)V
    .registers 1
    # Invoke the miranda method on an object of this class. This should fail type-checking,
    # instead of letting this pass as the declaring class is an interface.
    invoke-virtual {v0}, LMain$AbstractClass;->m()V
    return-void
.end method
