/*
 * Copyright 2015, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Create a test file in the format required by dmtrace.
 */
#include "profile.h"  // from VM header

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/*
 * Values from the header of the data file.
 */
typedef struct DataHeader {
  uint32_t magic;
  int16_t version;
  int16_t offsetToData;
  int64_t startWhen;
} DataHeader;

#define VERSION 2
int32_t versionNumber = VERSION;
int32_t verbose = 0;

DataHeader header = {0x574f4c53, VERSION, sizeof(DataHeader), 0LL};

const char* versionHeader = "*version\n";
const char* clockDef = "clock=thread-cpu\n";

const char* keyThreads =
    "*threads\n"
    "1      main\n"
    "2      foo\n"
    "3      bar\n"
    "4      blah\n";

const char* keyEnd = "*end\n";

typedef struct dataRecord {
  uint32_t time;
  int32_t threadId;
  uint32_t action; /* 0=entry, 1=exit, 2=exception exit */
  char* fullName;
  char* className;
  char* methodName;
  char* signature;
  uint32_t methodId;
} dataRecord;

dataRecord* records;

#define BUF_SIZE 1024
char buf[BUF_SIZE];

typedef struct stack {
  dataRecord** frames;
  int32_t indentLevel;
} stack;

/* Mac OS doesn't have strndup(), so implement it here.
 */
char* strndup(const char* src, size_t len) {
  char* dest = new char[len + 1];
  strncpy(dest, src, len);
  dest[len] = 0;
  return dest;
}

/*
 * Parse the input file.  It looks something like this:
 * # This is a comment line
 * 4  1 A
 * 6  1  B
 * 8  1  B
 * 10 1 A
 *
 * where the first column is the time, the second column is the thread id,
 * and the third column is the method (actually just the class name).  The
 * number of spaces between the 2nd and 3rd columns is the indentation and
 * determines the call stack.  Each called method must be indented by one
 * more space.  In the example above, A is called at time 4, A calls B at
 * time 6, B returns at time 8, and A returns at time 10.  Thread 1 is the
 * only thread that is running.
 *
 * An alternative file format leaves out the first two columns:
 * A
 *  B
 *  B
 * A
 *
 * In this file format, the thread id is always 1, and the time starts at
 * 2 and increments by 2 for each line.
 */
