.class public LFloatIntConstPassing;

.super Ljava/lang/Object;

.method public static getInt(I)I
  .registers 2
  const/4 v0, 1
  add-int/2addr v0, p0
  return v0
.end method

.method public static getFloat(F)F
  .registers 2
  const/4 v0, 0
  mul-float/2addr v0, p0
  return v0
.end method

.method public static run()I
  .registers 3
  const/4 v0, 1
  invoke-static {v0}, LFloatIntConstPassing;->getInt(I)I
  move-result v1
  invoke-static {v0}, LFloatIntConstPassing;->getFloat(F)F
  move-result v2
  float-to-int v2, v2
  add-int/2addr v1, v2
  return v1
.end method
