/*
 * Copyright (C) 2017 The Android Open Source Project
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

package art;

import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Base64;
import java.util.HashSet;
import java.util.Set;

public class Test1911 {
  // Class/dex file containing the following class.
  //
  // CLASS_BYTES generated with java version 1.8.0_45: javac -g art/Target.java
  // DEX_BYTES generated with dx version 1.14: dx --dex --output=./classes.dex art/Target.class
  //
  // package art;
  // import java.util.ArrayList;
  // public class Target {
  //   public int zzz;
  //   public Target(int xxx) {
  //     int q = xxx * 4;
  //     zzz = q;
  //   }
  //   public static void doNothing(Object... objs) { doNothing(objs); }
  //   public void doSomething(int x) {
  //     doNothing(this);
  //     int y = x + 3;
  //     for (int z = 0; z < y * x; z++) {
  //       float q = y - z;
  //       double i = 0.3d * q;
  //       doNothing(q, i);
  //     }
  //     Object o = new Object();
  //     ArrayList<Integer> i = new ArrayList<>();
  //     int p = 4 | x;
  //     long q = 3 * p;
  //     doNothing(p, q, o, i);
  //   }
  // }
  public static byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQARgoABAAuCQANAC8KAA0AMAcAMQY/0zMzMzMzMwoAMgAzCgA0ADUHADYKAAkALgoA" +
    "NwA4CgA5ADoHADsBAAN6enoBAAFJAQAGPGluaXQ+AQAEKEkpVgEABENvZGUBAA9MaW5lTnVtYmVy" +
    "VGFibGUBABJMb2NhbFZhcmlhYmxlVGFibGUBAAR0aGlzAQAMTGFydC9UYXJnZXQ7AQADeHh4AQAB" +
    "cQEACWRvTm90aGluZwEAFihbTGphdmEvbGFuZy9PYmplY3Q7KVYBAARvYmpzAQATW0xqYXZhL2xh" +
    "bmcvT2JqZWN0OwEAC2RvU29tZXRoaW5nAQABRgEAAWkBAAFEAQABegEAAXgBAAF5AQABbwEAEkxq" +
    "YXZhL2xhbmcvT2JqZWN0OwEAFUxqYXZhL3V0aWwvQXJyYXlMaXN0OwEAAXABAAFKAQAWTG9jYWxW" +
    "YXJpYWJsZVR5cGVUYWJsZQEAKkxqYXZhL3V0aWwvQXJyYXlMaXN0PExqYXZhL2xhbmcvSW50ZWdl" +
    "cjs+OwEADVN0YWNrTWFwVGFibGUBAApTb3VyY2VGaWxlAQALVGFyZ2V0LmphdmEMABAAPAwADgAP" +
    "DAAZABoBABBqYXZhL2xhbmcvT2JqZWN0BwA9DAA+AD8HAEAMAD4AQQEAE2phdmEvdXRpbC9BcnJh" +
    "eUxpc3QHAEIMAD4AQwcARAwAPgBFAQAKYXJ0L1RhcmdldAEAAygpVgEAD2phdmEvbGFuZy9GbG9h" +
    "dAEAB3ZhbHVlT2YBABQoRilMamF2YS9sYW5nL0Zsb2F0OwEAEGphdmEvbGFuZy9Eb3VibGUBABUo" +
    "RClMamF2YS9sYW5nL0RvdWJsZTsBABFqYXZhL2xhbmcvSW50ZWdlcgEAFihJKUxqYXZhL2xhbmcv" +
    "SW50ZWdlcjsBAA5qYXZhL2xhbmcvTG9uZwEAEyhKKUxqYXZhL2xhbmcvTG9uZzsAIQANAAQAAAAB" +
    "AAEADgAPAAAAAwABABAAEQABABIAAABYAAIAAwAAAA4qtwABGwdoPSoctQACsQAAAAIAEwAAABIA" +
    "BAAAAAUABAAGAAgABwANAAgAFAAAACAAAwAAAA4AFQAWAAAAAAAOABcADwABAAgABgAYAA8AAgCJ" +
    "ABkAGgABABIAAAAvAAEAAQAAAAUquAADsQAAAAIAEwAAAAYAAQAAAAkAFAAAAAwAAQAAAAUAGwAc" +
    "AAAAAQAdABEAAQASAAABWAAFAAgAAACCBL0ABFkDKlO4AAMbBmA9Az4dHBtoogAvHB1khjgEFAAF" +
    "FwSNazkFBb0ABFkDFwS4AAdTWQQYBbgACFO4AAOEAwGn/9C7AARZtwABTrsACVm3AAo6BAcbgDYF" +
    "BhUFaIU3Bge9AARZAxUFuAALU1kEFga4AAxTWQUtU1kGGQRTuAADsQAAAAQAEwAAADYADQAAAAsA" +
    "CwAMAA8ADQAYAA4AHgAPACcAEAA+AA0ARAASAEwAEwBVABQAWgAVAGEAFgCBABcAFAAAAGYACgAe" +
    "ACAAGAAeAAQAJwAXAB8AIAAFABEAMwAhAA8AAwAAAIIAFQAWAAAAAACCACIADwABAA8AcwAjAA8A" +
    "AgBMADYAJAAlAAMAVQAtAB8AJgAEAFoAKAAnAA8ABQBhACEAGAAoAAYAKQAAAAwAAQBVAC0AHwAq" +
    "AAQAKwAAAAoAAv0AEQEB+gAyAAEALAAAAAIALQ==");
  public static byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQCQtgjEV631Ma/btYyIy2IzqHWNN+nZiwl0BQAAcAAAAHhWNBIAAAAAAAAAANQEAAAk" +
    "AAAAcAAAAA0AAAAAAQAABwAAADQBAAABAAAAiAEAAAkAAACQAQAAAQAAANgBAAB8AwAA+AEAAB4D" +
    "AAAmAwAAKQMAACwDAAAvAwAAMgMAADYDAAA6AwAAPgMAAEIDAABQAwAAZAMAAHcDAACMAwAAngMA" +
    "ALIDAADJAwAA9QMAAAIEAAAFBAAACQQAAA0EAAAiBAAALQQAADoEAAA9BAAAQAQAAEYEAABJBAAA" +
    "TAQAAFIEAABbBAAAXgQAAGMEAABmBAAAaQQAAAEAAAACAAAAAwAAAAQAAAAJAAAACgAAAAsAAAAM" +
    "AAAADQAAAA4AAAAPAAAAEgAAABUAAAAFAAAABQAAAPgCAAAGAAAABgAAAAADAAAHAAAABwAAAAgD" +
    "AAAIAAAACAAAABADAAASAAAACwAAAAAAAAATAAAACwAAAAgDAAAUAAAACwAAABgDAAAEAAIAIwAA" +
    "AAQABQAAAAAABAAGABYAAAAEAAUAFwAAAAUAAAAeAAAABgABAB4AAAAHAAIAHgAAAAgAAwAeAAAA" +
    "CQAEAAAAAAAKAAQAAAAAAAQAAAABAAAACQAAAAAAAAARAAAAAAAAAL4EAAAAAAAAAwACAAEAAABu" +
    "BAAACAAAAHAQBwABANoAAgRZEAAADgABAAEAAQAAAHsEAAAEAAAAcRABAAAADgAQAAIAAgAAAIEE" +
    "AABcAAAAEhkjmQwAEgpNDgkKcRABAAkA2AUPAxIIkgkFDzWYJACRCQUIgpYYCjMzMzMzM9M/iWyt" +
    "AAoMEikjmQwAEgpxEAQABgAMC00LCQoSGnEgAwAQAAwLTQsJCnEQAQAJANgICAEo2yIDCQBwEAcA" +
    "AwAiAgoAcBAIAAIA3gQPBNoJBAOBlhJJI5kMABIKcRAFAAQADAtNCwkKEhpxIAYAdgAMC00LCQoS" +
    "Kk0DCQoSOk0CCQpxEAEACQAOAAEAAAAAAAAAAQAAAAEAAAABAAAAAgAAAAEAAAADAAAAAQAAAAwA" +
    "Bjxpbml0PgABRAABRgABSQABSgACTEQAAkxGAAJMSQACTEoADExhcnQvVGFyZ2V0OwASTGphdmEv" +
    "bGFuZy9Eb3VibGU7ABFMamF2YS9sYW5nL0Zsb2F0OwATTGphdmEvbGFuZy9JbnRlZ2VyOwAQTGph" +
    "dmEvbGFuZy9Mb25nOwASTGphdmEvbGFuZy9PYmplY3Q7ABVMamF2YS91dGlsL0FycmF5TGlzdDsA" +
    "KkxqYXZhL3V0aWwvQXJyYXlMaXN0PExqYXZhL2xhbmcvSW50ZWdlcjs+OwALVGFyZ2V0LmphdmEA" +
    "AVYAAlZJAAJWTAATW0xqYXZhL2xhbmcvT2JqZWN0OwAJZG9Ob3RoaW5nAAtkb1NvbWV0aGluZwAB" +
    "aQABbwAEb2JqcwABcAABcQAEdGhpcwAHdmFsdWVPZgABeAADeHh4AAF5AAF6AAN6enoABQEhBw48" +
    "LQMAHQMtAAkBGwcOAAsBIAcOli0DBSIDAQEDCCMDSzwDBh0ChwMAGQEBFAtABQAFBloDAxoKWgQC" +
    "GQsRLQMEHAM8AwYdBAEaDwAAAQIBAAEAgYAE+AMBiQGYBAIBsAQADQAAAAAAAAABAAAAAAAAAAEA" +
    "AAAkAAAAcAAAAAIAAAANAAAAAAEAAAMAAAAHAAAANAEAAAQAAAABAAAAiAEAAAUAAAAJAAAAkAEA" +
    "AAYAAAABAAAA2AEAAAEgAAADAAAA+AEAAAEQAAAFAAAA+AIAAAIgAAAkAAAAHgMAAAMgAAADAAAA" +
    "bgQAAAAgAAABAAAAvgQAAAAQAAABAAAA1AQAAA==");


  // The variables of the functions in the above Target class.
  public static Set<Locals.VariableDescription>[] CONSTRUCTOR_VARIABLES = new Set[] {
      // RI Local variable table
      new HashSet<>(Arrays.asList(
              new Locals.VariableDescription(8, 6, "q", "I", null, 2),
              new Locals.VariableDescription(0, 14, "xxx", "I", null, 1),
              new Locals.VariableDescription(0, 14, "this", "Lart/Target;", null, 0))),
      // ART Local variable table
      new HashSet<>(Arrays.asList(
              new Locals.VariableDescription(0, 8, "this", "Lart/Target;", null, 1),
              new Locals.VariableDescription(5, 3, "q", "I", null, 0),
              new Locals.VariableDescription(0, 8, "xxx", "I", null, 2))),
  };

  public static Set<Locals.VariableDescription>[] DO_NOTHING_VARIABLES = new Set[] {
      // RI Local variable table
      new HashSet<>(Arrays.asList(
              new Locals.VariableDescription(0, 5, "objs", "[Ljava/lang/Object;", null, 0))),
      // ART Local variable table
      new HashSet<>(Arrays.asList(
              new Locals.VariableDescription(0, 4, "objs", "[Ljava/lang/Object;", null, 0))),
  };

  public static Set<Locals.VariableDescription>[] DO_SOMETHING_VARIABLES = new Set[] {
      // RI Local variable table
      new HashSet<>(Arrays.asList(
              new Locals.VariableDescription(0, 130, "x", "I", null, 1),
              new Locals.VariableDescription(76, 54, "o", "Ljava/lang/Object;", null, 3),
              new Locals.VariableDescription(30, 32, "q", "F", null, 4),
              new Locals.VariableDescription(39, 23, "i", "D", null, 5),
              new Locals.VariableDescription(17, 51, "z", "I", null, 3),
              new Locals.VariableDescription(15, 115, "y", "I", null, 2),
              new Locals.VariableDescription(90, 40, "p", "I", null, 5),
              new Locals.VariableDescription(97, 33, "q", "J", null, 6),
              new Locals.VariableDescription(0, 130, "this", "Lart/Target;", null, 0),
              new Locals.VariableDescription(85,
                                             45,
                                             "i",
                                             "Ljava/util/ArrayList;",
                                             "Ljava/util/ArrayList<Ljava/lang/Integer;>;",
                                             4))),
      // ART Local variable table
      new HashSet<>(Arrays.asList(
              new Locals.VariableDescription(19, 31, "q", "F", null, 6),
              new Locals.VariableDescription(55, 37, "o", "Ljava/lang/Object;", null, 3),
              new Locals.VariableDescription(0, 92, "this", "Lart/Target;", null, 14),
              new Locals.VariableDescription(12, 80, "z", "I", null, 8),
              new Locals.VariableDescription(11, 81, "y", "I", null, 5),
              new Locals.VariableDescription(62, 30, "p", "I", null, 4),
              new Locals.VariableDescription(0, 92, "x", "I", null, 15),
              new Locals.VariableDescription(27, 23, "i", "D", null, 0),
              new Locals.VariableDescription(65, 27, "q", "J", null, 6),
              new Locals.VariableDescription(60,
                                             32,
                                             "i",
                                             "Ljava/util/ArrayList;",
                                             "Ljava/util/ArrayList<Ljava/lang/Integer;>;",
                                             2))),
  };

  // Get a classloader that can load the Target class.
  public static ClassLoader getClassLoader() throws Exception {
    try {
      Class<?> class_loader_class = Class.forName("dalvik.system.InMemoryDexClassLoader");
      Constructor<?> ctor = class_loader_class.getConstructor(ByteBuffer.class, ClassLoader.class);
      // We are on art since we got the InMemoryDexClassLoader.
      return (ClassLoader)ctor.newInstance(
          ByteBuffer.wrap(DEX_BYTES), Test1911.class.getClassLoader());
    } catch (ClassNotFoundException e) {
      // Running on RI.
      return new ClassLoader(Test1911.class.getClassLoader()) {
        protected Class<?> findClass(String name) throws ClassNotFoundException {
          if (name.equals("art.Target")) {
            return defineClass(name, CLASS_BYTES, 0, CLASS_BYTES.length);
          } else {
            return super.findClass(name);
          }
        }
      };
    }
  }

  public static void CheckLocalVariableTable(Executable m,
          Set<Locals.VariableDescription>[] possible_vars) {
    Set<Locals.VariableDescription> real_vars =
            new HashSet<>(Arrays.asList(Locals.GetLocalVariableTable(m)));
    for (Set<Locals.VariableDescription> pos : possible_vars) {
      if (pos.equals(real_vars)) {
        return;
      }
    }
    System.out.println("Unexpected variables for " + m);
    System.out.println("Received: " + real_vars);
    System.out.println("Expected one of:");
    for (Object pos : possible_vars) {
      System.out.println("\t" + pos);
    }
  }
  public static void run() throws Exception {
    Locals.EnableLocalVariableAccess();
    Class<?> target = getClassLoader().loadClass("art.Target");
    CheckLocalVariableTable(target.getDeclaredConstructor(Integer.TYPE),
            CONSTRUCTOR_VARIABLES);
    CheckLocalVariableTable(target.getDeclaredMethod("doNothing", (new Object[0]).getClass()),
            DO_NOTHING_VARIABLES);
    CheckLocalVariableTable(target.getDeclaredMethod("doSomething", Integer.TYPE),
            DO_SOMETHING_VARIABLES);
  }
}

