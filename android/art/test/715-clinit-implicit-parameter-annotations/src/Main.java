/*
 * Copyright (C) 2018 The Android Open Source Project
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
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Constructor;
import java.lang.reflect.Parameter;

public class Main {
    // A simple parameter annotation
    @Retention(RetentionPolicy.RUNTIME)
    public @interface AnnotationA {}

    // A parameter annotation with additional state
    @Retention(RetentionPolicy.RUNTIME)
    public @interface AnnotationB {
        String value() default "default-value";
    }

    // An inner class whose constructors with have an implicit
    // argument for the enclosing instance.
    public class Inner {
        private final int number;
        private final String text;
        boolean flag;

        Inner(@AnnotationA int number, String text) {
            this.number = number;
            this.text = text;
            this.flag = false;
        }

        Inner(@AnnotationA int number, String text, @AnnotationB("x") boolean flag) {
            this.number = number;
            this.text = text;
            this.flag = flag;
        }
    }

    // An inner class whose constructors with have no implicit
    // arguments for the enclosing instance.
    public static class StaticInner {
        private final int number;
        private final String text;
        boolean flag;

        StaticInner(@AnnotationA int number, String text) {
            this.number = number;
            this.text = text;
            this.flag = false;
        }

        StaticInner(@AnnotationB("foo") int number, String text, @AnnotationA boolean flag) {
            this.number = number;
            this.text = text;
            this.flag = flag;
        }
    }

    public enum ImportantNumber {
        ONE(1.0),
        TWO(2.0),
        MANY(3.0, true);

        private double doubleValue;
        private boolean isLarge;

        ImportantNumber(@AnnotationA double doubleValue) {
            this.doubleValue = doubleValue;
            this.isLarge = false;
        }

        ImportantNumber(@AnnotationB("x") double doubleValue, @AnnotationB("y") boolean isLarge) {
            this.doubleValue = doubleValue;
            this.isLarge = isLarge;
        }
    }

    public enum BinaryNumber {
        ZERO,
        ONE;
    }

    private abstract static class AnonymousBase {
        public AnonymousBase(@AnnotationA String s) {}
    }

    private static String annotationToNormalizedString(Annotation annotation) {
        // String.replace() to accomodate different representation across VMs.
        return annotation.toString().replace("\"", "");
    }

    private static void DumpConstructorParameterAnnotations(Class<?> cls) throws Throwable {
        System.out.println(cls.getName());
        for (Constructor c : cls.getDeclaredConstructors()) {
            System.out.println(" " + c);
            Annotation[][] annotations = c.getParameterAnnotations();
            Parameter[] parameters = c.getParameters();
            for (int i = 0; i < annotations.length; ++i) {
                // Exercise java.lang.reflect.Executable.getParameterAnnotationsNative()
                // which retrieves all annotations for the parameters.
                System.out.print("  Parameter [" + i + "]:");
                for (Annotation annotation : parameters[i].getAnnotations()) {
                    System.out.println("    Indexed : " + annotationToNormalizedString(annotation));
                }
                for (Annotation annotation : annotations[i]) {
                    System.out.println("    Array : " + annotationToNormalizedString(annotation));
                }

                // Exercise Parameter.getAnnotationNative() with
                // retrieves a single parameter annotation according to type.
                Object[] opaqueClasses = new Object[] {AnnotationA.class, AnnotationB.class};
                for (Object opaqueClass : opaqueClasses) {
                    @SuppressWarnings("unchecked")
                    Class<? extends Annotation> annotationClass =
                            (Class<? extends Annotation>) opaqueClass;
                    Annotation annotation = parameters[i].getDeclaredAnnotation(annotationClass);
                    String hasAnnotation = (annotation != null ? "Yes" : "No");
                    System.out.println("    " + annotationClass.getName() + " " + hasAnnotation);

                    Annotation[] parameterAnnotations = parameters[i].getDeclaredAnnotationsByType(annotationClass);
                    for (Annotation parameterAnnotation : parameterAnnotations) {
                        System.out.println("    " + annotationToNormalizedString(parameterAnnotation));
                    }
                }
            }
        }
    }

    private Class<?> getLocalClassWithEnclosingInstanceCapture() {
        class LocalClass {
            private final int integerValue;

            LocalClass(@AnnotationA int integerValue) {
                this.integerValue = integerValue;
            }
        }
        return LocalClass.class;
    }

    private Class<?> getLocalClassWithEnclosingInstanceAndLocalCapture() {
        final long CAPTURED_VALUE = System.currentTimeMillis();
        class LocalClassWithCapture {
            private final String value;
            private final long capturedValue;

            LocalClassWithCapture(@AnnotationA String p1) {
                this.value = p1;
                this.capturedValue = CAPTURED_VALUE;
            }
        }
        return LocalClassWithCapture.class;
    }

    public static void main(String[] args) throws Throwable {
        // A local class declared in a static context (0 implicit parameters).
        class LocalClassStaticContext {
            private final int value;

            LocalClassStaticContext(@AnnotationA int p0) {
                this.value = p0;
            }
        }

        final long CAPTURED_VALUE = System.currentTimeMillis();
        // A local class declared in a static context with a capture (1 implicit parameters).
        class LocalClassStaticContextWithCapture {
            private final long capturedValue;
            private final String argumentValue;

            LocalClassStaticContextWithCapture(@AnnotationA String p1) {
                this.capturedValue = CAPTURED_VALUE;
                this.argumentValue = p1;
            }
        }

        // Another local class declared in a static context with a capture (1 implicit parameters).
        class LocalClassStaticContextWithCaptureAlternateOrdering {
            private final String argumentValue;
            private final long capturedValue;

            LocalClassStaticContextWithCaptureAlternateOrdering(@AnnotationA String p1) {
                this.argumentValue = p1;
                this.capturedValue = CAPTURED_VALUE;
            }
        }

        DumpConstructorParameterAnnotations(Main.class);
        DumpConstructorParameterAnnotations(LocalClassStaticContext.class);
        DumpConstructorParameterAnnotations(LocalClassStaticContextWithCapture.class);
        DumpConstructorParameterAnnotations(LocalClassStaticContextWithCaptureAlternateOrdering.class);
        Main m = new Main();
        DumpConstructorParameterAnnotations(m.getLocalClassWithEnclosingInstanceCapture());
        DumpConstructorParameterAnnotations(m.getLocalClassWithEnclosingInstanceAndLocalCapture());
        DumpConstructorParameterAnnotations(Inner.class);
        DumpConstructorParameterAnnotations(StaticInner.class);
        DumpConstructorParameterAnnotations(ImportantNumber.class);
        DumpConstructorParameterAnnotations(BinaryNumber.class);
        DumpConstructorParameterAnnotations(new AnonymousBase("") {}.getClass());
    }
}
