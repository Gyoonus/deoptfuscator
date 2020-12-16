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

import dalvik.system.VMRuntime;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.util.function.Consumer;

public class ChildClass {
  enum PrimitiveType {
    TInteger('I', Integer.TYPE, Integer.valueOf(0)),
    TLong('J', Long.TYPE, Long.valueOf(0)),
    TFloat('F', Float.TYPE, Float.valueOf(0)),
    TDouble('D', Double.TYPE, Double.valueOf(0)),
    TBoolean('Z', Boolean.TYPE, Boolean.valueOf(false)),
    TByte('B', Byte.TYPE, Byte.valueOf((byte) 0)),
    TShort('S', Short.TYPE, Short.valueOf((short) 0)),
    TCharacter('C', Character.TYPE, Character.valueOf('0'));

    PrimitiveType(char shorty, Class klass, Object value) {
      mShorty = shorty;
      mClass = klass;
      mDefaultValue = value;
    }

    public char mShorty;
    public Class mClass;
    public Object mDefaultValue;
  }

  enum Hiddenness {
    Whitelist(PrimitiveType.TShort),
    LightGreylist(PrimitiveType.TBoolean),
    DarkGreylist(PrimitiveType.TByte),
    Blacklist(PrimitiveType.TCharacter);

    Hiddenness(PrimitiveType type) { mAssociatedType = type; }
    public PrimitiveType mAssociatedType;
  }

  enum Visibility {
    Public(PrimitiveType.TInteger),
    Package(PrimitiveType.TFloat),
    Protected(PrimitiveType.TLong),
    Private(PrimitiveType.TDouble);

    Visibility(PrimitiveType type) { mAssociatedType = type; }
    public PrimitiveType mAssociatedType;
  }

  enum Behaviour {
    Granted,
    Warning,
    Denied,
  }

  private static final boolean booleanValues[] = new boolean[] { false, true };

  public static void runTest(String libFileName, boolean expectedParentInBoot,
      boolean expectedChildInBoot, boolean everythingWhitelisted) throws Exception {
    System.load(libFileName);

    // Check expectations about loading into boot class path.
    isParentInBoot = (ParentClass.class.getClassLoader().getParent() == null);
    if (isParentInBoot != expectedParentInBoot) {
      throw new RuntimeException("Expected ParentClass " +
                                 (expectedParentInBoot ? "" : "not ") + "in boot class path");
    }
    isChildInBoot = (ChildClass.class.getClassLoader().getParent() == null);
    if (isChildInBoot != expectedChildInBoot) {
      throw new RuntimeException("Expected ChildClass " + (expectedChildInBoot ? "" : "not ") +
                                 "in boot class path");
    }
    ChildClass.everythingWhitelisted = everythingWhitelisted;

    boolean isSameBoot = (isParentInBoot == isChildInBoot);
    boolean isDebuggable = VMRuntime.getRuntime().isJavaDebuggable();

    // Run meaningful combinations of access flags.
    for (Hiddenness hiddenness : Hiddenness.values()) {
      final Behaviour expected;
      // Warnings are now disabled whenever access is granted, even for
      // greylisted APIs. This is the behaviour for release builds.
      if (isSameBoot || everythingWhitelisted || hiddenness == Hiddenness.Whitelist) {
        expected = Behaviour.Granted;
      } else if (hiddenness == Hiddenness.Blacklist) {
        expected = Behaviour.Denied;
      } else if (isDebuggable) {
        expected = Behaviour.Warning;
      } else {
        expected = Behaviour.Granted;
      }

      for (boolean isStatic : booleanValues) {
        String suffix = (isStatic ? "Static" : "") + hiddenness.name();

        for (Visibility visibility : Visibility.values()) {
          // Test reflection and JNI on methods and fields
          for (Class klass : new Class<?>[] { ParentClass.class, ParentInterface.class }) {
            String baseName = visibility.name() + suffix;
            checkField(klass, "field" + baseName, isStatic, visibility, expected);
            checkMethod(klass, "method" + baseName, isStatic, visibility, expected);
          }

          // Check whether one can use a class constructor.
          checkConstructor(ParentClass.class, visibility, hiddenness, expected);

          // Check whether one can use an interface default method.
          String name = "method" + visibility.name() + "Default" + hiddenness.name();
          checkMethod(ParentInterface.class, name, /*isStatic*/ false, visibility, expected);
        }

        // Test whether static linking succeeds.
        checkLinking("LinkFieldGet" + suffix, /*takesParameter*/ false, expected);
        checkLinking("LinkFieldSet" + suffix, /*takesParameter*/ true, expected);
        checkLinking("LinkMethod" + suffix, /*takesParameter*/ false, expected);
        checkLinking("LinkMethodInterface" + suffix, /*takesParameter*/ false, expected);
      }

      // Check whether Class.newInstance succeeds.
      checkNullaryConstructor(Class.forName("NullaryConstructor" + hiddenness.name()), expected);
    }
  }

