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

import java.lang.annotation.Annotation;
import java.lang.reflect.Method;

public class AnnotationTest extends AnnotationTestHelpers {
  public static void testAnnotationsByType() {
    System.out.println("==============================");
    System.out.println("Class annotations by type:");
    System.out.println("==============================");

    // Print associated annotations:
    // * A is directly present or repeatably present on an element E;
    // * No annotation of A is directly/repeatably present on an element
    //   AND E is a class AND A's type is inheritable, AND A is associated with its superclass.
    // (Looks through subtypes recursively only if there's 0 result at each level,
    // and the annotation is @Inheritable).
    printAnnotationsByType(Calendar.class, SingleUser.class);
    printAnnotationsByType(Calendars.class, SingleUser.class);

    printAnnotationsByType(Calendar.class, User.class);
    printAnnotationsByType(Calendars.class, User.class);

    printAnnotationsByType(Calendar.class, User2.class);  // Enforce ordering 'z,x,y'
    printAnnotationsByType(Calendars.class, User2.class);

    // NOTE:
    //    Order of outer-most annotations Calendars[C,C],S vs C,Calendars[C,C] is unspecified.
    //    In particular it's the order of #getDeclaredAnnotations which is completely unmentioned.
    //    The only requirement for #getAnnotationsByType is to have same ordering as
    //    #getDeclaredAnnotations.
    //    (Calendars[] itself has to maintain value() order).
    printAnnotationsByType(Calendar.class, UserComplex.class);  // Cs(C,C),C collapses into C,C,C.
    printAnnotationsByType(Calendars.class, UserComplex.class);

    printAnnotationsByType(Calendar.class, UserSub.class);
    printAnnotationsByType(Calendars.class, UserSub.class);

    printAnnotationsByType(Calendar.class, UserSub2.class);
    // The directly present "Calendar" annotation masks all the repeatably present
    // "Calendar" annotations coming from User.
    printAnnotationsByType(Calendars.class, UserSub2.class);
    // Edge case: UserSub2 doesn't directly have a Calendars annotation,
    // so it doesn't mask the "User" Calendars annotation.

    System.out.println("-----------------------------");
    System.out.println("-----------------------------");

  }

  public static void testDeclaredAnnotation() {
    System.out.println("==============================");
    System.out.println("Class declared annotation:");
    System.out.println("==============================");

    // Print directly present annotations:
    //
    // The element E has an annotation_item for it (accessible through an
    // annotations_directory_item) corresponding to an annotation A,
    // and A's type_idx must match that on the encoded_annotation (from the annotation_item).
    // (Does not look through the subtypes recursively)
    printDeclaredAnnotation(SingleUser.class, Calendar.class);
    printDeclaredAnnotation(SingleUser.class, Calendars.class);

    printDeclaredAnnotation(User.class, Calendar.class);
    printDeclaredAnnotation(User.class, Calendars.class);

    printDeclaredAnnotation(UserComplex.class, Calendar.class);
    printDeclaredAnnotation(UserComplex.class, Calendars.class);

    printDeclaredAnnotation(UserSub.class, Calendar.class);
    printDeclaredAnnotation(UserSub.class, Calendars.class);

    printDeclaredAnnotation(UserSub2.class, Calendar.class);
    printDeclaredAnnotation(UserSub2.class, Calendars.class);

    System.out.println("-----------------------------");
    System.out.println("-----------------------------");
  }

