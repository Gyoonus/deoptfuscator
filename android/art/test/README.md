# VM test harness

There are two suites of tests in this directory: run-tests and gtests.

The run-tests are identified by directories named with with a numeric
prefix and containing an info.txt file. For most run tests, the
sources are in the "src" subdirectory. Sources found in the "src2"
directory are compiled separately but to the same output directory;
this can be used to exercise "API mismatch" situations by replacing
class files created in the first pass. The "src-ex" directory is
built separately, and is intended for exercising class loaders.
Resources can be stored in the "res" directory, which is distributed
together with the executable files.

The gtests are in named directories and contain a .java source
file.

All tests in either suite can be run using the "art/test.py"
script. Additionally, run-tests can be run individidually. All of the
tests can be run on the build host, on a USB-attached device, or using
the build host "reference implementation".

To see command flags run:

```sh
$ art/test.py -h
```

## Running all tests on the build host

```sh
$ art/test.py --host
```

## Running all tests on the target device

```sh
$ art/test.py --target
```

## Running all gtests on the build host

```sh
$ art/test.py --host -g
```

## Running all gtests on the target device

```sh
$ art/test.py --target -g
```

## Running all run-tests on the build host

```sh
$ art/test.py --host -r
```

## Running all run-tests on the target device

```sh
$ art/test.py --target -r
```

## Running one run-test on the build host

```sh
$ art/test.py --host -r -t 001-HelloWorld
```

## Running one run-test on the target device

```sh
$ art/test.py --target -r -t 001-HelloWorld
```
