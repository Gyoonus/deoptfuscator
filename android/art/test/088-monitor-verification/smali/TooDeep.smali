.class public LTooDeep;

.super Ljava/lang/Object;

.method public static run(Ljava/lang/Object;)V
   .registers 3

   # Lock depth is 33, which is more than the verifier supports. This should have been punted to
   # the interpreter.
   invoke-static {}, LMain;->assertIsInterpreted()V

   monitor-enter v2        #  1
   monitor-enter v2        #  2
   monitor-enter v2        #  3
   monitor-enter v2        #  4
   monitor-enter v2        #  5
   monitor-enter v2        #  6
   monitor-enter v2        #  7
   monitor-enter v2        #  8
   monitor-enter v2        #  9
   monitor-enter v2        # 10
   monitor-enter v2        # 11
   monitor-enter v2        # 12
   monitor-enter v2        # 13
   monitor-enter v2        # 14
   monitor-enter v2        # 15
   monitor-enter v2        # 16
   monitor-enter v2        # 17
   monitor-enter v2        # 18
   monitor-enter v2        # 19
   monitor-enter v2        # 20
   monitor-enter v2        # 21
   monitor-enter v2        # 22
   monitor-enter v2        # 23
   monitor-enter v2        # 24
   monitor-enter v2        # 25
   monitor-enter v2        # 26
   monitor-enter v2        # 27
   monitor-enter v2        # 28
   monitor-enter v2        # 29
   monitor-enter v2        # 30
   monitor-enter v2        # 31
   monitor-enter v2        # 32
   monitor-enter v2        # 33

   monitor-exit v2         #  1
   monitor-exit v2         #  2
   monitor-exit v2         #  3
   monitor-exit v2         #  4
   monitor-exit v2         #  5
   monitor-exit v2         #  6
   monitor-exit v2         #  7
   monitor-exit v2         #  8
   monitor-exit v2         #  9
   monitor-exit v2         # 10
   monitor-exit v2         # 11
   monitor-exit v2         # 12
   monitor-exit v2         # 13
   monitor-exit v2         # 14
   monitor-exit v2         # 15
   monitor-exit v2         # 16
   monitor-exit v2         # 17
   monitor-exit v2         # 18
   monitor-exit v2         # 19
   monitor-exit v2         # 20
   monitor-exit v2         # 21
   monitor-exit v2         # 22
   monitor-exit v2         # 23
   monitor-exit v2         # 24
   monitor-exit v2         # 25
   monitor-exit v2         # 26
   monitor-exit v2         # 27
   monitor-exit v2         # 28
   monitor-exit v2         # 29
   monitor-exit v2         # 30
   monitor-exit v2         # 31
   monitor-exit v2         # 32
   monitor-exit v2         # 33

   return-void

.end method
