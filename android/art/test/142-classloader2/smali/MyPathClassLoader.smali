# Simple subclass of PathClassLoader with methods overridden.
# We need to use smali right now to subclass a libcore class, see b/24304298.

.class public LMyPathClassLoader;

.super Ldalvik/system/PathClassLoader;

# Simple forwarding constructor.
.method public constructor <init>(Ljava/lang/String;Ljava/lang/ClassLoader;)V
    .registers 3
    invoke-direct {p0, p1, p2}, Ldalvik/system/PathClassLoader;-><init>(Ljava/lang/String;Ljava/lang/ClassLoader;)V
    return-void
.end method
