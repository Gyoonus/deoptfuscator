JFuzz
=====

JFuzz is a tool for generating random programs with the objective
of fuzz testing the ART infrastructure. Each randomly generated program
can be run under various modes of execution, such as using the interpreter,
using the optimizing compiler, using an external reference implementation,
or using various target architectures. Any difference between the outputs
(**divergence**) may indicate a bug in one of the execution modes.

JFuzz can be combined with DexFuzz to get multi-layered fuzz testing.

How to run JFuzz
================

    jfuzz [-s seed] [-d expr-depth] [-l stmt-length]
             [-i if-nest] [-n loop-nest] [-v] [-h]

where

    -s : defines a deterministic random seed
         (randomized using time by default)
    -d : defines a fuzzing depth for expressions
         (higher values yield deeper expressions)
    -l : defines a fuzzing length for statement lists
         (higher values yield longer statement sequences)
    -i : defines a fuzzing nest for if/switch statements
         (higher values yield deeper nested conditionals)
    -n : defines a fuzzing nest for for/while/do-while loops
         (higher values yield deeper nested loops)
    -t : defines a fuzzing nest for try-catch-finally blocks
         (higher values yield deeper nested try-catch-finally blocks)
    -v : prints version number and exits
    -h : prints help and exits

The current version of JFuzz sends all output to stdout, and uses
a fixed testing class named Test. So a typical test run looks as follows.

    jfuzz > Test.java
    jack -cp ${JACK_CLASSPATH} --output-dex . Test.java
    art -classpath classes.dex Test

How to start JFuzz testing
==========================

    run_jfuzz_test.py
                          [--num_tests=NUM_TESTS]
                          [--device=DEVICE]
                          [--mode1=MODE] [--mode2=MODE]
                          [--report_script=SCRIPT]
                          [--jfuzz_arg=ARG]
                          [--true_divergence]
                          [--dexer=DEXER]
                          [--debug_info]

where

    --num_tests       : number of tests to run (10000 by default)
    --device          : target device serial number (passed to adb -s)
    --mode1           : m1
    --mode2           : m2, with m1 != m2, and values one of
      ri   = reference implementation on host (default for m1)
      hint = Art interpreter on host
      hopt = Art optimizing on host (default for m2)
      tint = Art interpreter on target
      topt = Art optimizing on target
    --report_script   : path to script called for each divergence
    --jfuzz_arg       : argument for jfuzz
    --true_divergence : don't bisect timeout divergences
    --dexer=DEXER     : use either dx, d8, or jack to obtain dex files
    --debug_info      : include debugging info

How to start JFuzz nightly testing
==================================

    run_jfuzz_test_nightly.py
                          [--num_proc NUM_PROC]

where

    --num_proc      : number of run_jfuzz_test.py instances to run (8 by default)

Remaining arguments are passed to run\_jfuzz_test.py.

How to start J/DexFuzz testing (multi-layered)
==============================================

    run_dex_fuzz_test.py
                          [--num_tests=NUM_TESTS]
                          [--num_inputs=NUM_INPUTS]
                          [--device=DEVICE]
                          [--dexer=DEXER]
                          [--debug_info]

where

    --num_tests   : number of tests to run (10000 by default)
    --num_inputs  : number of JFuzz programs to generate
    --device      : target device serial number (passed to adb -s)
    --dexer=DEXER : use either dx, d8, or jack to obtain dex files
    --debug_info  : include debugging info

Background
==========

Although test suites are extremely useful to validate the correctness of a
system and to ensure that no regressions occur, any test suite is necessarily
finite in size and scope. Tests typically focus on validating particular
features by means of code sequences most programmers would expect. Regression
tests often use slightly less idiomatic code sequences, since they reflect
problems that were not anticipated originally, but occurred “in the field”.
Still, any test suite leaves the developer wondering whether undetected bugs
and flaws still linger in the system.

Over the years, fuzz testing has gained popularity as a testing technique for
discovering such lingering bugs, including bugs that can bring down a system
in an unexpected way. Fuzzing refers to feeding a large amount of random data
as input to a system in an attempt to find bugs or make it crash. Generation-
based fuzz testing constructs random, but properly formatted input data.
Mutation-based fuzz testing applies small random changes to existing inputs
in order to detect shortcomings in a system. Profile-guided or coverage-guided
fuzzing adds a direction to the way these random changes are applied. Multi-
layered approaches generate random inputs that are subsequently mutated at
various stages of execution.

The randomness of fuzz testing implies that the size and scope of testing is no
longer bounded. Every new run can potentially discover bugs and crashes that were
hereto undetected.