  static final class RecordingConsumer implements Consumer<String> {
      public String recordedValue = null;

      @Override
      public void accept(String value) {
          recordedValue = value;
      }
  }

  private static void checkMemberCallback(Class<?> klass, String name,
          boolean isPublic, boolean isField) {
      try {
          RecordingConsumer consumer = new RecordingConsumer();
          VMRuntime.setNonSdkApiUsageConsumer(consumer);
          try {
              if (isPublic) {
                  if (isField) {
                      klass.getField(name);
                  } else {
                      klass.getMethod(name);
                  }
              } else {
                  if (isField) {
                      klass.getDeclaredField(name);
                  } else {
                      klass.getDeclaredMethod(name);
                  }
              }
          } catch (NoSuchFieldException|NoSuchMethodException ignored) {
              // We're not concerned whether an exception is thrown or not - we're
              // only interested in whether the callback is invoked.
          }

          if (consumer.recordedValue == null || !consumer.recordedValue.contains(name)) {
              throw new RuntimeException("No callback for member: " + name);
          }
      } finally {
          VMRuntime.setNonSdkApiUsageConsumer(null);
      }
  }

  private static void checkField(Class<?> klass, String name, boolean isStatic,
      Visibility visibility, Behaviour behaviour) throws Exception {

    boolean isPublic = (visibility == Visibility.Public);
    boolean canDiscover = (behaviour != Behaviour.Denied);
    boolean setsWarning = (behaviour == Behaviour.Warning);

    if (klass.isInterface() && (!isStatic || !isPublic)) {
      // Interfaces only have public static fields.
      return;
    }

    // Test discovery with reflection.

    if (Reflection.canDiscoverWithGetDeclaredField(klass, name) != canDiscover) {
      throwDiscoveryException(klass, name, true, "getDeclaredField()", canDiscover);
    }

    if (Reflection.canDiscoverWithGetDeclaredFields(klass, name) != canDiscover) {
      throwDiscoveryException(klass, name, true, "getDeclaredFields()", canDiscover);
    }

    if (Reflection.canDiscoverWithGetField(klass, name) != (canDiscover && isPublic)) {
      throwDiscoveryException(klass, name, true, "getField()", (canDiscover && isPublic));
    }

    if (Reflection.canDiscoverWithGetFields(klass, name) != (canDiscover && isPublic)) {
      throwDiscoveryException(klass, name, true, "getFields()", (canDiscover && isPublic));
    }

    // Test discovery with JNI.

    if (JNI.canDiscoverField(klass, name, isStatic) != canDiscover) {
      throwDiscoveryException(klass, name, true, "JNI", canDiscover);
    }

    // Test discovery with MethodHandles.lookup() which is caller
    // context sensitive.

    final MethodHandles.Lookup lookup = MethodHandles.lookup();
    if (JLI.canDiscoverWithLookupFindGetter(lookup, klass, name, int.class)
        != canDiscover) {
      throwDiscoveryException(klass, name, true, "MethodHandles.lookup().findGetter()",
                              canDiscover);
    }
    if (JLI.canDiscoverWithLookupFindStaticGetter(lookup, klass, name, int.class)
        != canDiscover) {
      throwDiscoveryException(klass, name, true, "MethodHandles.lookup().findStaticGetter()",
                              canDiscover);
    }

    // Test discovery with MethodHandles.publicLookup() which can only
    // see public fields. Looking up setters here and fields in
    // interfaces are implicitly final.

    final MethodHandles.Lookup publicLookup = MethodHandles.publicLookup();
    if (JLI.canDiscoverWithLookupFindSetter(publicLookup, klass, name, int.class)
        != canDiscover) {
      throwDiscoveryException(klass, name, true, "MethodHandles.publicLookup().findSetter()",
                              canDiscover);
    }
    if (JLI.canDiscoverWithLookupFindStaticSetter(publicLookup, klass, name, int.class)
        != canDiscover) {
      throwDiscoveryException(klass, name, true, "MethodHandles.publicLookup().findStaticSetter()",
                              canDiscover);
    }

    // Finish here if we could not discover the field.

    if (canDiscover) {
      // Test that modifiers are unaffected.

      if (Reflection.canObserveFieldHiddenAccessFlags(klass, name)) {
        throwModifiersException(klass, name, true);
      }

      // Test getters and setters when meaningful.

      clearWarning();
      if (!Reflection.canGetField(klass, name)) {
        throwAccessException(klass, name, true, "Field.getInt()");
      }
      if (hasPendingWarning() != setsWarning) {
        throwWarningException(klass, name, true, "Field.getInt()", setsWarning);
      }

      clearWarning();
      if (!Reflection.canSetField(klass, name)) {
        throwAccessException(klass, name, true, "Field.setInt()");
      }
      if (hasPendingWarning() != setsWarning) {
        throwWarningException(klass, name, true, "Field.setInt()", setsWarning);
      }

      clearWarning();
      if (!JNI.canGetField(klass, name, isStatic)) {
        throwAccessException(klass, name, true, "getIntField");
      }
      if (hasPendingWarning() != setsWarning) {
        throwWarningException(klass, name, true, "getIntField", setsWarning);
      }

      clearWarning();
      if (!JNI.canSetField(klass, name, isStatic)) {
        throwAccessException(klass, name, true, "setIntField");
      }
      if (hasPendingWarning() != setsWarning) {
        throwWarningException(klass, name, true, "setIntField", setsWarning);
      }
    }

    // Test that callbacks are invoked correctly.
    clearWarning();
    if (setsWarning || !canDiscover) {
      checkMemberCallback(klass, name, isPublic, true /* isField */);
    }
  }

