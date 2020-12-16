.class public LsameFieldNames;
.super Ljava/lang/Object;

# Test multiple fields with the same name and different types.
# (Invalid in Java language but valid in bytecode.)
.field static public a:D
.field static public a:S
.field static public a:J
.field static public a:F
.field static public a:Z
.field static public a:I
.field static public a:B
.field static public a:C
.field static public a:Ljava/lang/Integer;
.field static public a:Ljava/lang/Long;
.field static public a:Ljava/lang/Float;
.field static public a:Ljava/lang/Double;
.field static public a:Ljava/lang/Boolean;
.field static public a:Ljava/lang/Void;
.field static public a:Ljava/lang/Short;
.field static public a:Ljava/lang/Char;
.field static public a:Ljava/lang/Byte;

# Add some more fields to stress test the sorting for offset assignment.
.field static public b:C
.field static public c:J
.field static public d:C
.field static public e:B
.field static public f:C
.field static public g:J
.field static public h:C
.field static public i:J
.field static public j:I
.field static public k:J
.field static public l:J
.field static public m:I
.field static public n:J
.field static public o:I
.field static public p:Ljava/lang/Integer;
.field static public q:I
.field static public r:J
.field static public s:I
.field static public t:Ljava/lang/Integer;
.field static public u:I
.field static public v:J
.field static public w:I
.field static public x:Ljava/lang/Integer;
.field static public y:I
.field static public z:Ljava/lang/Integer;

.method public static getInt()I
    .locals 2
    const/4 v0, 2
    sput v0, LsameFieldNames;->a:I
    sget-object v1, LsameFieldNames;->a:Ljava/lang/Integer;
    const/4 v1, 0
    if-nez v1, :fail
    const/4 v0, 7
    :ret
    return v0
    :fail
    const/4 v0, 0
    goto :ret
.end method
