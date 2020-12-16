# Test that the verifier does not stash methods incorrectly because they are being invoked with
# the wrong opcode. See b_21869691A.smali for explanation.

.class public LB21869691C;

.super LB21869691B;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, LB21869691B;-><init>()V
    return-void
.end method
