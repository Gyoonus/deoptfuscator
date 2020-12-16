/*
 * Copyright 2013 The Android Open Source Project
 *
 * Generate a big pile of classes with big <clinit>.
 */
#include <stdio.h>

/*
 * Create N files.
 */
static int createFiles(int count, int array_size)
{
    FILE* fp;
    int i;
    int k;

    for (i = 0; i < count; i++) {
        char nameBuf[32];

        snprintf(nameBuf, sizeof(nameBuf), "src/Test%03d.java", i);
        fp = fopen(nameBuf, "w");
        if (fp == NULL) {
            fprintf(stderr, "ERROR: unable to open %s\n", nameBuf);
            return -1;
        }

        fprintf(fp, "public class Test%03d {\n", i);
        fprintf(fp, "    static String[] array = new String[%d];\n", array_size);
        fprintf(fp, "    static {\n");
        for (k = 0; k < array_size; k++) {
            fprintf(fp, "        array[%d] = \"string_%04d\";\n", k, k);
        }
        fprintf(fp, "    }\n");
        fprintf(fp, "}\n");
        fclose(fp);
    }

    // Create test class.
    fp = fopen("src/MainTest.java", "w");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: unable to open src/MainTest.java\n");
        return -1;
    }
    fprintf(fp, "public class MainTest {\n");
    fprintf(fp, "    public static void run() {\n");
    for (i = 0; i < count; i++) {
        fprintf(fp, "        System.out.println(\"Create new Test%03d\");\n", i);
        fprintf(fp, "        new Test%03d();\n", i);
    }
    fprintf(fp, "    }\n");
    fprintf(fp, "}\n");
    fclose(fp);

    return 0;
}

int main()
{
    int result;

    result = createFiles(40, 2000);

    return (result != 0);
}
