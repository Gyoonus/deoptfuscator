
public class Main {
    static class SuperClass {
      protected static int getVar(int w) {
          return w & 0xF;
      }
    }
    static class SubClass extends SuperClass {
      final int getVarDirect(int w) {
        return w & 0xF;
      }
      public void testDirect(int max) {
        for (int i = 0; i < max; ++i) {
          getVarDirect(max);
        }
      }
      public void testStatic(int max) {
        for (int i = 0; i < max; ++i) {
          getVar(max);
        }
      }
    }

    static public void main(String[] args) throws Exception {
        boolean timing = (args.length >= 1) && args[0].equals("--timing");
        run(timing);
    }

    static int testBasis(int iterations) {
      (new SubClass()).testDirect(iterations);
      return iterations;
    }

    static int testStatic(int iterations) {
      (new SubClass()).testStatic(iterations);
      return iterations;
    }

    static public void run(boolean timing) {
        long time0 = System.nanoTime();
        int count1 = testBasis(50000000);
        long time1 = System.nanoTime();
        int count2 = testStatic(50000000);
        long time2 = System.nanoTime();

        System.out.println("basis: performed " + count1 + " iterations");
        System.out.println("test1: performed " + count2 + " iterations");

        double basisMsec = (time1 - time0) / (double) count1 / 1000000;
        double msec1 = (time2 - time1) / (double) count2 / 1000000;

        if (msec1 < basisMsec * 5) {
            System.out.println("Timing is acceptable.");
        } else {
            System.out.println("Iterations are taking too long!");
            timing = true;
        }
        if (timing) {
            System.out.printf("basis time: %.3g msec\n", basisMsec);
            System.out.printf("test1: %.3g msec per iteration\n", msec1);
        }
    }
}
