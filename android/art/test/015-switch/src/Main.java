/*
 * Copyright (C) 2007 The Android Open Source Project
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

/**
 * Test switch() blocks
 */
public class Main {

    // TODO: This should be translated to smali tests, so it is guaranteed we have the right kind
    //       of switch.

    // Simple packed-switch.
    public static void packedSwitch(int value) {
        switch (value) {
            case 0:
                System.out.println("0"); break;
            case 1:
                System.out.println("1"); break;
            case 2:
                System.out.println("2"); break;
            case 3:
                System.out.println("3"); break;
            case 4:
                System.out.println("4"); break;
            default:
                System.out.println("default"); break;
        }
    }

    // Simple packed-switch starting at a negative index.
    public static void packedSwitch2(int value) {
        switch (value) {
            case -3:
                System.out.println("-3"); break;
            case -2:
                System.out.println("-2"); break;
            case -1:
                System.out.println("-1"); break;
            case 0:
                System.out.println("0"); break;
            case 1:
                System.out.println("1"); break;
            case 2:
                System.out.println("2"); break;
            default:
                System.out.println("default"); break;
        }
    }

    // Simple packed-switch starting above 0.
    public static void packedSwitch3(int value) {
        switch (value) {
            case 2:
                System.out.println("2"); break;
            case 3:
                System.out.println("3"); break;
            case 4:
                System.out.println("4"); break;
            case 5:
                System.out.println("5"); break;
            case 6:
                System.out.println("6"); break;
            default:
                System.out.println("default"); break;
        }
    }

    // Simple packed-switch going up to max_int.
    public static void packedSwitch4(int value) {
        switch (value) {
            case Integer.MAX_VALUE - 1:
                System.out.println(Integer.MAX_VALUE - 1); break;
            case Integer.MAX_VALUE:
                System.out.println(Integer.MAX_VALUE); break;
            default:
                System.out.println("default"); break;
        }
    }

    // Simple packed-switch starting at min_int.
    public static void packedSwitch5(int value) {
        switch (value) {
            case Integer.MIN_VALUE:
                System.out.println(Integer.MIN_VALUE); break;
            case Integer.MIN_VALUE + 1:
                System.out.println(Integer.MIN_VALUE + 1); break;
            default:
                System.out.println("default"); break;
        }
    }

    // Simple (packed-)switch with only min_int.
    public static void packedSwitch6(int value) {
        switch (value) {
            case Integer.MIN_VALUE:
                System.out.println(Integer.MIN_VALUE); break;
            default:
                System.out.println("default"); break;
        }
    }

    // Long packed-switch that might lead to not creating chained-ifs.
    public static long packedSwitch7(int value) {
        switch (value) {
            case 1:
                System.out.println(1); break;
            case 2:
                System.out.println(2); break;
            case 3:
                System.out.println(3); break;
            case 4:
                System.out.println(4); break;
            case 5:
                System.out.println(5); break;
            case 6:
                System.out.println(6); break;
            case 7:
                System.out.println(7); break;
            case 8:
                System.out.println(8); break;
            case 9:
                System.out.println(9); break;
            case 10:
                System.out.println(10); break;
            case 11:
                System.out.println(11); break;
            case 12:
                System.out.println(12); break;
            case 13:
                System.out.println(13); break;
            case 14:
                System.out.println(14); break;
            case 15:
                System.out.println(15); break;
            default:
                System.out.println("default"); break;
        }

        // Jump tables previously were emitted in the end of the method code buffer. The
        // following boilerplate code aims to fill the emitted code buffer extensively
        // and check that even for big method jump table is correctly emitted, its address
        // is within a range of corresponded pc-relative instructions (this applies to
        // ARM mainly).
        long temp = value;
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);
        temp = Long.rotateLeft(temp, value);

