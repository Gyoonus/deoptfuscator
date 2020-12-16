
-keep public class Main {
  public static void main(java.lang.String[]);
}

-keep public class SuperDumpedStuff {
  public void allocateObjectAtUnObfSuperSite();
}

# Produce useful obfuscated stack traces so we can test useful deobfuscation
# of stack traces.
-renamesourcefileattribute SourceFile
-keepattributes SourceFile,LineNumberTable

