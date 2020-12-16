  /*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Main driver of the dexlayout utility.
 *
 * This is a tool to read dex files into an internal representation,
 * reorganize the representation, and emit dex files with a better
 * file layout.
 */

#include "dexlayout.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <android-base/logging.h>

#include "base/logging.h"  // For InitLogging.
#include "jit/profile_compilation_info.h"
#include "mem_map.h"
#include "runtime.h"

namespace art {

static const char* kProgramName = "dexlayout";

/*
 * Shows usage.
 */
static void Usage(void) {
  LOG(ERROR) << "Copyright (C) 2016 The Android Open Source Project\n";
  LOG(ERROR) << kProgramName
             << ": [-a] [-c] [-d] [-e] [-f] [-h] [-i] [-l layout] [-o outfile] [-p profile]"
                " [-s] [-t] [-v] [-w directory] dexfile...\n";
  LOG(ERROR) << " -a : display annotations";
  LOG(ERROR) << " -b : build dex_ir";
  LOG(ERROR) << " -c : verify checksum and exit";
  LOG(ERROR) << " -d : disassemble code sections";
  LOG(ERROR) << " -e : display exported items only";
  LOG(ERROR) << " -f : display summary information from file header";
  LOG(ERROR) << " -h : display file header details";
  LOG(ERROR) << " -i : ignore checksum failures";
  LOG(ERROR) << " -l : output layout, either 'plain' or 'xml'";
  LOG(ERROR) << " -o : output file name (defaults to stdout)";
  LOG(ERROR) << " -p : profile file name (defaults to no profile)";
  LOG(ERROR) << " -s : visualize reference pattern";
  LOG(ERROR) << " -t : display file section sizes";
  LOG(ERROR) << " -v : verify output file is canonical to input (IR level comparison)";
  LOG(ERROR) << " -w : output dex directory";
  LOG(ERROR) << " -x : compact dex generation level, either 'none' or 'fast'";
}

/*
 * Main driver of the dexlayout utility.
 */
int DexlayoutDriver(int argc, char** argv) {
  // Art specific set up.
  InitLogging(argv, Runtime::Abort);
  MemMap::Init();

  Options options;
  options.dump_ = true;
  options.verbose_ = true;
  bool want_usage = false;

  // Parse all arguments.
  while (1) {
    const int ic = getopt(argc, argv, "abcdefghil:o:p:stvw:x:");
    if (ic < 0) {
      break;  // done
    }
    switch (ic) {
      case 'a':  // display annotations
        options.show_annotations_ = true;
        break;
      case 'b':  // build dex_ir
        options.build_dex_ir_ = true;
        break;
      case 'c':  // verify the checksum then exit
        options.checksum_only_ = true;
        break;
      case 'd':  // disassemble Dalvik instructions
        options.disassemble_ = true;
        break;
      case 'e':  // exported items only
        options.exports_only_ = true;
        break;
      case 'f':  // display outer file header
        options.show_file_headers_ = true;
        break;
      case 'h':  // display section headers, i.e. all meta-data
        options.show_section_headers_ = true;
        break;
      case 'i':  // continue even if checksum is bad
        options.ignore_bad_checksum_ = true;
        break;
      case 'l':  // layout
        if (strcmp(optarg, "plain") == 0) {
          options.output_format_ = kOutputPlain;
        } else if (strcmp(optarg, "xml") == 0) {
          options.output_format_ = kOutputXml;
          options.verbose_ = false;
        } else {
          want_usage = true;
        }
        break;
      case 'o':  // output file
        options.output_file_name_ = optarg;
        break;
      case 'p':  // profile file
        options.profile_file_name_ = optarg;
        break;
      case 's':  // visualize access pattern
        options.visualize_pattern_ = true;
        options.verbose_ = false;
        break;
      case 't':  // display section statistics
        options.show_section_statistics_ = true;
        options.verbose_ = false;
        break;
      case 'v':  // verify output
        options.verify_output_ = true;
        break;
      case 'w':  // output dex files directory
        options.output_dex_directory_ = optarg;
        break;
      case 'x':  // compact dex level
        if (strcmp(optarg, "none") == 0) {
          options.compact_dex_level_ = CompactDexLevel::kCompactDexLevelNone;
        } else if (strcmp(optarg, "fast") == 0) {
          options.compact_dex_level_ = CompactDexLevel::kCompactDexLevelFast;
        } else {
          want_usage = true;
        }
        break;
      default:
        want_usage = true;
        break;
    }  // switch
  }  // while

  // Detect early problems.
  if (optind == argc) {
    LOG(ERROR) << "no file specified";
    want_usage = true;
  }
  if (options.checksum_only_ && options.ignore_bad_checksum_) {
    LOG(ERROR) << "Can't specify both -c and -i";
    want_usage = true;
  }
  if (want_usage) {
    Usage();
    return 2;
  }

  // Open alternative output file.
  FILE* out_file = stdout;
  if (options.output_file_name_) {
    out_file = fopen(options.output_file_name_, "w");
    if (!out_file) {
      PLOG(ERROR) << "Can't open " << options.output_file_name_;
      return 1;
    }
  }

  // Open profile file.
  std::unique_ptr<ProfileCompilationInfo> profile_info;
  if (options.profile_file_name_) {
    int profile_fd = open(options.profile_file_name_, O_RDONLY);
    if (profile_fd < 0) {
      PLOG(ERROR) << "Can't open " << options.profile_file_name_;
      return 1;
    }
    profile_info.reset(new ProfileCompilationInfo());
    if (!profile_info->Load(profile_fd)) {
      LOG(ERROR) << "Can't read profile info from " << options.profile_file_name_;
      return 1;
    }
  }

  // Create DexLayout instance.
  DexLayout dex_layout(options, profile_info.get(), out_file, /*header*/ nullptr);

  // Process all files supplied on command line.
  int result = 0;
  while (optind < argc) {
    result |= dex_layout.ProcessFile(argv[optind++]);
  }  // while

  if (options.output_file_name_) {
    CHECK(out_file != nullptr && out_file != stdout);
    fclose(out_file);
  }

  return result != 0;
}

}  // namespace art

int main(int argc, char** argv) {
  // Output all logging to stderr.
  android::base::SetLogger(android::base::StderrLogger);

  return art::DexlayoutDriver(argc, argv);
}