  private static void checkMethod(Class<?> klass, String name, boolean isStatic,
      Visibility visibility, Behaviour behaviour) throws Exception {

    boolean isPublic = (visibility == Visibility.Public);
    if (klass.isInterface() && !isPublic) {
      // All interface members are public.
      return;
    }

    boolean canDiscover = (behaviour != Behaviour.Denied);
    boolean setsWarning = (behaviour == Behaviour.Warning);

    // Test discovery with reflection.

    if (Reflection.canDiscoverWithGetDeclaredMethod(klass, name) != canDiscover) {
      throwDiscoveryException(klass, name, false, "getDeclaredMethod()", canDiscover);
    }

    if (Reflection.canDiscoverWithGetDeclaredMethods(klass, name) != canDiscover) {
      throwDiscoveryException(klass, name, false, "getDeclaredMethods()", canDiscover);
    }

    if (Reflection.canDiscoverWithGetMethod(klass, name) != (canDiscover && isPublic)) {
      throwDiscoveryException(klass, name, false, "getMethod()", (canDiscover && isPublic));
    }

    if (Reflection.canDiscoverWithGetMethods(klass, name) != (canDiscover && isPublic)) {
      throwDiscoveryException(klass, name, false, "getMethods()", (canDiscover && isPublic));
    }

    // Test discovery with JNI.

    if (JNI.canDiscoverMethod(klass, name, isStatic) != canDiscover) {
      throwDiscoveryException(klass, name, false, "JNI", canDiscover);
    }

    // Test discovery with MethodHandles.lookup().

    final MethodHandles.Lookup lookup = MethodHandles.lookup();
    final MethodType methodType = MethodType.methodType(int.class);
    if (JLI.canDiscoverWithLookupFindVirtual(lookup, klass, name, methodType) != canDiscover) {
      throwDiscoveryException(klass, name, false, "MethodHandles.lookup().findVirtual()",
                              canDiscover);
    }

    if (JLI.canDiscoverWithLookupFindStatic(lookup, klass, name, methodType) != canDiscover) {
      throwDiscoveryException(klass, name, false, "MethodHandles.lookup().findStatic()",
                              canDiscover);
    }

    // Finish here if we could not discover the method.

    if (canDiscover) {
      // Test that modifiers are unaffected.

      if (Reflection.canObserveMethodHiddenAccessFlags(klass, name)) {
        throwModifiersException(klass, name, false);
      }

      // Test whether we can invoke the method. This skips non-static interface methods.

      if (!klass.isInterface() || isStatic) {
        clearWarning();
        if (!Reflection.canInvokeMethod(klass, name)) {
          throwAccessException(klass, name, false, "invoke()");
        }
        if (hasPendingWarning() != setsWarning) {
          throwWarningException(klass, name, false, "invoke()", setsWarning);
        }

        clearWarning();
        if (!JNI.canInvokeMethodA(klass, name, isStatic)) {
          throwAccessException(klass, name, false, "CallMethodA");
        }
        if (hasPendingWarning() != setsWarning) {
          throwWarningException(klass, name, false, "CallMethodA()", setsWarning);
        }

        clearWarning();
        if (!JNI.canInvokeMethodV(klass, name, isStatic)) {
          throwAccessException(klass, name, false, "CallMethodV");
        }
        if (hasPendingWarning() != setsWarning) {
          throwWarningException(klass, name, false, "CallMethodV()", setsWarning);
        }
      }
    }

    // Test that callbacks are invoked correctly.
    clearWarning();
    if (setsWarning || !canDiscover) {
        checkMemberCallback(klass, name, isPublic, false /* isField */);
    }
  }