void parseInputFile(const char* inputFileName) {
  FILE* inputFp = fopen(inputFileName, "r");
  if (inputFp == nullptr) {
    perror(inputFileName);
    exit(1);
  }

  /* Count the number of lines in the buffer */
  int32_t numRecords = 0;
  int32_t maxThreadId = 1;
  int32_t maxFrames = 0;
  char* indentEnd;
  while (fgets(buf, BUF_SIZE, inputFp)) {
    char* cp = buf;
    if (*cp == '#') continue;
    numRecords += 1;
    if (isdigit(*cp)) {
      while (isspace(*cp)) cp += 1;
      int32_t threadId = strtoul(cp, &cp, 0);
      if (maxThreadId < threadId) maxThreadId = threadId;
    }
    indentEnd = cp;
    while (isspace(*indentEnd)) indentEnd += 1;
    if (indentEnd - cp + 1 > maxFrames) maxFrames = indentEnd - cp + 1;
  }
  int32_t numThreads = maxThreadId + 1;

  /* Add space for a sentinel record at the end */
  numRecords += 1;
  records = new dataRecord[numRecords];
  stack* callStack = new stack[numThreads];
  for (int32_t ii = 0; ii < numThreads; ++ii) {
    callStack[ii].frames = nullptr;
    callStack[ii].indentLevel = 0;
  }

  rewind(inputFp);

  uint32_t time = 0;
  int32_t linenum = 0;
  int32_t nextRecord = 0;
  int32_t indentLevel = 0;
  while (fgets(buf, BUF_SIZE, inputFp)) {
    uint32_t threadId;
    int32_t len;
    int32_t indent;
    int32_t action;
    char* save_cp;

    linenum += 1;
    char* cp = buf;

    /* Skip lines that start with '#' */
    if (*cp == '#') continue;

    /* Get time and thread id */
    if (!isdigit(*cp)) {
      /* If the line does not begin with a digit, then fill in
       * default values for the time and threadId.
       */
      time += 2;
      threadId = 1;
    } else {
      time = strtoul(cp, &cp, 0);
      while (isspace(*cp)) cp += 1;
      threadId = strtoul(cp, &cp, 0);
      cp += 1;
    }

    // Allocate space for the thread stack, if necessary
    if (callStack[threadId].frames == nullptr) {
      dataRecord** stk = new dataRecord*[maxFrames];
      callStack[threadId].frames = stk;
    }
    indentLevel = callStack[threadId].indentLevel;

    save_cp = cp;
    while (isspace(*cp)) {
      cp += 1;
    }
    indent = cp - save_cp + 1;
    records[nextRecord].time = time;
    records[nextRecord].threadId = threadId;

    save_cp = cp;
    while (*cp != '\n') cp += 1;

    /* Remove trailing spaces */
    cp -= 1;
    while (isspace(*cp)) cp -= 1;
    cp += 1;
    len = cp - save_cp;
    records[nextRecord].fullName = strndup(save_cp, len);

    /* Parse the name to support "class.method signature" */
    records[nextRecord].className = nullptr;
    records[nextRecord].methodName = nullptr;
    records[nextRecord].signature = nullptr;
    cp = strchr(save_cp, '.');
    if (cp) {
      len = cp - save_cp;
      if (len > 0) records[nextRecord].className = strndup(save_cp, len);
      save_cp = cp + 1;
      cp = strchr(save_cp, ' ');
      if (cp == nullptr) cp = strchr(save_cp, '\n');
      if (cp && cp > save_cp) {
        len = cp - save_cp;
        records[nextRecord].methodName = strndup(save_cp, len);
        save_cp = cp + 1;
        cp = strchr(save_cp, ' ');
        if (cp == nullptr) cp = strchr(save_cp, '\n');
        if (cp && cp > save_cp) {
          len = cp - save_cp;
          records[nextRecord].signature = strndup(save_cp, len);
        }
      }
    }

    if (verbose) {
      printf("Indent: %d; IndentLevel: %d; Line: %s", indent, indentLevel, buf);
    }

    action = 0;
    if (indent == indentLevel + 1) {  // Entering a method
      if (verbose) printf("  Entering %s\n", records[nextRecord].fullName);
      callStack[threadId].frames[indentLevel] = &records[nextRecord];
    } else if (indent == indentLevel) {  // Exiting a method
      // Exiting method must be currently on top of stack (unless stack is
      // empty)
      if (callStack[threadId].frames[indentLevel - 1] == nullptr) {
        if (verbose)
          printf("  Exiting %s (past bottom of stack)\n",
                 records[nextRecord].fullName);
        callStack[threadId].frames[indentLevel - 1] = &records[nextRecord];
        action = 1;
      } else {
        if (indentLevel < 1) {
          fprintf(stderr, "Error: line %d: %s", linenum, buf);
          fprintf(stderr, "  expected positive (>0) indentation, found %d\n",
                  indent);
          exit(1);
        }
        char* name = callStack[threadId].frames[indentLevel - 1]->fullName;
        if (strcmp(name, records[nextRecord].fullName) == 0) {
          if (verbose) printf("  Exiting %s\n", name);
          action = 1;
        } else {  // exiting method doesn't match stack's top method
          fprintf(stderr, "Error: line %d: %s", linenum, buf);
          fprintf(stderr, "  expected exit from %s\n",
                  callStack[threadId].frames[indentLevel - 1]->fullName);
          exit(1);
        }
      }
    } else {
      if (nextRecord != 0) {
        fprintf(stderr, "Error: line %d: %s", linenum, buf);
        fprintf(stderr, "  expected indentation %d [+1], found %d\n",
                indentLevel, indent);
        exit(1);
      }

      if (verbose) {
        printf("  Nonzero indent at first record\n");
        printf("  Entering %s\n", records[nextRecord].fullName);
      }

      // This is the first line of data, so we allow a larger
      // initial indent.  This allows us to test popping off more
      // frames than we entered.
      indentLevel = indent - 1;
      callStack[threadId].frames[indentLevel] = &records[nextRecord];
    }

    if (action == 0)
      indentLevel += 1;
    else
      indentLevel -= 1;
    records[nextRecord].action = action;
    callStack[threadId].indentLevel = indentLevel;

    nextRecord += 1;
  }

  /* Mark the last record with a sentinel */
  memset(&records[nextRecord], 0, sizeof(dataRecord));
}

/*
 * Write values to the binary data file.
 */
void write2LE(FILE* fp, uint16_t val) {
  putc(val & 0xff, fp);
  putc(val >> 8, fp);
}

void write4LE(FILE* fp, uint32_t val) {
  putc(val & 0xff, fp);
  putc((val >> 8) & 0xff, fp);
  putc((val >> 16) & 0xff, fp);
  putc((val >> 24) & 0xff, fp);
}

