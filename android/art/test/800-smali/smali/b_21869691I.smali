# Test that the verifier does not stash methods incorrectly because they are being invoked with
# the wrong opcode.
#
# This is the interface class that has an "a" method.

.class public abstract interface LB21869691I;

.super Ljava/lang/Object;

.method public abstract a()V
.end method
