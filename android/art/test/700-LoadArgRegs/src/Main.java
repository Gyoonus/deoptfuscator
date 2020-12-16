public class Main {

  static void testI1(int p1) {
     System.out.println(p1);
  }
  static void testI2(int p1, int p2) {
     System.out.println(p1+", "+p2);
  }
  static void testI3(int p1, int p2, int p3) {
     System.out.println(p1+", "+p2+", "+p3);
  }
  static void testI4(int p1, int p2, int p3, int p4) {
     System.out.println(p1+", "+p2+", "+p3+", "+p4);
  }
  static void testI5(int p1, int p2, int p3, int p4, int p5) {
     System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5);
  }
  static void testI6(int p1, int p2, int p3, int p4, int p5, int p6) {
     System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6);
  }
  static void testI7(int p1, int p2, int p3, int p4, int p5, int p6, int p7) {
     System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7);
  }
  static void testI8(int p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8) {
     System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8);
  }
  static void testI9(int p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8, int p9) {
     System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8+", "+p9);
  }
  static void testI10(int p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8, int p9, int p10) {
     System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8+", "+p9+", "+p10);
  }
  static void testI11(int p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8, int p9, int p10, int p11) {
     System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8+", "+p9+", "+p10+", "+p11);
  }
  static void testI12(int p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8, int p9, int p10, int p11, int p12) {
     System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8+", "+p9+", "+p10+", "+p11+", "+p12);
  }
  void testI6_nonstatic(int p1, int p2, int p3, int p4, int p5, int p6) {
     System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6);
  }

  static void testB1(boolean p1) {
    System.out.println(p1);
  }
  static void testB2(boolean p1, boolean p2) {
    System.out.println(p1+", "+p2);
  }
  static void testB3(boolean p1, boolean p2, boolean p3) {
    System.out.println(p1+", "+p2+", "+p3);
  }
  static void testB4(boolean p1, boolean p2, boolean p3, boolean p4) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4);
  }
  static void testB5(boolean p1, boolean p2, boolean p3, boolean p4, boolean p5) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5);
  }
  static void testB6(boolean p1, boolean p2, boolean p3, boolean p4, boolean p5, boolean p6) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6);
  }
  static void testB7(boolean p1, boolean p2, boolean p3, boolean p4, boolean p5, boolean p6, boolean p7) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7);
  }

  static void testO1(Object p1) {
    System.out.println(p1);
  }
  static void testO2(Object p1, Object p2) {
    System.out.println(p1+", "+p2);
  }
  static void testO3(Object p1, Object p2, Object p3) {
    System.out.println(p1+", "+p2+", "+p3);
  }
  static void testO4(Object p1, Object p2, Object p3, Object p4) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4);
  }
  static void testO5(Object p1, Object p2, Object p3, Object p4, Object p5) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5);
  }
  static void testO6(Object p1, Object p2, Object p3, Object p4, Object p5, Object p6) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6);
  }
  static void testO7(Object p1, Object p2, Object p3, Object p4, Object p5, Object p6, Object p7) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7);
  }

  static void testIOB1(int p1) {
    System.out.println(p1);
  }
  static void testIOB2(int p1, Object p2) {
    System.out.println(p1+", "+p2);
  }
  static void testIOB3(int p1, Object p2, boolean p3) {
    System.out.println(p1+", "+p2+", "+p3);
  }
  static void testIOB4(int p1, Object p2, boolean p3, int p4) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4);
  }
  static void testIOB5(int p1, Object p2, boolean p3, int p4, Object p5) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5);
  }
  static void testIOB6(int p1, Object p2, boolean p3, int p4, Object p5, boolean p6) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6);
  }
  static void testIOB7(int p1, Object p2, boolean p3, int p4, Object p5, boolean p6, int p7) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7);
  }

  static void testF1(float p1) {
    System.out.println(p1);
  }
  static void testF2(float p1, float p2) {
    System.out.println(p1+", "+p2);
  }
  static void testF3(float p1, float p2, float p3) {
    System.out.println(p1+", "+p2+", "+p3);
  }
  static void testF4(float p1, float p2, float p3, float p4) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4);
  }
  static void testF5(float p1, float p2, float p3, float p4, float p5) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5);
  }
  static void testF6(float p1, float p2, float p3, float p4, float p5, float p6) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6);
  }
  static void testF7(float p1, float p2, float p3, float p4, float p5, float p6, float p7) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7);
  }
  static void testF8(float p1, float p2, float p3, float p4, float p5, float p6, float p7, float p8) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8);
  }
  static void testF9(float p1, float p2, float p3, float p4, float p5, float p6, float p7, float p8, float p9) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8+", "+p9);
  }
  static void testF10(float p1, float p2, float p3, float p4, float p5, float p6, float p7, float p8, float p9, float p10) {
    System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8+", "+p9+", "+p10);
  }

  static void testD1 (double p1) { System.out.println(p1); }
  static void testD2 (double p1, double p2) { System.out.println(p1+", "+p2); }
  static void testD3 (double p1, double p2, double p3) { System.out.println(p1+", "+p2+", "+p3); }
  static void testD4 (double p1, double p2, double p3, double p4) { System.out.println(p1+", "+p2+", "+p3+", "+p4); }
  static void testD5 (double p1, double p2, double p3, double p4, double p5) { System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5); }
  static void testD6 (double p1, double p2, double p3, double p4, double p5, double p6) { System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6); }
  static void testD7 (double p1, double p2, double p3, double p4, double p5, double p6, double p7) { System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7); }
  static void testD8 (double p1, double p2, double p3, double p4, double p5, double p6, double p7, double p8) { System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8); }
  static void testD9 (double p1, double p2, double p3, double p4, double p5, double p6, double p7, double p8, double p9) { System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8+", "+p9); }
  static void testD9f (float p0, double p1, double p2, double p3, double p4, double p5, double p6, double p7, double p8, double p9) { System.out.println(p0+", "+p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8+", "+p9); }
  static void testD10(double p1, double p2, double p3, double p4, double p5, double p6, double p7, double p8, double p9, double p10) { System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8+", "+p9+", "+p10); }

  static void testI() {
    testI1(11);
    testI2(21, 22);
    testI3(31, 32, 33);
    testI4(41, 42, 43, 44);
    testI5(51, 52, 53, 54, 55);
    testI6(61, 62, 63, 64, 65, 66);
    testI7(71, 72, 73, 74, 75, 76, 77);
    testI8(81, 82, 83, 84, 85, 86, 87, 88);
    testI9(91, 92, 93, 94, 95, 96, 97, 98, 99);
    testI10(101, 102, 103, 104, 105, 106, 107, 108, 109, 110);
    testI11(111, 112, 113, 114, 115, 116, 117, 118, 119, 1110, 1111);
    testI12(121, 122, 123, 124, 125, 126, 127, 128, 129, 1210, 1211, 1212);
    new Main().testI6_nonstatic(61, 62, 63, 64, 65, 66);
  }

  static void testB() {
    testB1(true);
    testB2(true, false);
    testB3(true, false, true);
    testB4(true, false, true, false);
    testB5(true, false, true, false, true);
    testB6(true, false, true, false, true, false);
    testB7(true, false, true, false, true, false, true);
  }

  static void testO() {
    testO1("a");
    testO2("a", "b");
    testO3("a", "b", "c");
    testO4("a", "b", "c", "d");
    testO5("a", "b", "c", "d", "e");
    testO6("a", "b", "c", "d", "e", "f");
    testO7("a", "b", "c", "d", "e", "f", "g");
  }

  static void testIOB() {
    testIOB1(11);
    testIOB2(11, "b");
    testIOB3(11, "b", true);
    testIOB4(11, "b", true, 12);
    testIOB5(11, "b", true, 12, "e");
    testIOB6(11, "b", true, 12, "e", false);
    testIOB7(11, "b", true, 12, "e", false, 13);
  }

  static void testF() {
    testF1(1.1f);
    testF2(2.1f, 2.2f);
    testF3(3.1f, 3.2f, 3.3f);
    testF4(4.1f, 4.2f, 4.3f, 4.4f);
    testF5(5.1f, 5.2f, 5.3f, 5.4f, 5.5f);
    testF6(6.1f, 6.2f, 6.3f, 6.4f, 6.5f, 6.6f);
    testF7(7.1f, 7.2f, 7.3f, 7.4f, 7.5f, 7.6f, 7.7f);
    testF8(8.1f, 8.2f, 8.3f, 8.4f, 8.5f, 8.6f, 8.7f, 8.8f);
    testF9(9.1f, 9.2f, 9.3f, 9.4f, 9.5f, 9.6f, 9.7f, 9.8f, 9.9f);
    testF10(10.1f, 10.2f, 10.3f, 10.4f, 10.5f, 10.6f, 10.7f, 10.8f, 10.9f, 10.1f);
  }

  static void testD() {

    testD1(1.01);
    testD2(2.01, 2.02);
    testD3(3.01, 3.02, 3.03);
    testD4(4.01, 4.02, 4.03, 4.04);
    testD5(5.01, 5.02, 5.03, 5.04, 5.05);
    testD6(6.01, 6.02, 6.03, 6.04, 6.05, 6.06);
    testD7(7.01, 7.02, 7.03, 7.04, 7.05, 7.06, 7.07);
    testD8(8.01, 8.02, 8.03, 8.04, 8.05, 8.06, 8.07, 8.08);
    testD9(9.01, 9.02, 9.03, 9.04, 9.05, 9.06, 9.07, 9.08, 9.09);
    testD9f(-1.1f, 9.01, 9.02, 9.03, 9.04, 9.05, 9.06, 9.07, 9.08, 9.09);

    // TODO: 10.01 as first arg fails: 10.009994506835938
    testD10(10.01, 10.02, 10.03, 10.04, 10.05, 10.06, 10.07, 10.08, 10.09, 10.01);
  }

  static void testL1(long p1) { System.out.println(p1); }
