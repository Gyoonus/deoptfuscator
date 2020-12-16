# Java Heap Model

The value of the Java heap model is the sum of bytes of Java objects allocated
on the Java heap. It can be calculated using:

    Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory()

A Java heap dump is used for an actionable breakdown of the Java heap.
