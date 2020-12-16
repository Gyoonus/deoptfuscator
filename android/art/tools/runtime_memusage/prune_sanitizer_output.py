#!/usr/bin/env python
#
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Cleans up overlapping portions of traces provided by logcat."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import re
import os

STACK_DIVIDER = 65 * "="


def match_to_int(match):
    """Returns trace line number matches as integers for sorting.
    Maps other matches to negative integers.
    """
    # Hard coded string are necessary since each trace must have the address
    # accessed, which is printed before trace lines.
    if match == "use-after-poison" or match == "unknown-crash":
        return -2
    elif match == "READ":
        return -1
    # Cutting off non-integer part of match
    return int(match[1:-1])


def clean_trace_if_valid(trace, stack_min_size, prune_exact):
    """Cleans trace if it meets a certain standard. Returns None otherwise."""
    # Note: Sample input may contain "unknown-crash" instead of
    # "use-after-poison"
    #
    # Sample input:
    #   trace:
    # "...ERROR: AddressSanitizer: use-after-poison on address 0x0071126a870a...
    # ...READ of size 2 at 0x0071126a870a thread T0 (droid.deskclock)
    # ...    #0 0x71281013b3  (/data/asan/system/lib64/libart.so+0x2263b3)
    # ...    #1 0x71280fe6b7  (/data/asan/system/lib64/libart.so+0x2236b7)
    # ...    #3 0x71280c22ef  (/data/asan/system/lib64/libart.so+0x1e72ef)
    # ...    #2 0x712810a84f  (/data/asan/system/lib64/libart.so+0x22f84f)"
    #
    #   stack_min_size: 2
    #   prune_exact: False
    #
    # Sample output:
    #
    # "...ERROR: AddressSanitizer: use-after-poison on address 0x0071126a870a...
    # ...READ of size 2 at 0x0071126a870a thread T0 (droid.deskclock)
    # ...    #0 0x71281013b3  (/data/asan/system/lib64/libart.so+0x2263b3)
    # ...    #1 0x71280fe6b7  (/data/asan/system/lib64/libart.so+0x2236b7)
    # "

    # Adds a newline character if not present at the end of trace
    trace = trace if trace[-1] == "\n" else trace + "\n"
    trace_line_matches = [(match_to_int(match.group()), match.start())
                          for match in re.finditer("#[0-9]+ "
                                                   "|use-after-poison"
                                                   "|unknown-crash"
                                                   "|READ", trace)
                          ]
    # Finds the first index where the line number ordering isn't in sequence or
    # returns the number of matches if it everything is in order.
    bad_line_no = next((i - 2 for i, match in enumerate(trace_line_matches)
                        if i - 2 != match[0]), len(trace_line_matches) - 2)
    # If the number ordering breaks after minimum stack size, then the trace is
    # still valid.
    if bad_line_no >= stack_min_size:
        # Added if the trace is already clean
        trace_line_matches.append((trace_line_matches[-1][0] + 1, len(trace)))
        bad_match = trace_line_matches[bad_line_no + 2]
        if prune_exact:
            bad_match = trace_line_matches[stack_min_size + 2]
        # Up to new-line that comes before bad line number
        return trace[:trace.rindex("\n", 0, bad_match[1]) + 1]
    return None


def extant_directory(path_name):
    """Checks if a path is an actual directory."""
    if not os.path.isdir(path_name):
        dir_error = "%s is not a directory" % (path_name)
        raise argparse.ArgumentTypeError(dir_error)
    return path_name


def parse_args():
    """Parses arguments passed in."""
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", action="store",
                        default="", dest="out_dir_name", type=extant_directory,
                        help="Output Directory")
    parser.add_argument("-e", action="store_true",
                        default=False, dest="check_exact",
                        help="Forces each trace to be cut to have "
                             "minimum number of lines")
    parser.add_argument("-m", action="store",
                        default=4, dest="stack_min_size", type=int,
                        help="minimum number of lines a trace should have")
    parser.add_argument("trace_file", action="store",
                        type=argparse.FileType("r"),
                        help="File only containing lines that are related to "
                             "Sanitizer traces")
    return parser.parse_args()


def main():
    """Parses arguments and cleans up traces using other functions."""
    stack_min_size = 4
    check_exact = False

    parsed_argv = parse_args()
    stack_min_size = parsed_argv.stack_min_size
    check_exact = parsed_argv.check_exact
    out_dir_name = parsed_argv.out_dir_name
    trace_file = parsed_argv.trace_file

    trace_split = trace_file.read().split(STACK_DIVIDER)
    trace_file.close()
    trace_clean_split = [clean_trace_if_valid(trace,
                                              stack_min_size,
                                              check_exact)
                         for trace in trace_split
                         ]
    trace_clean_split = [trace for trace in trace_clean_split
                         if trace is not None]
    filename = os.path.basename(trace_file.name + "_filtered")
    outfile = os.path.join(out_dir_name, filename)
    with open(outfile, "w") as output_file:
        output_file.write(STACK_DIVIDER.join(trace_clean_split))

    filter_percent = 100.0 - (float(len(trace_clean_split)) /
                              len(trace_split) * 100)
    filter_amount = len(trace_split) - len(trace_clean_split)
    print("Filtered out %d (%f%%) of %d. %d (%f%%) remain."
          % (filter_amount, filter_percent, len(trace_split),
             len(trace_split) - filter_amount, 1 - filter_percent))


if __name__ == "__main__":
    main()