//  static void testL2x(long p1, long p2) { testL2(p1+p2, p2); }  // TODO(64) GenAddLong 64BIT_TEMP
  static void testL2(long p1, long p2) { System.out.println(p1+", "+p2); }
  static void testL3(long p1, long p2, long p3) { System.out.println(p1+", "+p2+", "+p3); }
  static void testL4(long p1, long p2, long p3, long p4) { System.out.println(p1+", "+p2+", "+p3+", "+p4); }
  static void testL5(long p1, long p2, long p3, long p4, long p5) { System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5); }
  static void testL6(long p1, long p2, long p3, long p4, long p5, long p6) { System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6); }
  static void testL7(long p1, long p2, long p3, long p4, long p5, long p6, long p7) { System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7); }
  static void testL8(long p1, long p2, long p3, long p4, long p5, long p6, long p7, long p8) { System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8); }
  static void testL9(long p1, long p2, long p3, long p4, long p5, long p6, long p7, long p8, long p9) { System.out.println(p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8+", "+p9); }

  static void testL9i(int p0, long p1, long p2, long p3, long p4, long p5, long p6, long p7, long p8, long p9) { System.out.println(p0+", "+p1+", "+p2+", "+p3+", "+p4+", "+p5+", "+p6+", "+p7+", "+p8+", "+p9); }

  static void testL() {
//        testL2x(100021, 100022);
        testL1(100011);
        testL2(100021, 100022);
        testL3(100031, 100032, 100033);
        testL4(100041, 100042, 100043, 100044);
        testL5(100051, 100052, 100053, 100054, 100055);
        testL6(100061, 100062, 100063, 100064, 100065, 100066);
        testL7(100071, 100072, 100073, 100074, 100075, 100076, 100077);
        testL8(100081, 100082, 100083, 100084, 100085, 100086, 100087, 100088);
        testL9(100091, 100092, 100093, 100094, 100095, 100096, 100097, 100098, 100099);
  }

  static void testLL() {
        testL1(100100100100011L);

        testL1(-11L);
        testL2(-21L, -22L);
        testL3(-31L, -32L, -33L);
        testL4(-41L, -42L, -43L, -44L);
        testL5(-51L, -52L, -53L, -54L, -55L);
        testL6(-61L, -62L, -63L, -64L, -65L, -66L);
        testL7(-71L, -72L, -73L, -74L, -75L, -76L, -77L);
        testL8(-81L, -82L, -83L, -84L, -85L, -86L, -87L, -88L);
        testL9(-91L, -92L, -93L, -94L, -95L, -96L, -97L, -98L, -99L);
        testL9i(-1, -91L, -92L, -93L, -94L, -95L, -96L, -97L, -98L, -99L);

        // TODO(64) GenAddLong 64BIT_TEMP
//        testL2x(100100100100011L, 1L);
//        testL2x(100100100100011L, 100100100100011L);
  }

  static void testMore(int i1, double d1, double d2, double d3, double d4, double d5, double d6, double d7, double d8, double d9, int i2, int i3, int i4, int i5, int i6) {
    System.out.println(i1+", "+d1+", "+d2+", "+d3+", "+d4+", "+d5+", "+d6+", "+d7+", "+d8+", "+d9+", "+i2+", "+i3+", "+i4+", "+i5+", "+i6);
  }

  static void testRefs1(Object o1, Object o2, Object o3, Object o4, Object o5, long l1, long l2, long l3) {
    System.out.println(l1 + ", " + l2 + ", " + l3);
  }

  static void testRefs(Object o1, Object o2, Object o3, Object o4, Object o5, long l1, long l2, long l3) {
    testRefs1(o1, o2, o3, o4, o5, l1, l2, l3);
  }

  static public void main(String[] args) throws Exception {
    testI();
    testB();
    testO();
    testIOB();
    testF();

    testD();

    testL();

    testLL();

    testMore(1, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 2, 3, 4, 5, 6);

    Object obj = new Object();
    testRefs(obj, obj, obj, obj, obj, 0x1122334455667788L, 0x8877665544332211L, 0x1122334455667788L);
  }
}
