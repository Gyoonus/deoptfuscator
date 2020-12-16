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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
    public static void main(String[] args) {
        System.loadLibrary(args[0]);

        testGetFieldId(TestClass.class, "intField", "I");
        testGetFieldId(TestClass.class, "intField", "int");
        testGetFieldId(TestClass.class, "intField", "Lint;");
        testGetFieldId(TestClass.class, "stringField", "I");
        testGetFieldId(TestClass.class, "stringField", "Ljava/lang/String;");
        testGetFieldId(TestClass.class, "stringField", "java/lang/String");
        testGetFieldId(TestClass.class, "stringField", "Ljava.lang.String;");
        testGetFieldId(TestClass.class, "stringField", "java.lang.String");

        try {
            Method get = Main.class.getDeclaredMethod("getFieldId",
                                                      Class.class,
                                                      String.class,
                                                      String.class);
            MyClassLoader loader = new MyClassLoader(Main.class.getClassLoader());
            Class<?> otherMain = Class.forName("Main", true, loader);
            Method m = otherMain.getDeclaredMethod("testClassLoading", Method.class);
            m.invoke(null, get);
        } catch (Throwable t) {
            t.printStackTrace(System.out);
        }
    }

    public static void testClassLoading(Method get) throws Exception {
        System.out.println("Test that MyClassLoader.loadClass(\"Bad.Class\") shall not be called.");
        String[] bad_class_names = { "Bad/Class", "Bad.Class", "LBad.Class;" };
        for (String signature : bad_class_names) {
            try {
                get.invoke(null, TestClass.class, "bogus", signature);
                System.out.println("FAIL!");
            } catch (InvocationTargetException ite) {
                if (!(ite.getCause() instanceof NoSuchFieldError) ||
                    !(ite.getCause().getCause() instanceof NoClassDefFoundError)) {
                  throw ite;
                }
                NoClassDefFoundError ncdfe = (NoClassDefFoundError) ite.getCause().getCause();
                System.out.println("  Error message for " + signature + ": " + ncdfe.getMessage());
            }
        }
    }

    public static void testGetFieldId(Class<?> cls, String name, String signature) {
        System.out.println("getFieldId(" + cls + ", \"" + name + "\", \"" + signature + "\")");
        try {
            boolean result = getFieldId(cls, name, signature);
            System.out.println("Result: " + result);
        } catch (Throwable t) {
            System.out.println("Caught " + DescribeThrowable(t));
            for (Throwable cause = t.getCause(); cause != null; cause = cause.getCause()) {
                System.out.println("  caused by " + DescribeThrowable(cause));
            }
        }
    }

    public static String DescribeThrowable(Throwable t) {
        return PRINT_MESSAGE ? t.getClass().getName() + ": " + t.getMessage()
                             : t.getClass().getName();
    }

    public static native boolean getFieldId(Class<?> cls, String name, String signature);

    // Set to true to see actual messages.
    public static final boolean PRINT_MESSAGE = false;
}

class TestClass {
    public int intField;
    public String stringField;
}

class MyClassLoader extends DefiningLoader {
  public MyClassLoader(ClassLoader parent) {
      super(parent);
  }

  public Class<?> loadClass(String name) throws ClassNotFoundException
  {
      if (name.equals("Bad.Class")) {
          throw new Error("findClass(\"Bad.Class\")");
      }
      return super.loadClass(name);
  }
}
