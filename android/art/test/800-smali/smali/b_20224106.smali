.class public LB20224106;

# Test that a hard + soft verifier failure in invoke-interface does not lead to
# an order abort (the last failure must be hard).

.super Ljava/lang/Object;

.method public static run(LB20224106;Ljava/lang/Object;)V
    .registers 4
    # Two failure points here:
    # 1) There is a parameter type mismatch. The formal type is integral (int), but the actual
    #    type is reference.
    # 2) The receiver is not an interface or Object
    invoke-interface {v2, v3}, Ljava/net/DatagramSocket;->checkPort(I)V
    return-void
.end method
