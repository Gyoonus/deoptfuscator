.class public LMoveExceptionOnEntry;

.super Ljava/lang/Object;

# Test that we cannot have a catch-handler with move-exception at the beginning of a method.

.method public static moveExceptionOnEntry(I)I
.registers 4
:Label1
       move-exception v2
       const v1, 100
       move v0, p0
       add-int/lit8 p0, p0, 1

:Label2
       invoke-static {v0}, LMoveExceptionOnEntry;->foo(I)V

:Label3
       return v1

.catchall {:Label2 .. :Label3} :Label1
.end method

.method public static foo(I)I
.registers 4
:Label1
       return-void

.end method

