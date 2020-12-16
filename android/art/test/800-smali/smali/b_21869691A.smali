# Test that the verifier does not stash methods incorrectly because they are being invoked with
# the wrong opcode.
#
# When using invoke-interface on a method id that is not from an interface class, we should throw
# an IncompatibleClassChangeError. FindInterfaceMethod assumes that the given type is an interface,
# so we can construct a class hierarchy that would have a surprising result:
#
#   interface I {
#     void a();
#   }
#
#   class B implements I {
#      // miranda method for a, or a implemented.
#   }
#
#   class C extends B {
#   }
#
# Then calling invoke-interface C.a() will go wrong if there is no explicit check: a can't be found
# in C, but in the interface table, so we will find an interface method and pass ICCE checks.
#
# If we do this before a correct invoke-virtual C.a(), we poison the dex cache with an incorrect
# method. In this test, this is done in A (A < B, so processed first). The "real" call is in B.

.class public LB21869691A;

.super Ljava/lang/Object;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public run()V
  .registers 3
  new-instance v0, LB21869691C;
  invoke-direct {v0}, LB21869691C;-><init>()V
  invoke-virtual {v2, v0}, LB21869691A;->callinf(LB21869691C;)V
  return-void
.end method

.method public callinf(LB21869691C;)V
  .registers 2
  invoke-interface {p1}, LB21869691C;->a()V
  return-void
.end method
