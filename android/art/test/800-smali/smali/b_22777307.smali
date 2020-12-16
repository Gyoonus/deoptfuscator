.class public LB22777307;
.super Ljava/lang/Object;

# A static field. That way we can use the reference.
.field private static sTest:Ljava/lang/Object;

.method public static run()V
.registers 2
       # This is a broken new-instance. It needs to throw at runtime, though. This test is here to
       # ensure we won't produce a VerifyError.
       # Cloneable was chosen because it's an already existing interface.
       new-instance v0, Ljava/lang/Cloneable;
       invoke-direct {v0}, Ljava/lang/Cloneable;-><init>()V
       sput-object v0, LB22777307;->sTest:Ljava/lang/Object;

       return-void

.end method
