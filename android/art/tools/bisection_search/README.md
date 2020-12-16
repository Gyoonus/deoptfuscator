Bisection Bug Search
====================

Bisection Bug Search is a tool for finding compiler optimizations bugs. It
accepts a program which exposes a bug by producing incorrect output and expected
output for the program. It then attempts to narrow down the issue to a single
method and optimization pass under the assumption that interpreter is correct.

Given methods in order M0..Mn finds smallest i such that compiling Mi and
interpreting all other methods produces incorrect output. Then, given ordered
optimization passes P0..Pl, finds smallest j such that compiling Mi with passes
P0..Pj-1 produces expected output and compiling Mi with passes P0..Pj produces
incorrect output. Prints Mi and Pj.

How to run Bisection Bug Search
===============================

There are two supported invocation modes:

1. Regular invocation, dalvikvm command is constructed internally:

        ./bisection_search.py -cp classes.dex --expected-output out_int --class Test

2. Raw-cmd invocation, dalvikvm command is accepted as an argument.

   Extra dalvikvm arguments will be placed on second position in the command
   by default. {ARGS} tag can be used to specify a custom position.

   If used in device mode, the command has to exec a dalvikvm instance. Bisection
   will fail if pid of the process started by raw-cmd is different than pid of runtime.

        ./bisection_search.py --raw-cmd='run.sh -cp classes.dex Test' --expected-retcode SUCCESS
        ./bisection_search.py --raw-cmd='/bin/sh art {ARGS} -cp classes.dex Test' --expected-retcode SUCCESS

Help:

    bisection_search.py [-h] [-cp CLASSPATH] [--class CLASSNAME] [--lib LIB]
                             [--dalvikvm-option [OPT [OPT ...]]] [--arg [ARG [ARG ...]]]
                             [--image IMAGE] [--raw-cmd RAW_CMD]
                             [--64] [--device] [--device-serial DEVICE_SERIAL]
                             [--expected-output EXPECTED_OUTPUT]
                             [--expected-retcode {SUCCESS,TIMEOUT,ERROR}]
                             [--check-script CHECK_SCRIPT] [--logfile LOGFILE] [--cleanup]
                             [--timeout TIMEOUT] [--verbose]

    Tool for finding compiler bugs. Either --raw-cmd or both -cp and --class are required.

    optional arguments:
      -h, --help                                  show this help message and exit

    dalvikvm command options:
      -cp CLASSPATH, --classpath CLASSPATH        classpath
      --class CLASSNAME                           name of main class
      --lib LIB                                   lib to use, default: libart.so
      --dalvikvm-option [OPT [OPT ...]]           additional dalvikvm option
      --arg [ARG [ARG ...]]                       argument passed to test
      --image IMAGE                               path to image
      --raw-cmd RAW_CMD                           bisect with this command, ignore other command options

    bisection options:
      --64                                        x64 mode
      --device                                    run on device
      --device-serial DEVICE_SERIAL               device serial number, implies --device
      --expected-output EXPECTED_OUTPUT           file containing expected output
      --expected-retcode {SUCCESS,TIMEOUT,ERROR}  expected normalized return code
      --check-script CHECK_SCRIPT                 script comparing output and expected output
      --logfile LOGFILE                           custom logfile location
      --cleanup                                   clean up after bisecting
      --timeout TIMEOUT                           if timeout seconds pass assume test failed
      --verbose                                   enable verbose output
