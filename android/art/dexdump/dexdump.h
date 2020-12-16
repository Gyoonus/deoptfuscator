/*
 * Copyright (C) 2015 The Android Open Source Project
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
 * Header file of the dexdump utility.
 *
 * This is a re-implementation of the original dexdump utility that was
 * based on Dalvik functions in libdex into a new dexdump that is now
 * based on Art functions in libart instead. The output is identical to
 * the original for correct DEX files. Error messages may differ, however.
 * Also, ODEX files are no longer supported.
 */

#ifndef ART_DEXDUMP_DEXDUMP_H_
#define ART_DEXDUMP_DEXDUMP_H_

#include <stdint.h>
#include <stdio.h>

namespace art {

/* Supported output formats. */
enum OutputFormat {
  OUTPUT_PLAIN = 0,  // default
  OUTPUT_XML,        // XML-style
};

/* Command-line options. */
struct Options {
  bool checksumOnly;
  bool disassemble;
  bool exportsOnly;
  bool ignoreBadChecksum;
  bool disableVerifier;
  bool showAnnotations;
  bool showCfg;
  bool showFileHeaders;
  bool showSectionHeaders;
  bool verbose;
  OutputFormat outputFormat;
  const char* outputFileName;
};

/* Prototypes. */
extern struct Options gOptions;
extern FILE* gOutFile;
int processFile(const char* fileName);

}  // namespace art

#endif  // ART_DEXDUMP_DEXDUMP_H_