  public static void testDeclaredAnnotationsByType() {
    System.out.println("==============================");
    System.out.println("Declared class annotations by type:");
    System.out.println("==============================");

    // A is directly present or repeatably present on an element E;
    // -- (does not do any recursion for classes regardless of @Inherited)
    printDeclaredAnnotationsByType(Calendar.class, SingleUser.class);
    printDeclaredAnnotationsByType(Calendars.class, SingleUser.class);

    printDeclaredAnnotationsByType(Calendar.class, User.class);
    printDeclaredAnnotationsByType(Calendars.class, User.class);

    printDeclaredAnnotationsByType(Calendar.class, User2.class);  // Enforce ordering 'z,x,y'
    printDeclaredAnnotationsByType(Calendars.class, User2.class);

    printDeclaredAnnotationsByType(Calendar.class, UserComplex.class);
    printDeclaredAnnotationsByType(Calendars.class, UserComplex.class);

    printDeclaredAnnotationsByType(Calendar.class, UserSub.class);
    printDeclaredAnnotationsByType(Calendars.class, UserSub.class);

    printDeclaredAnnotationsByType(Calendar.class, UserSub2.class);
    // The directly present "Calendar" annotation masks all the repeatably present "Calendar"
    // annotations coming from User.
    printDeclaredAnnotationsByType(Calendars.class, UserSub2.class);
    // Edge case: UserSub2 doesn't directly have a Calendars annotation,
    // so it doesn't mask the "User" Calendars annotation.

    System.out.println("-----------------------------");
    System.out.println("-----------------------------");
  }

  // Print the annotation "annotationClass" that is associated with an element denoted by
  // "annotationUseClass."
  private static <A extends Annotation> void printAnnotationsByType(Class<A> annotationClass,
      Class<?> annotationUseClass) {
    A[] annotationsByType = annotationUseClass.getAnnotationsByType(annotationClass);

    String msg = "Annotations by type, defined by class "
        + annotationUseClass.getName() + " with annotation " + annotationClass.getName() + ": "
        + asString(annotationsByType);


    System.out.println(msg);
  }

  private static <A extends Annotation> void printDeclaredAnnotation(Class<?> annotationUseClass,
      Class<A> annotationDefClass) {
    A anno = annotationUseClass.getDeclaredAnnotation(annotationDefClass);

    String msg = asString(anno);

    System.out.println("Declared annotations by class " + annotationUseClass
        + ", annotation " + annotationDefClass + ": " + msg);
  }

  // Print the annotation "annotationClass" that is directly/indirectly present with an element
  // denoted by "annotationUseClass."
  private static <A extends Annotation> void printDeclaredAnnotationsByType(
      Class<A> annotationClass, Class<?> annotationUseClass) {
    A[] annotationsByType = annotationUseClass.getDeclaredAnnotationsByType(annotationClass);

    String msg = "Declared annnotations by type, defined by class " + annotationUseClass.getName()
        + " with annotation " + annotationClass.getName() + ": "
        + asString(annotationsByType);

    System.out.println(msg);
  }

  public static void testMethodAnnotationsByType() {
    System.out.println("==============================");
    System.out.println("Method annotations by type:");
    System.out.println("==============================");

    // Print associated annotations:
    // * A is directly present or repeatably present on an element E;
    // * No annotation of A is directly/repeatably present on an element AND E is a class
    //   AND A's type is inheritable, AND A is associated with its superclass.
    // (Looks through subtypes recursively only if there's 0 result at each level,
    // and the annotation is @Inheritable).
    printMethodAnnotationsByType(Calendar.class, "singleUser", AnnotationTestFixture.class);
    printMethodAnnotationsByType(Calendars.class, "singleUser", AnnotationTestFixture.class);

    printMethodAnnotationsByType(Calendar.class, "user", AnnotationTestFixture.class);
    printMethodAnnotationsByType(Calendars.class, "user", AnnotationTestFixture.class);

    printMethodAnnotationsByType(Calendar.class, "user2", AnnotationTestFixture.class);
    printMethodAnnotationsByType(Calendars.class, "user2", AnnotationTestFixture.class);

    printMethodAnnotationsByType(Calendar.class, "userComplex", AnnotationTestFixture.class);
    printMethodAnnotationsByType(Calendars.class, "userComplex", AnnotationTestFixture.class);

    System.out.println("-----------------------------");
    System.out.println("-----------------------------");
  }

