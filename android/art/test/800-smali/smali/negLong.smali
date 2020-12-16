.class public LnegLong;
.super Ljava/lang/Object;
.source "negLong.java"
# static fields
.field public static final N:I = 0x64
.field public static i:I
# direct methods
.method static constructor <clinit>()V
    .registers 1
    .prologue
    .line 5
    const/16 v0, 0x44da
    sput v0, LnegLong;->i:I
    return-void
.end method
.method public constructor <init>()V
    .registers 1
    .prologue
    .line 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method
.method public static checkSum1([S)J
    .registers 7
    .prologue
    .line 14
    array-length v3, p0
    .line 15
    const-wide/16 v0, 0x0
    .line 16
    const/4 v2, 0x0
    :goto_4
    if-ge v2, v3, :cond_d
    .line 17
    aget-short v4, p0, v2
    int-to-long v4, v4
    add-long/2addr v0, v4
    .line 16
    add-int/lit8 v2, v2, 0x1
    goto :goto_4
    .line 18
    :cond_d
    return-wide v0
.end method
.method public static init1([SS)V
    .registers 4
    .prologue
    .line 8
    array-length v1, p0
    .line 9
    const/4 v0, 0x0
    :goto_2
    if-ge v0, v1, :cond_9
    .line 10
    aput-short p1, p0, v0
    .line 9
    add-int/lit8 v0, v0, 0x1
    goto :goto_2
    .line 11
    :cond_9
    return-void
.end method
.method public static main([Ljava/lang/String;)V
    .registers 6
    .prologue
    .line 50
    invoke-static {}, LnegLong;->negLong()J
    move-result-wide v0
    .line 51
    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    new-instance v3, Ljava/lang/StringBuilder;
    invoke-direct {v3}, Ljava/lang/StringBuilder;-><init>()V
    const-string v4, "nbp ztw p = "
    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;
    move-result-object v3
    invoke-virtual {v3, v0, v1}, Ljava/lang/StringBuilder;->append(J)Ljava/lang/StringBuilder;
    move-result-object v0
    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;
    move-result-object v0
    invoke-virtual {v2, v0}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    .line 52
    return-void
.end method
.method public static negLong()J
    .registers 17
    .prologue
    .line 23
    const-wide v1, -0x4c4a1f4aa9b1db83L
    .line 24
    const v7, -0x3f727efa
    .line 25
    const/16 v4, -0x284b
    const v3, 0xdc01
    .line 26
    const/16 v0, 0x64
    new-array v8, v0, [S
    .line 28
    const/16 v0, 0x1c60
    invoke-static {v8, v0}, LnegLong;->init1([SS)V
    .line 29
    const/4 v0, 0x2
    move v6, v0
    :goto_18
    const/16 v0, 0x56
    if-ge v6, v0, :cond_64
    .line 30
    const/4 v0, 0x1
    move v5, v0
    move v0, v3
    move-wide v15, v1
    move-wide v2, v15
    :goto_21
    if-ge v5, v6, :cond_5d
    .line 31
    int-to-float v0, v4
    neg-float v1, v7
    add-float/2addr v0, v1
    float-to-int v1, v0
    .line 32
    const/4 v0, 0x1
    move v4, v1
    move-wide v15, v2
    move-wide v1, v15
    .line 33
    :goto_2b
    add-int/lit8 v3, v0, 0x1
    const/16 v0, 0x1b
    if-ge v3, v0, :cond_3a
    .line 35
    int-to-long v9, v5
    mul-long v0, v9, v1
    neg-long v1, v0
    .line 38
    sget v0, LnegLong;->i:I
    move v4, v0
    move v0, v3
    goto :goto_2b
    .line 40
    :cond_3a
    aget-short v0, v8, v6
    int-to-double v9, v0
    long-to-double v11, v1
    const-wide v13, 0x403f9851eb851eb8L
    sub-double/2addr v11, v13
    add-double/2addr v9, v11
    double-to-int v0, v9
    int-to-short v0, v0
    aput-short v0, v8, v6
    .line 41
    const/4 v0, 0x2
    :goto_4a
    const/16 v9, 0x43
    if-ge v0, v9, :cond_56
    .line 42
    neg-long v9, v1
    const-wide/16 v11, 0x1
    or-long/2addr v9, v11
    add-long/2addr v1, v9
    .line 41
    add-int/lit8 v0, v0, 0x1
    goto :goto_4a
    .line 30
    :cond_56
    add-int/lit8 v0, v5, 0x1
    move v5, v0
    move v0, v3
    move-wide v15, v1
    move-wide v2, v15
    goto :goto_21
    .line 29
    :cond_5d
    add-int/lit8 v1, v6, 0x1
    move v6, v1
    move-wide v15, v2
    move-wide v1, v15
    move v3, v0
    goto :goto_18
    .line 45
    :cond_64
    invoke-static {v8}, LnegLong;->checkSum1([S)J
    move-result-wide v0
    int-to-long v2, v3
    add-long/2addr v0, v2
    .line 46
    return-wide v0
.end method
