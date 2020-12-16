openjdkjvmti plugin
====

This is a partial implementation of the JVMTI v1.2 interface for the android
runtime as a plugin. This allows the use of agents that can modify the running
state of the program by modifying dex files in memory and performing other
operations on the global runtime state.
