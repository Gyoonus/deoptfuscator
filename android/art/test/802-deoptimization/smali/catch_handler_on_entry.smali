.class public LCatchHandlerOnEntry;

.super Ljava/lang/Object;

# Test we can execute a method starting with a catch handler (without
# move-exception instruction). This method must be called with parameter
# initialized to 0.
#
# We execute the catch handler (Label1) for the first time with p0 == 0.
# We save its value in v0, increment p0 to 1 and execute the div-int
# instruction (Label2) which throws an ArithmeticException (division by zero).
# That exception is caught by the catch handler so we execute it a second time.
# Now p0 == 1. When we we execute the div-int instruction, it succeeds and we
# return its result: this is the initial value of v1 because "v1 = v1 / 1".
.method public static catchHandlerOnEntry(I)I
.registers 4
:Label1
       const v1, 100
       move v0, p0
       add-int/lit8 p0, p0, 1

:Label2
       invoke-static {v0}, LCatchHandlerOnEntryHelper;->throwExceptionDuringDeopt(I)V

:Label3
       return v1

.catchall {:Label2 .. :Label3} :Label1
.end method
