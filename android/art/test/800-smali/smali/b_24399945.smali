.class public Lb_24399945;

.super Ljava/lang/Object;

.method public static packedSwitch_overflow(I)I
    .registers 2

    const/4 v0, 0
    packed-switch v0, :switch_data
    goto :default

    :switch_data
    .packed-switch 0x7FFFFFFE
        :case1 # key = INT_MAX - 1
        :case2 # key = INT_MAX
        :case3 # key = INT_MIN (overflow!)
    .end packed-switch

    :return
    return v1

    :default
    goto :return

    :case1
    goto :return
    :case2
    goto :return
    :case3
    goto :return

.end method
