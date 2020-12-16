/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

interface Itf {
  public Class<?> method1();
  public Class<?> method2();
  public Class<?> method3();
  public Class<?> method4();
  public Class<?> method5();
  public Class<?> method6();
  public Class<?> method7();
  public Class<?> method8();
  public Class<?> method9();
  public Class<?> method10();
  public Class<?> method11();
  public Class<?> method12();
  public Class<?> method13();
  public Class<?> method14();
  public Class<?> method15();
  public Class<?> method16();
  public Class<?> method17();
  public Class<?> method18();
  public Class<?> method19();
  public Class<?> method20();
  public Class<?> method21();
  public Class<?> method22();
  public Class<?> method23();
  public Class<?> method24();
  public Class<?> method25();
  public Class<?> method26();
  public Class<?> method27();
  public Class<?> method28();
  public Class<?> method29();
  public Class<?> method30();
  public Class<?> method31();
  public Class<?> method32();
  public Class<?> method33();
  public Class<?> method34();
  public Class<?> method35();
  public Class<?> method36();
  public Class<?> method37();
  public Class<?> method38();
  public Class<?> method39();
  public Class<?> method40();
  public Class<?> method41();
  public Class<?> method42();
  public Class<?> method43();
  public Class<?> method44();
  public Class<?> method45();
  public Class<?> method46();
  public Class<?> method47();
  public Class<?> method48();
  public Class<?> method49();
  public Class<?> method50();
  public Class<?> method51();
  public Class<?> method52();
  public Class<?> method53();
  public Class<?> method54();
  public Class<?> method55();
  public Class<?> method56();
  public Class<?> method57();
  public Class<?> method58();
  public Class<?> method59();
  public Class<?> method60();
  public Class<?> method61();
  public Class<?> method62();
  public Class<?> method63();
  public Class<?> method64();
  public Class<?> method65();
  public Class<?> method66();
  public Class<?> method67();
  public Class<?> method68();
  public Class<?> method69();
  public Class<?> method70();
  public Class<?> method71();
  public Class<?> method72();
  public Class<?> method73();
  public Class<?> method74();
  public Class<?> method75();
  public Class<?> method76();
  public Class<?> method77();
  public Class<?> method78();
  public Class<?> method79();
}

public class Main implements Itf {
  public static Itf main;
  public static void main(String[] args) {
    main = new Main();
    callMains();
    main = new SubMain();
    callSubMains();
  }

  public static void callMains() {
    // We loop to artificially create branches. The compiler will
    // not compile this method otherwise.
    for (int i = 0; i < 2; ++i) {
      expectEquals(main.method1(), Main.class);
      expectEquals(main.method2(), Main.class);
      expectEquals(main.method3(), Main.class);
      expectEquals(main.method4(), Main.class);
      expectEquals(main.method5(), Main.class);
      expectEquals(main.method6(), Main.class);
      expectEquals(main.method7(), Main.class);
      expectEquals(main.method8(), Main.class);
      expectEquals(main.method9(), Main.class);
      expectEquals(main.method10(), Main.class);
      expectEquals(main.method11(), Main.class);
      expectEquals(main.method12(), Main.class);
      expectEquals(main.method13(), Main.class);
      expectEquals(main.method14(), Main.class);
      expectEquals(main.method15(), Main.class);
      expectEquals(main.method16(), Main.class);
      expectEquals(main.method17(), Main.class);
      expectEquals(main.method18(), Main.class);
      expectEquals(main.method19(), Main.class);
      expectEquals(main.method20(), Main.class);
      expectEquals(main.method21(), Main.class);
      expectEquals(main.method22(), Main.class);
      expectEquals(main.method23(), Main.class);
      expectEquals(main.method24(), Main.class);
      expectEquals(main.method25(), Main.class);
      expectEquals(main.method26(), Main.class);
      expectEquals(main.method27(), Main.class);
      expectEquals(main.method28(), Main.class);
      expectEquals(main.method29(), Main.class);
      expectEquals(main.method30(), Main.class);
      expectEquals(main.method31(), Main.class);
      expectEquals(main.method32(), Main.class);
      expectEquals(main.method33(), Main.class);
      expectEquals(main.method34(), Main.class);
      expectEquals(main.method35(), Main.class);
      expectEquals(main.method36(), Main.class);
      expectEquals(main.method37(), Main.class);
      expectEquals(main.method38(), Main.class);
      expectEquals(main.method39(), Main.class);
      expectEquals(main.method40(), Main.class);
      expectEquals(main.method41(), Main.class);
      expectEquals(main.method42(), Main.class);
      expectEquals(main.method43(), Main.class);
      expectEquals(main.method44(), Main.class);
      expectEquals(main.method45(), Main.class);
      expectEquals(main.method46(), Main.class);
      expectEquals(main.method47(), Main.class);
      expectEquals(main.method48(), Main.class);
      expectEquals(main.method49(), Main.class);
      expectEquals(main.method50(), Main.class);
      expectEquals(main.method51(), Main.class);
      expectEquals(main.method52(), Main.class);
      expectEquals(main.method53(), Main.class);
      expectEquals(main.method54(), Main.class);
      expectEquals(main.method55(), Main.class);
      expectEquals(main.method56(), Main.class);
      expectEquals(main.method57(), Main.class);
      expectEquals(main.method58(), Main.class);
      expectEquals(main.method59(), Main.class);
      expectEquals(main.method60(), Main.class);
      expectEquals(main.method61(), Main.class);
      expectEquals(main.method62(), Main.class);
      expectEquals(main.method63(), Main.class);
      expectEquals(main.method64(), Main.class);
      expectEquals(main.method65(), Main.class);
      expectEquals(main.method66(), Main.class);
      expectEquals(main.method67(), Main.class);
      expectEquals(main.method68(), Main.class);
      expectEquals(main.method69(), Main.class);
      expectEquals(main.method70(), Main.class);
      expectEquals(main.method71(), Main.class);
      expectEquals(main.method72(), Main.class);
      expectEquals(main.method73(), Main.class);
      expectEquals(main.method74(), Main.class);
      expectEquals(main.method75(), Main.class);
      expectEquals(main.method76(), Main.class);
      expectEquals(main.method77(), Main.class);
      expectEquals(main.method78(), Main.class);
      expectEquals(main.method79(), Main.class);
    }
  }