        return temp;
    }

    // Sparse switch, just leave a gap.
    public static void sparseSwitch(int value) {
        switch (value) {
            case 0:
                System.out.println("0"); break;
            case 1:
                System.out.println("1"); break;
            case 3:
                System.out.println("3"); break;
            case 4:
                System.out.println("4"); break;
            default:
                System.out.println("default"); break;
        }
    }

    // Simple sparse-switch starting at a negative index.
    public static void sparseSwitch2(int value) {
        switch (value) {
            case -3:
                System.out.println("-3"); break;
            case -2:
                System.out.println("-2"); break;
            case -1:
                System.out.println("-1"); break;
            case 0:
                System.out.println("0"); break;
            case 2:
                System.out.println("2"); break;
            default:
                System.out.println("default"); break;
        }
    }

    // Simple sparse-switch starting above 0.
    public static void sparseSwitch3(int value) {
        switch (value) {
            case 2:
                System.out.println("2"); break;
            case 4:
                System.out.println("4"); break;
            case 5:
                System.out.println("5"); break;
            case 6:
                System.out.println("6"); break;
            default:
                System.out.println("default"); break;
        }
    }

    // Simple sparse-switch going up to max_int.
    public static void sparseSwitch4(int value) {
        switch (value) {
            case Integer.MAX_VALUE - 2:
                System.out.println(Integer.MAX_VALUE - 2); break;
            case Integer.MAX_VALUE:
                System.out.println(Integer.MAX_VALUE); break;
            default:
                System.out.println("default"); break;
        }
    }

    // Simple sparse-switch starting at min_int.
    public static void sparseSwitch5(int value) {
        switch (value) {
            case Integer.MIN_VALUE:
                System.out.println(Integer.MIN_VALUE); break;
            case Integer.MIN_VALUE + 2:
                System.out.println(Integer.MIN_VALUE + 2); break;
            default:
                System.out.println("default"); break;
        }
    }

    // Long sparse-switch that might lead to not creating chained-ifs.
    public static void sparseSwitch7(int value) {
        switch (value) {
            case 1:
                System.out.println(1); break;
            case 2:
                System.out.println(2); break;
            case 4:
                System.out.println(4); break;
            case 5:
                System.out.println(5); break;
            case 6:
                System.out.println(6); break;
            case 7:
                System.out.println(7); break;
            case 8:
                System.out.println(8); break;
            case 9:
                System.out.println(9); break;
            case 10:
                System.out.println(10); break;
            case 11:
                System.out.println(11); break;
            case 12:
                System.out.println(12); break;
            case 13:
                System.out.println(13); break;
            case 14:
                System.out.println(14); break;
            case 15:
                System.out.println(15); break;
            default:
                System.out.println("default"); break;
        }
    }

    public static void main(String args[]) {
        /*
         * Note: We are using for loops and calls to hopefully avoid simplifying the switch
         *       structure from constant propagation. When inlining is supported, this needs to
         *       be revisited.
         */

        System.out.println("packed");
        for (int i = -2; i < 3; i++) {
            packedSwitch(i);
        }
        packedSwitch(Integer.MIN_VALUE);
        packedSwitch(Integer.MAX_VALUE);

        System.out.println("packed2");
        for (int i = -2; i < 3; i++) {
            packedSwitch2(i);
        }
        packedSwitch2(Integer.MIN_VALUE);
        packedSwitch2(Integer.MAX_VALUE);

        System.out.println("packed3");
        for (int i = -2; i < 7; i++) {
            packedSwitch3(i);
        }
        packedSwitch3(Integer.MIN_VALUE);
        packedSwitch3(Integer.MAX_VALUE);

        System.out.println("packed4");
        for (int i = Integer.MAX_VALUE - 2; i > 0; i++) {
            packedSwitch4(i);
        }
        packedSwitch4(Integer.MIN_VALUE);

        System.out.println("packed5");
        for (int i = Integer.MIN_VALUE; i < Integer.MIN_VALUE + 2; i++) {
            packedSwitch5(i);
        }
        packedSwitch5(Integer.MAX_VALUE);

        System.out.println("packed6");
        packedSwitch6(Integer.MIN_VALUE);
        packedSwitch6(Integer.MAX_VALUE);

        System.out.println("packed7");
        for (int i = -1; i < 17; i++) {
            packedSwitch7(i);
        }


        System.out.println("sparse");
        for (int i = -2; i < 4; i++) {
            sparseSwitch(i);
        }
        sparseSwitch(Integer.MIN_VALUE);
        sparseSwitch(Integer.MAX_VALUE);

        System.out.println("sparse2");
        for (int i = -2; i < 3; i++) {
            sparseSwitch2(i);
        }
        sparseSwitch2(Integer.MIN_VALUE);
        sparseSwitch2(Integer.MAX_VALUE);

        System.out.println("sparse3");
        for (int i = -2; i < 7; i++) {
            sparseSwitch3(i);
        }
        sparseSwitch3(Integer.MIN_VALUE);
        sparseSwitch3(Integer.MAX_VALUE);

        System.out.println("sparse4");
        for (int i = Integer.MAX_VALUE - 2; i > 0; i++) {
            sparseSwitch4(i);
        }
        sparseSwitch4(Integer.MIN_VALUE);

        System.out.println("sparse5");
        for (int i = Integer.MIN_VALUE; i < Integer.MIN_VALUE + 2; i++) {
            sparseSwitch5(i);
        }
        sparseSwitch5(Integer.MAX_VALUE);

        System.out.println("sparse7");
        for (int i = -1; i < 17; i++) {
            sparseSwitch7(i);
        }

        // Older tests.

        int a = 1;

        switch (a) {
            case -1: System.out.print("neg one\n"); break;
            case 0: System.out.print("zero\n"); break;
            case 1: System.out.print("CORRECT (one)\n"); break;
            case 2: System.out.print("two\n"); break;
            case 3: System.out.print("three\n"); break;
            case 4: System.out.print("four\n"); break;
            default: System.out.print("???\n"); break;
        }
        switch (a) {
            case 3: System.out.print("three\n"); break;
            case 4: System.out.print("four\n"); break;
            default: System.out.print("CORRECT (not found)\n"); break;
        }

        a = 0x12345678;

        switch (a) {
            case 0x12345678: System.out.print("CORRECT (large)\n"); break;
            case 0x12345679: System.out.print("large+1\n"); break;
            default: System.out.print("nuts\n"); break;
        }
        switch (a) {
            case 0x12345678: System.out.print("CORRECT (large2)\n"); break;
            case 0x12345700: System.out.print("large+many\n"); break;
            default: System.out.print("nuts\n"); break;
        }
        switch (a) {
            case 57: System.out.print("fifty-seven!\n"); break;
            case -6: System.out.print("neg six!\n"); break;
            case 0x12345678: System.out.print("CORRECT (large3)\n"); break;
            case 22: System.out.print("twenty-two!\n"); break;
            case 3: System.out.print("three!\n"); break;
            default: System.out.print("huh?\n"); break;
        }
        switch (a) {
            case -6: System.out.print("neg six!\n"); break;
            case 3: System.out.print("three!\n"); break;
            default: System.out.print("CORRECT (not found)\n"); break;
        }

        a = -5;
        switch (a) {
            case 12: System.out.print("twelve\n"); break;
            case -5: System.out.print("CORRECT (not found)\n"); break;
            case 0: System.out.print("zero\n"); break;
            default: System.out.print("wah?\n"); break;
        }

        switch (a) {
            default: System.out.print("CORRECT (default only)\n"); break;
        }

        a = -10;
        switch (a) {
            case -10: System.out.print("CORRECT big sparse / first\n"); break;
            case -5: System.out.print("neg five\n"); break;
            case 0: System.out.print("zero\n"); break;
            case 5: System.out.print("five\n"); break;
            case 10: System.out.print("ten\n"); break;
            case 15: System.out.print("fifteen\n"); break;
            case 20: System.out.print("twenty\n"); break;
            case 50: System.out.print("fifty\n"); break;
            case 100: System.out.print("hundred\n"); break;
            default: System.out.print("blah!\n"); break;
        }

        a = 100;
        switch (a) {
            case -10: System.out.print("neg ten\n"); break;
            case -5: System.out.print("neg five\n"); break;
            case 0: System.out.print("zero\n"); break;
            case 5: System.out.print("five\n"); break;
            case 10: System.out.print("ten\n"); break;
            case 15: System.out.print("fifteen\n"); break;
            case 20: System.out.print("twenty\n"); break;
            case 50: System.out.print("fifty\n"); break;
            case 100: System.out.print("CORRECT big sparse / last\n"); break;
            default: System.out.print("blah!\n"); break;
        }

        for (a = 253; a <= 258; a++) {
          switch (a) {
            case 254: System.out.println("254"); break;
            case 255: System.out.println("255"); break;
            case 256: System.out.println("256"); break;
            case 257: System.out.println("257"); break;
            default: System.out.println("default"); break;
          }
        }
    }
}
