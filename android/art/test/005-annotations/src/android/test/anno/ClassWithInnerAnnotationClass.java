package android.test.anno;

import java.lang.annotation.*;

public class ClassWithInnerAnnotationClass {
  @Retention(RetentionPolicy.SOURCE)
  public @interface MissingInnerAnnotationClass {}
}
