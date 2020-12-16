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

"""Outputs quantitative information about Address Sanitizer traces."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from collections import Counter
from datetime import datetime
import argparse
import bisect
import os
import re


def find_match(list_substrings, big_string):
    """Returns the category a trace belongs to by searching substrings."""
    for ind, substr in enumerate(list_substrings):
        if big_string.find(substr) != -1:
            return ind
    return list_substrings.index("Uncategorized")


def absolute_to_relative(data_lists, symbol_traces):
    """Address changed to Dex File offset and shifting time to 0 min in ms."""

    offsets = data_lists["offsets"]
    time_offsets = data_lists["times"]

    # Format of time provided by logcat
    time_format_str = "%H:%M:%S.%f"
    first_access_time = datetime.strptime(data_lists["plot_list"][0][0],
                                          time_format_str)
    for ind, elem in enumerate(data_lists["plot_list"]):
        elem_date_time = datetime.strptime(elem[0], time_format_str)
        # Shift time values so that first access is at time 0 milliseconds
        elem[0] = int((elem_date_time - first_access_time).total_seconds() *
                      1000)
        address_access = int(elem[1], 16)
        # For each poisoned address, find highest Dex File starting address less
        # than address_access
        dex_start_list, dex_size_list = zip(*data_lists["dex_ends_list"])
        dex_file_ind = bisect.bisect(dex_start_list, address_access) - 1
        dex_offset = address_access - dex_start_list[dex_file_ind]
        # Assumes that offsets is already sorted and constrains offset to be
        # within range of the dex_file
        max_offset = min(offsets[1], dex_size_list[dex_file_ind])
        # Meant to nullify data that does not meet offset criteria if specified
        if (dex_offset >= offsets[0] and dex_offset < max_offset and
                elem[0] >= time_offsets[0] and elem[0] < time_offsets[1]):

            elem.insert(1, dex_offset)
            # Category that a data point belongs to
            elem.insert(2, data_lists["cat_list"][ind])
        else:
            elem[:] = 4 * [None]
            symbol_traces[ind] = None
            data_lists["cat_list"][ind] = None


def print_category_info(cat_split, outname, out_dir_name, title):
    """Prints information of category and puts related traces in a files."""
    trace_counts_dict = Counter(cat_split)
    trace_counts_list_ordered = trace_counts_dict.most_common()
    print(53 * "-")
    print(title)
    print("\tNumber of distinct traces: " +
          str(len(trace_counts_list_ordered)))
    print("\tSum of trace counts: " +
          str(sum([trace[1] for trace in trace_counts_list_ordered])))
    print("\n\tCount: How many traces appeared with count\n\t", end="")
    print(Counter([trace[1] for trace in trace_counts_list_ordered]))
    with open(os.path.join(out_dir_name, outname), "w") as output_file:
        for trace in trace_counts_list_ordered:
            output_file.write("\n\nNumber of times appeared: " +
                              str(trace[1]) +
                              "\n")
            output_file.write(trace[0].strip())


def print_categories(categories, symbol_file_split, out_dir_name):
    """Prints details of all categories."""
    symbol_file_split = [trace for trace in symbol_file_split
                         if trace is not None]
    # Info of traces containing a call to current category
    for cat_num, cat_name in enumerate(categories[1:]):
        print("\nCategory #%d" % (cat_num + 1))
        cat_split = [trace for trace in symbol_file_split
                     if cat_name in trace]
        cat_file_name = cat_name.lower() + "cat_output"
        print_category_info(cat_split, cat_file_name, out_dir_name,
                            "Traces containing: " + cat_name)
        noncat_split = [trace for trace in symbol_file_split
                        if cat_name not in trace]
        print_category_info(noncat_split, "non" + cat_file_name,
                            out_dir_name,
                            "Traces not containing: " +
                            cat_name)

    # All traces (including uncategorized) together
    print_category_info(symbol_file_split, "allcat_output",
                        out_dir_name,
                        "All traces together:")
    # Traces containing none of keywords
    # Only used if categories are passed in
    if len(categories) > 1:
        noncat_split = [trace for trace in symbol_file_split if
                        all(cat_name not in trace
                            for cat_name in categories)]
        print_category_info(noncat_split, "noncat_output",
                            out_dir_name,
                            "Uncategorized calls")


def is_directory(path_name):
    """Checks if a path is an actual directory."""
    if not os.path.isdir(path_name):
        dir_error = "%s is not a directory" % (path_name)
        raise argparse.ArgumentTypeError(dir_error)
    return path_name


def parse_args(argv):
    """Parses arguments passed in."""
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", action="store",
                        default="", dest="out_dir_name", type=is_directory,
                        help="Output Directory")
    parser.add_argument("--dex-file", action="store",
                        default=None, dest="dex_file",
                        type=argparse.FileType("r"),
                        help="Baksmali Dex File Dump")
    parser.add_argument("--offsets", action="store", nargs=2,
                        default=[float(0), float("inf")],
                        dest="offsets",
                        metavar="OFFSET",
                        type=float,
                        help="Filters out accesses not between provided"
                             " offsets if provided. Can provide 'inf'"
                             " for infinity")
    parser.add_argument("--times", action="store", nargs=2,
                        default=[float(0), float("inf")],
                        dest="times",
                        metavar="TIME",
                        type=float,
                        help="Filters out accesses not between provided"
                             " time offsets if provided. Can provide 'inf'"
                             " for infinity")
    parser.add_argument("sanitizer_trace", action="store",
                        type=argparse.FileType("r"),
                        help="File containing sanitizer traces filtered by "
                             "prune_sanitizer_output.py")
    parser.add_argument("symbol_trace", action="store",
                        type=argparse.FileType("r"),
                        help="File containing symbolized traces that match "
                             "sanitizer_trace")
    parser.add_argument("dex_starts", action="store",
                        type=argparse.FileType("r"),
                        help="File containing starting addresses of Dex Files")
    parser.add_argument("categories", action="store", nargs="*",
                        help="Keywords expected to show in large amounts of"
                             " symbolized traces")

    return parser.parse_args(argv)


def get_dex_offset_data(line, dex_file_item):
    """ Returns a tuple of dex file offset, item name, and data of a line."""
    return (int(line[:line.find(":")], 16),
            (dex_file_item,
             line.split("|")[1].strip())
            )


def read_data(parsed_argv):
    """Reads data from filepath arguments and parses them into lists."""
    # Using a dictionary to establish relation between lists added
    data_lists = {}
    categories = parsed_argv.categories
    # Makes sure each trace maps to some category
    categories.insert(0, "Uncategorized")

    data_lists["offsets"] = parsed_argv.offsets
    data_lists["offsets"].sort()

    data_lists["times"] = parsed_argv.times
    data_lists["times"].sort()

    logcat_file_data = parsed_argv.sanitizer_trace.readlines()
    parsed_argv.sanitizer_trace.close()

    symbol_file_split = parsed_argv.symbol_trace.read().split("Stack Trace")
    # Removes text before first trace
    symbol_file_split = symbol_file_split[1:]
    parsed_argv.symbol_trace.close()

    dex_start_file_data = parsed_argv.dex_starts.readlines()
    parsed_argv.dex_starts.close()

    if parsed_argv.dex_file is not None:
        dex_file_data = parsed_argv.dex_file.read()
        parsed_argv.dex_file.close()
        # Splits baksmali dump by each item
        item_split = [s.splitlines() for s in re.split(r"\|\[[0-9]+\] ",
                                                       dex_file_data)]
        # Splits each item by line and creates a list of offsets and a
        # corresponding list of the data associated with that line
        offset_list, offset_data = zip(*[get_dex_offset_data(line, item[0])
                                         for item in item_split
                                         for line in item[1:]
                                         if re.search("[0-9a-f]{6}:", line)
                                         is not None and
                                         line.find("|") != -1])
        data_lists["offset_list"] = offset_list
        data_lists["offset_data"] = offset_data
    else:
        dex_file_data = None

    # Each element is a tuple of time and address accessed
    data_lists["plot_list"] = [[elem[1] for elem in enumerate(line.split())
                                if elem[0] in (1, 11)
                                ]
                               for line in logcat_file_data
                               if "use-after-poison" in line or
                               "unknown-crash" in line
                               ]
    # Contains a mapping between traces and the category they belong to
    # based on arguments
    data_lists["cat_list"] = [categories[find_match(categories, trace)]
                              for trace in symbol_file_split]

    # Contains a list of starting address of all dex files to calculate dex
    # offsets
    data_lists["dex_ends_list"] = [(int(line.split()[9], 16),
                                    int(line.split()[12])
                                    )
                                   for line in dex_start_file_data
                                   if "RegisterDexFile" in line
                                   ]
    # Dex File Starting addresses must be sorted because bisect requires sorted
    # lists.
    data_lists["dex_ends_list"].sort()

    return data_lists, categories, symbol_file_split


def main():
    """Takes in trace information and outputs details about them."""
    parsed_argv = parse_args(None)
    data_lists, categories, symbol_file_split = read_data(parsed_argv)

    # Formats plot_list such that each element is a data point
    absolute_to_relative(data_lists, symbol_file_split)
    for file_ext, cat_name in enumerate(categories):
        out_file_name = os.path.join(parsed_argv.out_dir_name, "time_output_" +
                                     str(file_ext) +
                                     ".dat")
        with open(out_file_name, "w") as output_file:
            output_file.write("# Category: " + cat_name + "\n")
            output_file.write("# Time, Dex File Offset_10, Dex File Offset_16,"
                              " Address, Item Accessed, Item Member Accessed"
                              " Unaligned\n")
            for time, dex_offset, category, address in data_lists["plot_list"]:
                if category == cat_name:
                    output_file.write(
                        str(time) +
                        " " +
                        str(dex_offset) +
                        " #" +
                        hex(dex_offset) +
                        " " +
                        str(address))
                    if "offset_list" in data_lists:
                        dex_offset_index = bisect.bisect(
                            data_lists["offset_list"],
                            dex_offset) - 1
                        aligned_dex_offset = (data_lists["offset_list"]
                                                        [dex_offset_index])
                        dex_offset_data = (data_lists["offset_data"]
                                                     [dex_offset_index])
                        output_file.write(
                            " " +
                            "|".join(dex_offset_data) +
                            " " +
                            str(aligned_dex_offset != dex_offset))
                    output_file.write("\n")
    print_categories(categories, symbol_file_split, parsed_argv.out_dir_name)


if __name__ == "__main__":
    main()