  private static void checkConstructor(Class<?> klass, Visibility visibility, Hiddenness hiddenness,
      Behaviour behaviour) throws Exception {

    boolean isPublic = (visibility == Visibility.Public);
    String signature = "(" + visibility.mAssociatedType.mShorty +
                             hiddenness.mAssociatedType.mShorty + ")V";
    String fullName = "<init>" + signature;
    Class<?> args[] = new Class[] { visibility.mAssociatedType.mClass,
                                    hiddenness.mAssociatedType.mClass };
    Object initargs[] = new Object[] { visibility.mAssociatedType.mDefaultValue,
                                       hiddenness.mAssociatedType.mDefaultValue };
    MethodType methodType = MethodType.methodType(void.class, args);

    boolean canDiscover = (behaviour != Behaviour.Denied);
    boolean setsWarning = (behaviour == Behaviour.Warning);

    // Test discovery with reflection.

    if (Reflection.canDiscoverWithGetDeclaredConstructor(klass, args) != canDiscover) {
      throwDiscoveryException(klass, fullName, false, "getDeclaredConstructor()", canDiscover);
    }

    if (Reflection.canDiscoverWithGetDeclaredConstructors(klass, args) != canDiscover) {
      throwDiscoveryException(klass, fullName, false, "getDeclaredConstructors()", canDiscover);
    }

    if (Reflection.canDiscoverWithGetConstructor(klass, args) != (canDiscover && isPublic)) {
      throwDiscoveryException(
          klass, fullName, false, "getConstructor()", (canDiscover && isPublic));
    }

    if (Reflection.canDiscoverWithGetConstructors(klass, args) != (canDiscover && isPublic)) {
      throwDiscoveryException(
          klass, fullName, false, "getConstructors()", (canDiscover && isPublic));
    }

    // Test discovery with JNI.

    if (JNI.canDiscoverConstructor(klass, signature) != canDiscover) {
      throwDiscoveryException(klass, fullName, false, "JNI", canDiscover);
    }

    // Test discovery with MethodHandles.lookup()

    final MethodHandles.Lookup lookup = MethodHandles.lookup();
    if (JLI.canDiscoverWithLookupFindConstructor(lookup, klass, methodType) != canDiscover) {
      throwDiscoveryException(klass, fullName, false, "MethodHandles.lookup().findConstructor",
                              canDiscover);
    }

    final MethodHandles.Lookup publicLookup = MethodHandles.publicLookup();
    if (JLI.canDiscoverWithLookupFindConstructor(publicLookup, klass, methodType) != canDiscover) {
      throwDiscoveryException(klass, fullName, false,
                              "MethodHandles.publicLookup().findConstructor",
                              canDiscover);
    }

    // Finish here if we could not discover the constructor.

    if (!canDiscover) {
      return;
    }

    // Test whether we can invoke the constructor.

    clearWarning();
    if (!Reflection.canInvokeConstructor(klass, args, initargs)) {
      throwAccessException(klass, fullName, false, "invoke()");
    }
    if (hasPendingWarning() != setsWarning) {
      throwWarningException(klass, fullName, false, "invoke()", setsWarning);
    }

    clearWarning();
    if (!JNI.canInvokeConstructorA(klass, signature)) {
      throwAccessException(klass, fullName, false, "NewObjectA");
    }
    if (hasPendingWarning() != setsWarning) {
      throwWarningException(klass, fullName, false, "NewObjectA", setsWarning);
    }

    clearWarning();
    if (!JNI.canInvokeConstructorV(klass, signature)) {
      throwAccessException(klass, fullName, false, "NewObjectV");
    }
    if (hasPendingWarning() != setsWarning) {
      throwWarningException(klass, fullName, false, "NewObjectV", setsWarning);
    }
  }