  // Print the annotation "annotationClass" that is associated with an element denoted by
  // "annotationUseClass" method methodName.
  private static <A extends Annotation> void printMethodAnnotationsByType(Class<A> annotationClass,
      String methodName, Class<?> annotationUseClass) {
    Method m = null;
    try {
      m = annotationUseClass.getDeclaredMethod(methodName);
    } catch (Throwable t) {
      throw new AssertionError(t);
    }
    A[] annotationsByType = m.getAnnotationsByType(annotationClass);

    String msg = "Annotations by type, defined by method " + m.getName() + " with annotation " +
      annotationClass.getName() + ": " +
      asString(annotationsByType);

    System.out.println(msg);
  }

  public static void testMethodDeclaredAnnotations() {
    System.out.println("==============================");
    System.out.println("Declared method annotations:");
    System.out.println("==============================");

    printMethodDeclaredAnnotation(Calendar.class, "singleUser", AnnotationTestFixture.class);
    printMethodDeclaredAnnotation(Calendars.class, "singleUser", AnnotationTestFixture.class);

    printMethodDeclaredAnnotation(Calendar.class, "user", AnnotationTestFixture.class);
    printMethodDeclaredAnnotation(Calendars.class, "user", AnnotationTestFixture.class);

    printMethodDeclaredAnnotation(Calendar.class, "user2", AnnotationTestFixture.class);
    printMethodDeclaredAnnotation(Calendars.class, "user2", AnnotationTestFixture.class);

    printMethodDeclaredAnnotation(Calendar.class, "userComplex", AnnotationTestFixture.class);
    printMethodDeclaredAnnotation(Calendars.class, "userComplex", AnnotationTestFixture.class);

    System.out.println("-----------------------------");
    System.out.println("-----------------------------");
  }

  // Print the annotation "annotationClass" that is associated with an element denoted by
  // methodName in annotationUseClass.
  private static <A extends Annotation> void printMethodDeclaredAnnotation(Class<A> annotationClass,
      String methodName, Class<?> annotationUseClass) {
    Method m = null;
    try {
      m = annotationUseClass.getDeclaredMethod(methodName);
    } catch (Throwable t) {
      throw new AssertionError(t);
    }
    Annotation annotationsByType = m.getDeclaredAnnotation(annotationClass);

    String msg = "Annotations declared by method " + m.getName() + " with annotation "
        + annotationClass.getName() + ": "
        + asString(annotationsByType);

    System.out.println(msg);
  }

  public static void testMethodDeclaredAnnotationsByType() {
    System.out.println("==============================");
    System.out.println("Declared method annotations by type:");
    System.out.println("==============================");

    printMethodDeclaredAnnotationByType(Calendar.class, "singleUser", AnnotationTestFixture.class);
    printMethodDeclaredAnnotationByType(Calendars.class, "singleUser", AnnotationTestFixture.class);

    printMethodDeclaredAnnotationByType(Calendar.class, "user", AnnotationTestFixture.class);
    printMethodDeclaredAnnotationByType(Calendars.class, "user", AnnotationTestFixture.class);

    printMethodDeclaredAnnotationByType(Calendar.class, "user2", AnnotationTestFixture.class);
    printMethodDeclaredAnnotationByType(Calendars.class, "user2", AnnotationTestFixture.class);

    printMethodDeclaredAnnotationByType(Calendar.class, "userComplex", AnnotationTestFixture.class);
    printMethodDeclaredAnnotationByType(Calendars.class, "userComplex",
        AnnotationTestFixture.class);

    System.out.println("-----------------------------");
    System.out.println("-----------------------------");
  }

  // Print the annotation "annotationClass" that is associated with an element denoted by
  // methodName in annotationUseClass.
  private static <A extends Annotation> void printMethodDeclaredAnnotationByType(
      Class<A> annotationClass, String methodName, Class<?> annotationUseClass) {
    Method m = null;
    try {
      m = annotationUseClass.getDeclaredMethod(methodName);
    } catch (Throwable t) {
      throw new AssertionError(t);
    }
    A[] annotationsByType = m.getDeclaredAnnotationsByType(annotationClass);

    String msg = "Annotations by type, defined by method " + m.getName() + " with annotation "
        + annotationClass.getName() + ": "
        + asString(annotationsByType);

    System.out.println(msg);
  }
}