  public static void callSubMains() {
    // We loop to artificially create branches. The compiler will
    // not compile this method otherwise.
    for (int i = 0; i < 2; ++i) {
      expectEquals(main.method1(), SubMain.class);
      expectEquals(main.method2(), SubMain.class);
      expectEquals(main.method3(), SubMain.class);
      expectEquals(main.method4(), SubMain.class);
      expectEquals(main.method5(), SubMain.class);
      expectEquals(main.method6(), SubMain.class);
      expectEquals(main.method7(), SubMain.class);
      expectEquals(main.method8(), SubMain.class);
      expectEquals(main.method9(), SubMain.class);
      expectEquals(main.method10(), SubMain.class);
      expectEquals(main.method11(), SubMain.class);
      expectEquals(main.method12(), SubMain.class);
      expectEquals(main.method13(), SubMain.class);
      expectEquals(main.method14(), SubMain.class);
      expectEquals(main.method15(), SubMain.class);
      expectEquals(main.method16(), SubMain.class);
      expectEquals(main.method17(), SubMain.class);
      expectEquals(main.method18(), SubMain.class);
      expectEquals(main.method19(), SubMain.class);
      expectEquals(main.method20(), SubMain.class);
      expectEquals(main.method21(), SubMain.class);
      expectEquals(main.method22(), SubMain.class);
      expectEquals(main.method23(), SubMain.class);
      expectEquals(main.method24(), SubMain.class);
      expectEquals(main.method25(), SubMain.class);
      expectEquals(main.method26(), SubMain.class);
      expectEquals(main.method27(), SubMain.class);
      expectEquals(main.method28(), SubMain.class);
      expectEquals(main.method29(), SubMain.class);
      expectEquals(main.method30(), SubMain.class);
      expectEquals(main.method31(), SubMain.class);
      expectEquals(main.method32(), SubMain.class);
      expectEquals(main.method33(), SubMain.class);
      expectEquals(main.method34(), SubMain.class);
      expectEquals(main.method35(), SubMain.class);
      expectEquals(main.method36(), SubMain.class);
      expectEquals(main.method37(), SubMain.class);
      expectEquals(main.method38(), SubMain.class);
      expectEquals(main.method39(), SubMain.class);
      expectEquals(main.method40(), SubMain.class);
      expectEquals(main.method41(), SubMain.class);
      expectEquals(main.method42(), SubMain.class);
      expectEquals(main.method43(), SubMain.class);
      expectEquals(main.method44(), SubMain.class);
      expectEquals(main.method45(), SubMain.class);
      expectEquals(main.method46(), SubMain.class);
      expectEquals(main.method47(), SubMain.class);
      expectEquals(main.method48(), SubMain.class);
      expectEquals(main.method49(), SubMain.class);
      expectEquals(main.method50(), SubMain.class);
      expectEquals(main.method51(), SubMain.class);
      expectEquals(main.method52(), SubMain.class);
      expectEquals(main.method53(), SubMain.class);
      expectEquals(main.method54(), SubMain.class);
      expectEquals(main.method55(), SubMain.class);
      expectEquals(main.method56(), SubMain.class);
      expectEquals(main.method57(), SubMain.class);
      expectEquals(main.method58(), SubMain.class);
      expectEquals(main.method59(), SubMain.class);
      expectEquals(main.method60(), SubMain.class);
      expectEquals(main.method61(), SubMain.class);
      expectEquals(main.method62(), SubMain.class);
      expectEquals(main.method63(), SubMain.class);
      expectEquals(main.method64(), SubMain.class);
      expectEquals(main.method65(), SubMain.class);
      expectEquals(main.method66(), SubMain.class);
      expectEquals(main.method67(), SubMain.class);
      expectEquals(main.method68(), SubMain.class);
      expectEquals(main.method69(), SubMain.class);
      expectEquals(main.method70(), SubMain.class);
      expectEquals(main.method71(), SubMain.class);
      expectEquals(main.method72(), SubMain.class);
      expectEquals(main.method73(), SubMain.class);
      expectEquals(main.method74(), SubMain.class);
      expectEquals(main.method75(), SubMain.class);
      expectEquals(main.method76(), SubMain.class);
      expectEquals(main.method77(), SubMain.class);
      expectEquals(main.method78(), SubMain.class);
      expectEquals(main.method79(), SubMain.class);
    }
  }

