target_config = {

# Configuration syntax:
#
#   Required keys: (Use one or more of these)
#    * golem - specify a golem machine-type to build, e.g. android-armv8
#              (uses art/tools/golem/build-target.sh)
#    * make - specify a make target to build, e.g. build-art-host
#    * run-test - runs the tests in art/test/ directory with testrunner.py,
#                 specify a list of arguments to pass to testrunner.py
#
#   Optional keys: (Use any of these)
#    * env - Add additional environment variable to the current environment.
#
# *** IMPORTANT ***:
#    This configuration is used by the android build server. Targets must not be renamed
#    or removed.
#

##########################################

    # General ART configurations.
    # Calls make and testrunner both.

    'art-test' : {
        'make' : 'test-art-host-gtest',
        'run-test' : [],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true'
        }
    },

    'art-test-javac' : {
        'run-test' : ['--jvm']
    },

    # ART run-test configurations
    # (calls testrunner which builds and then runs the test targets)

    'art-ndebug' : {
        'run-test' : ['--ndebug'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true'
        }
    },
    'art-interpreter' : {
        'run-test' : ['--interpreter'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true'
        }
    },
    'art-interpreter-access-checks' : {
        'run-test' : ['--interp-ac'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true'
        }
    },
    'art-jit' : {
        'run-test' : ['--jit'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true'
        }
    },
    'art-pictest' : {
        'run-test' : ['--pictest',
                      '--optimizing'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true'
        }
    },
    'art-gcstress-gcverify': {
        'run-test': ['--gcstress',
                     '--gcverify'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
            'ART_DEFAULT_GC_TYPE' : 'SS'
        }
    },
    'art-interpreter-gcstress' : {
        'run-test' : ['--interpreter',
                      '--gcstress'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
            'ART_DEFAULT_GC_TYPE' : 'SS'
        }
    },
    'art-optimizing-gcstress' : {
        'run-test' : ['--gcstress',
                      '--optimizing'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
            'ART_DEFAULT_GC_TYPE' : 'SS'
        }
    },
    'art-jit-gcstress' : {
        'run-test' : ['--jit',
                      '--gcstress'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
            'ART_DEFAULT_GC_TYPE' : 'SS'
        }
    },
    'art-read-barrier' : {
        'run-test': ['--interpreter',
                  '--optimizing'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-read-barrier-gcstress' : {
        'run-test' : ['--interpreter',
                      '--optimizing',
                      '--gcstress'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-read-barrier-table-lookup' : {
        'run-test' : ['--interpreter',
                      '--optimizing'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
            'ART_READ_BARRIER_TYPE' : 'TABLELOOKUP',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-debug-gc' : {
        'run-test' : ['--interpreter',
                      '--optimizing'],
        'env' : {
            'ART_TEST_DEBUG_GC' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-ss-gc' : {
        'run-test' : ['--interpreter',
                      '--optimizing',
                      '--jit'],
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gss-gc' : {
        'run-test' : ['--interpreter',
                      '--optimizing',
                      '--jit'],
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'GSS',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-ss-gc-tlab' : {
        'run-test' : ['--interpreter',
                      '--optimizing',
                      '--jit'],
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_TLAB' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gss-gc-tlab' : {
        'run-test' : ['--interpreter',
                      '--optimizing',
                      '--jit'],
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'GSS',
            'ART_USE_TLAB' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-tracing' : {
        'run-test' : ['--trace'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true'
        }
    },
    'art-interpreter-tracing' : {
        'run-test' : ['--interpreter',
                      '--trace'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
        }
    },
    'art-forcecopy' : {
        'run-test' : ['--forcecopy'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
        }
    },
    'art-no-prebuild' : {
        'run-test' : ['--no-prebuild'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
        }
    },
    'art-no-image' : {
        'run-test' : ['--no-image'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
        }
    },
    'art-interpreter-no-image' : {
        'run-test' : ['--interpreter',
                      '--no-image'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
        }
    },
    'art-relocate-no-patchoat' : {
        'run-test' : ['--relocate-npatchoat'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
        }
    },
    'art-no-dex2oat' : {
        'run-test' : ['--no-dex2oat'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
        }
    },
    'art-heap-poisoning' : {
        'run-test' : ['--interpreter',
                      '--optimizing',
                      '--cdex-none'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'false',
            'ART_HEAP_POISONING' : 'true',
            # Disable compact dex to get coverage of standard dex file usage.
            'ART_DEFAULT_COMPACT_DEX_LEVEL' : 'none'
        }
    },
    'art-preopt' : {
        # This test configuration is intended to be representative of the case
        # of preopted apps, which are precompiled compiled pic against an
        # unrelocated image, then used with a relocated image.
        'run-test' : ['--pictest',
                      '--prebuild',
                      '--relocate',
                      '--jit'],
        'env' : {
            'ART_USE_READ_BARRIER' : 'true'
        }
    },

    # ART gtest configurations
    # (calls make 'target' which builds and then runs the gtests).

    'art-gtest' : {
        'make' :  'test-art-host-gtest',
        'env' : {
            'ART_USE_READ_BARRIER' : 'true'
        }
    },
    'art-gtest-read-barrier': {
        'make' :  'test-art-host-gtest',
        'env' : {
            'ART_USE_READ_BARRIER' : 'true',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-gtest-read-barrier-table-lookup': {
        'make' :  'test-art-host-gtest',
        'env': {
            'ART_USE_READ_BARRIER' : 'true',
            'ART_READ_BARRIER_TYPE' : 'TABLELOOKUP',
            'ART_HEAP_POISONING' : 'true'
        }
    },
    'art-gtest-ss-gc': {
        'make' :  'test-art-host-gtest',
        'env': {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_READ_BARRIER' : 'false',
            # Disable compact dex to get coverage of standard dex file usage.
            'ART_DEFAULT_COMPACT_DEX_LEVEL' : 'none'
        }
    },
    'art-gtest-gss-gc': {
        'make' :  'test-art-host-gtest',
        'env' : {
            'ART_DEFAULT_GC_TYPE' : 'GSS',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gtest-ss-gc-tlab': {
        'make' :  'test-art-host-gtest',
        'env': {
            'ART_DEFAULT_GC_TYPE' : 'SS',
            'ART_USE_TLAB' : 'true',
            'ART_USE_READ_BARRIER' : 'false',
        }
    },
    'art-gtest-gss-gc-tlab': {
        'make' :  'test-art-host-gtest',
        'env': {
            'ART_DEFAULT_GC_TYPE' : 'GSS',
            'ART_USE_TLAB' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gtest-debug-gc' : {
        'make' :  'test-art-host-gtest',
        'env' : {
            'ART_TEST_DEBUG_GC' : 'true',
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gtest-valgrind32': {
      # Disabled: x86 valgrind does not understand SSE4.x
      # 'make' : 'valgrind-test-art-host32',
        'env': {
            'ART_USE_READ_BARRIER' : 'false'
        }
    },
    'art-gtest-valgrind64': {
        'make' : 'valgrind-test-art-host64',
        'env': {
            'ART_USE_READ_BARRIER' : 'false'
        }
    },

   # ASAN (host) configurations.

   # These configurations need detect_leaks=0 to work in non-setup environments like build bots,
   # as our build tools leak. b/37751350

    'art-gtest-asan': {
        'make' : 'test-art-host-gtest',
        'env': {
            'SANITIZE_HOST' : 'address',
            'ASAN_OPTIONS' : 'detect_leaks=0'
        }
    },
    'art-asan': {
        'run-test' : ['--interpreter',
                      '--optimizing',
                      '--jit'],
        'env': {
            'SANITIZE_HOST' : 'address',
            'ASAN_OPTIONS' : 'detect_leaks=0'
        }
    },
    'art-gtest-heap-poisoning': {
        'make' : 'test-art-host-gtest',
        'env' : {
            'ART_HEAP_POISONING' : 'true',
            'ART_USE_READ_BARRIER' : 'false',
            'SANITIZE_HOST' : 'address',
            'ASAN_OPTIONS' : 'detect_leaks=0'
        }
    },

   # ART Golem build targets used by go/lem (continuous ART benchmarking),
   # (art-opt-cc is used by default since it mimics the default preopt config),
   #
   # calls golem/build-target.sh which builds a golem tarball of the target name,
   #     e.g. 'golem: android-armv7' produces an 'android-armv7.tar.gz' upon success.

    'art-golem-android-armv7': {
        'golem' : 'android-armv7'
    },
    'art-golem-android-armv8': {
        'golem' : 'android-armv8'
    },
    'art-golem-linux-armv7': {
        'golem' : 'linux-armv7'
    },
    'art-golem-linux-armv8': {
        'golem' : 'linux-armv8'
    },
    'art-golem-linux-ia32': {
        'golem' : 'linux-ia32'
    },
    'art-golem-linux-x64': {
        'golem' : 'linux-x64'
    },
}