void write8LE(FILE* fp, uint64_t val) {
  putc(val & 0xff, fp);
  putc((val >> 8) & 0xff, fp);
  putc((val >> 16) & 0xff, fp);
  putc((val >> 24) & 0xff, fp);
  putc((val >> 32) & 0xff, fp);
  putc((val >> 40) & 0xff, fp);
  putc((val >> 48) & 0xff, fp);
  putc((val >> 56) & 0xff, fp);
}

void writeDataRecord(FILE* dataFp, int32_t threadId, uint32_t methodVal, uint32_t elapsedTime) {
  if (versionNumber == 1)
    putc(threadId, dataFp);
  else
    write2LE(dataFp, threadId);
  write4LE(dataFp, methodVal);
  write4LE(dataFp, elapsedTime);
}

void writeDataHeader(FILE* dataFp) {
  struct timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);
  uint64_t startTime = tv.tv_sec;
  startTime = (startTime << 32) | tv.tv_usec;
  header.version = versionNumber;
  write4LE(dataFp, header.magic);
  write2LE(dataFp, header.version);
  write2LE(dataFp, header.offsetToData);
  write8LE(dataFp, startTime);
}

void writeKeyMethods(FILE* keyFp) {
  const char* methodStr = "*methods\n";
  fwrite(methodStr, strlen(methodStr), 1, keyFp);

  /* Assign method ids in multiples of 4 */
  uint32_t methodId = 0;
  for (dataRecord* pRecord = records; pRecord->fullName; ++pRecord) {
    if (pRecord->methodId) continue;
    uint32_t id = ++methodId << 2;
    pRecord->methodId = id;

    /* Assign this id to all the other records that have the
     * same name.
     */
    for (dataRecord* pNext = pRecord + 1; pNext->fullName; ++pNext) {
      if (pNext->methodId) continue;
      if (strcmp(pRecord->fullName, pNext->fullName) == 0) pNext->methodId = id;
    }
    if (pRecord->className == nullptr || pRecord->methodName == nullptr) {
      fprintf(keyFp, "%#x        %s      m       ()\n", pRecord->methodId,
              pRecord->fullName);
    } else if (pRecord->signature == nullptr) {
      fprintf(keyFp, "%#x        %s      %s      ()\n", pRecord->methodId,
              pRecord->className, pRecord->methodName);
    } else {
      fprintf(keyFp, "%#x        %s      %s      %s\n", pRecord->methodId,
              pRecord->className, pRecord->methodName, pRecord->signature);
    }
  }
}

void writeKeys(FILE* keyFp) {
  fprintf(keyFp, "%s%d\n%s", versionHeader, versionNumber, clockDef);
  fwrite(keyThreads, strlen(keyThreads), 1, keyFp);
  writeKeyMethods(keyFp);
  fwrite(keyEnd, strlen(keyEnd), 1, keyFp);
}

void writeDataRecords(FILE* dataFp) {
  for (dataRecord* pRecord = records; pRecord->fullName; ++pRecord) {
    uint32_t val = METHOD_COMBINE(pRecord->methodId, pRecord->action);
    writeDataRecord(dataFp, pRecord->threadId, val, pRecord->time);
  }
}

void writeTrace(const char* traceFileName) {
  FILE* fp = fopen(traceFileName, "w");
  if (fp == nullptr) {
    perror(traceFileName);
    exit(1);
  }
  writeKeys(fp);
  writeDataHeader(fp);
  writeDataRecords(fp);
  fclose(fp);
}

int32_t parseOptions(int32_t argc, char** argv) {
  int32_t err = 0;
  while (1) {
    int32_t opt = getopt(argc, argv, "v:d");
    if (opt == -1) break;
    switch (opt) {
      case 'v':
        versionNumber = strtoul(optarg, nullptr, 0);
        if (versionNumber != 1 && versionNumber != 2) {
          fprintf(stderr, "Error: version number (%d) must be 1 or 2\n", versionNumber);
          err = 1;
        }
        break;
      case 'd':
        verbose = 1;
        break;
      default:
        err = 1;
        break;
    }
  }
  return err;
}

int32_t main(int32_t argc, char** argv) {
  char* inputFile;
  char* traceFileName = nullptr;

  if (parseOptions(argc, argv) || argc - optind != 2) {
    fprintf(stderr, "Usage: %s [-v version] [-d] input_file trace_prefix\n", argv[0]);
    exit(1);
  }

  inputFile = argv[optind++];
  parseInputFile(inputFile);
  traceFileName = argv[optind++];

  writeTrace(traceFileName);

  return 0;
}