  public static void expectEquals(Object actual, Object expected) {
    if (!actual.equals(expected)) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public Class<?> method1() { return Main.class; }
  public Class<?> method2() { return Main.class; }
  public Class<?> method3() { return Main.class; }
  public Class<?> method4() { return Main.class; }
  public Class<?> method5() { return Main.class; }
  public Class<?> method6() { return Main.class; }
  public Class<?> method7() { return Main.class; }
  public Class<?> method8() { return Main.class; }
  public Class<?> method9() { return Main.class; }
  public Class<?> method10() { return Main.class; }
  public Class<?> method11() { return Main.class; }
  public Class<?> method12() { return Main.class; }
  public Class<?> method13() { return Main.class; }
  public Class<?> method14() { return Main.class; }
  public Class<?> method15() { return Main.class; }
  public Class<?> method16() { return Main.class; }
  public Class<?> method17() { return Main.class; }
  public Class<?> method18() { return Main.class; }
  public Class<?> method19() { return Main.class; }
  public Class<?> method20() { return Main.class; }
  public Class<?> method21() { return Main.class; }
  public Class<?> method22() { return Main.class; }
  public Class<?> method23() { return Main.class; }
  public Class<?> method24() { return Main.class; }
  public Class<?> method25() { return Main.class; }
  public Class<?> method26() { return Main.class; }
  public Class<?> method27() { return Main.class; }
  public Class<?> method28() { return Main.class; }
  public Class<?> method29() { return Main.class; }
  public Class<?> method30() { return Main.class; }
  public Class<?> method31() { return Main.class; }
  public Class<?> method32() { return Main.class; }
  public Class<?> method33() { return Main.class; }
  public Class<?> method34() { return Main.class; }
  public Class<?> method35() { return Main.class; }
  public Class<?> method36() { return Main.class; }
  public Class<?> method37() { return Main.class; }
  public Class<?> method38() { return Main.class; }
  public Class<?> method39() { return Main.class; }
  public Class<?> method40() { return Main.class; }
  public Class<?> method41() { return Main.class; }
  public Class<?> method42() { return Main.class; }
  public Class<?> method43() { return Main.class; }
  public Class<?> method44() { return Main.class; }
  public Class<?> method45() { return Main.class; }
  public Class<?> method46() { return Main.class; }
  public Class<?> method47() { return Main.class; }
  public Class<?> method48() { return Main.class; }
  public Class<?> method49() { return Main.class; }
  public Class<?> method50() { return Main.class; }
  public Class<?> method51() { return Main.class; }
  public Class<?> method52() { return Main.class; }
  public Class<?> method53() { return Main.class; }
  public Class<?> method54() { return Main.class; }
  public Class<?> method55() { return Main.class; }
  public Class<?> method56() { return Main.class; }
  public Class<?> method57() { return Main.class; }
  public Class<?> method58() { return Main.class; }
  public Class<?> method59() { return Main.class; }
  public Class<?> method60() { return Main.class; }
  public Class<?> method61() { return Main.class; }
  public Class<?> method62() { return Main.class; }
  public Class<?> method63() { return Main.class; }
  public Class<?> method64() { return Main.class; }
  public Class<?> method65() { return Main.class; }
  public Class<?> method66() { return Main.class; }
  public Class<?> method67() { return Main.class; }
  public Class<?> method68() { return Main.class; }
  public Class<?> method69() { return Main.class; }
  public Class<?> method70() { return Main.class; }
  public Class<?> method71() { return Main.class; }
  public Class<?> method72() { return Main.class; }
  public Class<?> method73() { return Main.class; }
  public Class<?> method74() { return Main.class; }
  public Class<?> method75() { return Main.class; }
  public Class<?> method76() { return Main.class; }
  public Class<?> method77() { return Main.class; }
  public Class<?> method78() { return Main.class; }
  public Class<?> method79() { return Main.class; }
}

class SubMain extends Main {
  public Class<?> method1() { return SubMain.class; }
  public Class<?> method2() { return SubMain.class; }
  public Class<?> method3() { return SubMain.class; }
  public Class<?> method4() { return SubMain.class; }
  public Class<?> method5() { return SubMain.class; }
  public Class<?> method6() { return SubMain.class; }
  public Class<?> method7() { return SubMain.class; }
  public Class<?> method8() { return SubMain.class; }
  public Class<?> method9() { return SubMain.class; }
  public Class<?> method10() { return SubMain.class; }
  public Class<?> method11() { return SubMain.class; }
  public Class<?> method12() { return SubMain.class; }
  public Class<?> method13() { return SubMain.class; }
  public Class<?> method14() { return SubMain.class; }
  public Class<?> method15() { return SubMain.class; }
  public Class<?> method16() { return SubMain.class; }
  public Class<?> method17() { return SubMain.class; }
  public Class<?> method18() { return SubMain.class; }
  public Class<?> method19() { return SubMain.class; }
  public Class<?> method20() { return SubMain.class; }
  public Class<?> method21() { return SubMain.class; }
  public Class<?> method22() { return SubMain.class; }
  public Class<?> method23() { return SubMain.class; }
  public Class<?> method24() { return SubMain.class; }
  public Class<?> method25() { return SubMain.class; }
  public Class<?> method26() { return SubMain.class; }
  public Class<?> method27() { return SubMain.class; }
  public Class<?> method28() { return SubMain.class; }
  public Class<?> method29() { return SubMain.class; }
  public Class<?> method30() { return SubMain.class; }
  public Class<?> method31() { return SubMain.class; }
  public Class<?> method32() { return SubMain.class; }
  public Class<?> method33() { return SubMain.class; }
  public Class<?> method34() { return SubMain.class; }
  public Class<?> method35() { return SubMain.class; }
  public Class<?> method36() { return SubMain.class; }
  public Class<?> method37() { return SubMain.class; }
  public Class<?> method38() { return SubMain.class; }
  public Class<?> method39() { return SubMain.class; }
  public Class<?> method40() { return SubMain.class; }
  public Class<?> method41() { return SubMain.class; }
  public Class<?> method42() { return SubMain.class; }
  public Class<?> method43() { return SubMain.class; }
  public Class<?> method44() { return SubMain.class; }
  public Class<?> method45() { return SubMain.class; }
  public Class<?> method46() { return SubMain.class; }
  public Class<?> method47() { return SubMain.class; }
  public Class<?> method48() { return SubMain.class; }
  public Class<?> method49() { return SubMain.class; }
  public Class<?> method50() { return SubMain.class; }
  public Class<?> method51() { return SubMain.class; }
  public Class<?> method52() { return SubMain.class; }
  public Class<?> method53() { return SubMain.class; }
  public Class<?> method54() { return SubMain.class; }
  public Class<?> method55() { return SubMain.class; }
  public Class<?> method56() { return SubMain.class; }
  public Class<?> method57() { return SubMain.class; }
  public Class<?> method58() { return SubMain.class; }
  public Class<?> method59() { return SubMain.class; }
  public Class<?> method60() { return SubMain.class; }
  public Class<?> method61() { return SubMain.class; }
  public Class<?> method62() { return SubMain.class; }
  public Class<?> method63() { return SubMain.class; }
  public Class<?> method64() { return SubMain.class; }
  public Class<?> method65() { return SubMain.class; }
  public Class<?> method66() { return SubMain.class; }
  public Class<?> method67() { return SubMain.class; }
  public Class<?> method68() { return SubMain.class; }
  public Class<?> method69() { return SubMain.class; }
  public Class<?> method70() { return SubMain.class; }
  public Class<?> method71() { return SubMain.class; }
  public Class<?> method72() { return SubMain.class; }
  public Class<?> method73() { return SubMain.class; }
  public Class<?> method74() { return SubMain.class; }
  public Class<?> method75() { return SubMain.class; }
  public Class<?> method76() { return SubMain.class; }
  public Class<?> method77() { return SubMain.class; }
  public Class<?> method78() { return SubMain.class; }
  public Class<?> method79() { return SubMain.class; }
}
