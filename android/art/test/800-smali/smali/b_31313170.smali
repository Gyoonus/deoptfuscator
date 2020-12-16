.class public LB31313170;
.super Ljava/lang/Object;


.method public constructor <init>()V
.registers 1
       invoke-direct {p0}, Ljava/lang/Object;-><init>()V
       return-void
.end method

.method public static run()I
.registers 4
       const/4 v0, 0
       const/4 v1, 1
       sget v2, LB31313170;->a:I
       if-nez v2, :exit
       move-object v1, v0
       :exit
       return v1
.end method

.field static public a:I
