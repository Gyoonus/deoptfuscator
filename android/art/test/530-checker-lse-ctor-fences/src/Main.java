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
import java.lang.reflect.Method;

// This base class has a single final field;
// the constructor should have one fence.
class Circle {
  Circle(double radius) {
    this.radius = radius;
  }
  public double getRadius() {
    return radius;
  }
  public double getArea() {
    return radius * radius * Math.PI;
  }

  public double getCircumference() {
    return 2 * Math.PI * radius;
  }

  private final double radius;
}

// This subclass adds an extra final field;
// there should be an extra constructor fence added
// (for a total of 2 after inlining).
class Ellipse extends Circle {
  Ellipse(double vertex, double covertex) {
    super(vertex);

    this.covertex = covertex;
  }

  public double getVertex() {
    return getRadius();
  }

  public double getCovertex() {
    return covertex;
  }

  @Override
  public double getArea() {
    return getRadius() * covertex * Math.PI;
  }

  private final double covertex;
}

class CalcCircleAreaOrCircumference {
  public static final int TYPE_AREA = 0;
  public static final int TYPE_CIRCUMFERENCE = 1;

  double value;

  public CalcCircleAreaOrCircumference(int type) {
    this.type = type;
  }

  final int type;
}

public class Main {

  /// CHECK-START: double Main.calcCircleArea(double) load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: ConstructorFence
  /// CHECK: InstanceFieldGet

  /// CHECK-START: double Main.calcCircleArea(double) load_store_elimination (after)
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: ConstructorFence
  /// CHECK-NOT: InstanceFieldGet

  // Make sure the constructor fence gets eliminated when the allocation is eliminated.
  static double calcCircleArea(double radius) {
    return new Circle(radius).getArea();
  }

  /// CHECK-START: double Main.calcEllipseArea(double, double) load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: ConstructorFence
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: double Main.calcEllipseArea(double, double) load_store_elimination (after)
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: ConstructorFence
  /// CHECK-NOT: InstanceFieldGet

  // Multiple constructor fences can accumulate through inheritance, make sure
  // they are all eliminated when the allocation is eliminated.
  static double calcEllipseArea(double vertex, double covertex) {
    return new Ellipse(vertex, covertex).getArea();
  }

  /// CHECK-START: double Main.calcCircleAreaOrCircumference(double, boolean) load_store_elimination (after)
  /// CHECK-NOT: ConstructorFence

  //
  // The object allocation will not be eliminated by LSE because of aliased stores.
  // However the object is still a singleton, so it never escapes the current thread.
  // There should not be a constructor fence here after LSE.
  static double calcCircleAreaOrCircumference(double radius, boolean area_or_circumference) {
    CalcCircleAreaOrCircumference calc =
      new CalcCircleAreaOrCircumference(
          area_or_circumference ? CalcCircleAreaOrCircumference.TYPE_AREA :
          CalcCircleAreaOrCircumference.TYPE_CIRCUMFERENCE);

    if (area_or_circumference) {
      // Area
      calc.value = Math.PI * Math.PI * radius;
    } else {
      // Circumference
      calc.value = 2 * Math.PI * radius;
    }

    return calc.value;
  }

  static double calcCircleAreaOrCircumferenceSmali(double radius, boolean area_or_circumference) {
    try {
      Class<?> c = Class.forName("Smali");
      Method m = c.getMethod("calcCircleAreaOrCircumference", double.class, boolean.class);
      return (Double) m.invoke(null, radius, area_or_circumference);
    } catch (Exception ex) {
      throw new Error(ex);
    }
  }

  /// CHECK-START: Circle Main.makeCircle(double) load_store_elimination (after)
  /// CHECK: NewInstance
  /// CHECK: ConstructorFence

  // The object allocation is considered a singleton by LSE,
  // but we cannot eliminate the new because it is returned.
  //
  // The constructor fence must also not be removed because the object could escape the
  // current thread (in the caller).
  static Circle makeCircle(double radius) {
    return new Circle(radius);
  }

  static void assertIntEquals(int result, int expected) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  static void assertFloatEquals(float result, float expected) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  static void assertDoubleEquals(double result, double expected) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  static void assertInstanceOf(Object result, Class<?> expected) {
    if (result.getClass() != expected) {
      throw new Error("Expected type: " + expected + ", found : " + result.getClass());
    }
  }

  public static void main(String[] args) {
    assertDoubleEquals(Math.PI * Math.PI * Math.PI, calcCircleArea(Math.PI));
    assertDoubleEquals(Math.PI * Math.PI * Math.PI, calcEllipseArea(Math.PI, Math.PI));
    assertDoubleEquals(2 * Math.PI * Math.PI, calcCircleAreaOrCircumference(Math.PI, false));
    assertDoubleEquals(2 * Math.PI * Math.PI, calcCircleAreaOrCircumferenceSmali(Math.PI, false));
    assertInstanceOf(makeCircle(Math.PI), Circle.class);
  }

  static boolean sFlag;
}
