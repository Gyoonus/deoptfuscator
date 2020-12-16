.class public LB18380491ConcreteClass;

.super LB18380491AbstractBase;

.method public constructor <init>()V
    .locals 0
    invoke-direct {p0}, LB18380491AbstractBase;-><init>()V
    return-void
.end method

.method public foo(I)I
  .locals 1
  if-eqz p1, :invoke_super_abstract
  return p1
  :invoke_super_abstract
  invoke-super {p0, p1}, LB18380491AbstractBase;->foo(I)I
  move-result v0
  return v0
.end method
