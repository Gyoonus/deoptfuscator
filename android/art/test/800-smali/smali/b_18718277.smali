.class public LB18718277;

.super Ljava/lang/Object;

.method public static helper(I)I
    .locals 1
    add-int/lit8 v0, p0, 2
    neg-int v0, v0
    return v0
.end method

.method public static getInt()I
    .registers 2
    const/4 v1, 3
    invoke-static {v1}, LB18718277;->helper(I)I
    move-result v0
    :outer_loop
    if-eqz v1, :exit_outer_loop
    const/4 v0, 0
    if-eqz v0, :skip_dead_loop
    :dead_loop
    add-int/2addr v0, v0
    if-gez v0, :dead_loop
    :skip_dead_loop
    add-int/lit8 v1, v1, -1
    goto :outer_loop
    :exit_outer_loop
    return v0
.end method