  private static void checkNullaryConstructor(Class<?> klass, Behaviour behaviour)
      throws Exception {
    boolean canAccess = (behaviour != Behaviour.Denied);
    boolean setsWarning = (behaviour == Behaviour.Warning);

    clearWarning();
    if (Reflection.canUseNewInstance(klass) != canAccess) {
      throw new RuntimeException("Expected to " + (canAccess ? "" : "not ") +
          "be able to construct " + klass.getName() + ". " +
          "isParentInBoot = " + isParentInBoot + ", " + "isChildInBoot = " + isChildInBoot);
    }
    if (canAccess && hasPendingWarning() != setsWarning) {
      throwWarningException(klass, "nullary constructor", false, "newInstance", setsWarning);
    }
  }

  private static void checkLinking(String className, boolean takesParameter, Behaviour behaviour)
      throws Exception {
    boolean canAccess = (behaviour != Behaviour.Denied);
    boolean setsWarning = (behaviour == Behaviour.Warning);

    clearWarning();
    if (Linking.canAccess(className, takesParameter) != canAccess) {
      throw new RuntimeException("Expected to " + (canAccess ? "" : "not ") +
          "be able to verify " + className + "." +
          "isParentInBoot = " + isParentInBoot + ", " + "isChildInBoot = " + isChildInBoot);
    }
    if (canAccess && hasPendingWarning() != setsWarning) {
      throwWarningException(
          Class.forName(className), "access", false, "static linking", setsWarning);
    }
  }

  private static void throwDiscoveryException(Class<?> klass, String name, boolean isField,
      String fn, boolean canAccess) {
    throw new RuntimeException("Expected " + (isField ? "field " : "method ") + klass.getName() +
        "." + name + " to " + (canAccess ? "" : "not ") + "be discoverable with " + fn + ". " +
        "isParentInBoot = " + isParentInBoot + ", " + "isChildInBoot = " + isChildInBoot + ", " +
        "everythingWhitelisted = " + everythingWhitelisted);
  }

  private static void throwAccessException(Class<?> klass, String name, boolean isField,
      String fn) {
    throw new RuntimeException("Expected to be able to access " + (isField ? "field " : "method ") +
        klass.getName() + "." + name + " using " + fn + ". " +
        "isParentInBoot = " + isParentInBoot + ", " + "isChildInBoot = " + isChildInBoot + ", " +
        "everythingWhitelisted = " + everythingWhitelisted);
  }

  private static void throwWarningException(Class<?> klass, String name, boolean isField,
      String fn, boolean setsWarning) {
    throw new RuntimeException("Expected access to " + (isField ? "field " : "method ") +
        klass.getName() + "." + name + " using " + fn + " to " + (setsWarning ? "" : "not ") +
        "set the warning flag. " +
        "isParentInBoot = " + isParentInBoot + ", " + "isChildInBoot = " + isChildInBoot + ", " +
        "everythingWhitelisted = " + everythingWhitelisted);
  }

  private static void throwModifiersException(Class<?> klass, String name, boolean isField) {
    throw new RuntimeException("Expected " + (isField ? "field " : "method ") + klass.getName() +
        "." + name + " to not expose hidden modifiers");
  }

  private static boolean isParentInBoot;
  private static boolean isChildInBoot;
  private static boolean everythingWhitelisted;

  private static native boolean hasPendingWarning();
  private static native void clearWarning();
}
