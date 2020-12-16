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
 */

/*
 * Process dmtrace output.
 *
 * This is the wrong way to go about it -- C is a clumsy language for
 * shuffling data around.  It'll do for a first pass.
 */
#include "profile.h"  // from VM header

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Version number in the key file.
 * Version 1 uses one byte for the thread id.
 * Version 2 uses two bytes for the thread ids.
 * Version 3 encodes the record size and adds an optional extra timestamp field.
 */
int32_t versionNumber;

/* arbitrarily limit indentation */
#define MAX_STACK_DEPTH 10000

/* thread list in key file is not reliable, so just max out */
#define MAX_THREADS 32768

/* Size of temporary buffers for escaping html strings */
#define HTML_BUFSIZE 10240

const char* htmlHeader =
    "<html>\n<head>\n<script type=\"text/javascript\" "
    "src=\"%ssortable.js\"></script>\n"
    "<script langugage=\"javascript\">\n"
    "function toggle(item) {\n"
    "    obj=document.getElementById(item);\n"
    "    visible=(obj.style.display!=\"none\" && obj.style.display!=\"\");\n"
    "    key=document.getElementById(\"x\" + item);\n"
    "    if (visible) {\n"
    "        obj.style.display=\"none\";\n"
    "        key.innerHTML=\"+\";\n"
    "    } else {\n"
    "        obj.style.display=\"block\";\n"
    "        key.innerHTML=\"-\";\n"
    "    }\n"
    "}\n"
    "function onMouseOver(obj) {\n"
    "    obj.style.background=\"lightblue\";\n"
    "}\n"
    "function onMouseOut(obj) {\n"
    "    obj.style.background=\"white\";\n"
    "}\n"
    "</script>\n"
    "<style type=\"text/css\">\n"
    "div { font-family: courier; font-size: 13 }\n"
    "div.parent { margin-left: 15; display: none }\n"
    "div.leaf { margin-left: 10 }\n"
    "div.header { margin-left: 10 }\n"
    "div.link { margin-left: 10; cursor: move }\n"
    "span.parent { padding-right: 10; }\n"
    "span.leaf { padding-right: 10; }\n"
    "a img { border: 0;}\n"
    "table.sortable th { border-width: 0px 1px 1px 1px; background-color: "
    "#ccc;}\n"
    "a { text-decoration: none; }\n"
    "a:hover { text-decoration: underline; }\n"
    "table.sortable th, table.sortable td { text-align: left;}"
    "table.sortable tr.odd td { background-color: #ddd; }\n"
    "table.sortable tr.even td { background-color: #fff; }\n"
    "</style>\n"
    "</head><body>\n\n";

const char* htmlFooter = "\n</body>\n</html>\n";
const char* profileSeparator =
    "======================================================================";

const char* tableHeader =
    "<table class='sortable' id='%s'><tr>\n"
    "<th>Method</th>\n"
    "<th>Run 1 (us)</th>\n"
    "<th>Run 2 (us)</th>\n"
    "<th>Diff (us)</th>\n"
    "<th>Diff (%%)</th>\n"
    "<th>1: # calls</th>\n"
    "<th>2: # calls</th>\n"
    "</tr>\n";

const char* tableHeaderMissing =
    "<table class='sortable' id='%s'>\n"
    "<th>Method</th>\n"
    "<th>Exclusive</th>\n"
    "<th>Inclusive</th>\n"
    "<th># calls</th>\n";

#define GRAPH_LABEL_VISITED 0x0001
#define GRAPH_NODE_VISITED 0x0002

/*
 * Values from the header of the data file.
 */
typedef struct DataHeader {
  uint32_t magic;
  int16_t version;
  int16_t offsetToData;
  int64_t startWhen;
  int16_t recordSize;
} DataHeader;

/*
 * Entry from the thread list.
 */
typedef struct ThreadEntry {
  int32_t threadId;
  const char* threadName;
} ThreadEntry;

struct MethodEntry;
typedef struct TimedMethod {
  struct TimedMethod* next;
  uint64_t elapsedInclusive;
  int32_t numCalls;
  struct MethodEntry* method;
} TimedMethod;

typedef struct ClassEntry {
  const char* className;
  uint64_t elapsedExclusive;
  int32_t numMethods;
  struct MethodEntry** methods; /* list of methods in this class */
  int32_t numCalls[2];              /* 0=normal, 1=recursive */
} ClassEntry;

typedef struct UniqueMethodEntry {
  uint64_t elapsedExclusive;
  int32_t numMethods;
  struct MethodEntry** methods; /* list of methods with same name */
  int32_t numCalls[2];              /* 0=normal, 1=recursive */
} UniqueMethodEntry;

/*
 * Entry from the method list.
 */
typedef struct MethodEntry {
  int64_t methodId;
  const char* className;
  const char* methodName;
  const char* signature;
  const char* fileName;
  int32_t lineNum;
  uint64_t elapsedExclusive;
  uint64_t elapsedInclusive;
  uint64_t topExclusive; /* non-recursive exclusive time */
  uint64_t recursiveInclusive;
  struct TimedMethod* parents[2];  /* 0=normal, 1=recursive */
  struct TimedMethod* children[2]; /* 0=normal, 1=recursive */
  int32_t numCalls[2];             /* 0=normal, 1=recursive */
  int32_t index;                   /* used after sorting to number methods */
  int32_t recursiveEntries;        /* number of entries on the stack */
  int32_t graphState; /* used when graphing to see if this method has been visited before */
} MethodEntry;

/*
 * The parsed contents of the key file.
 */
typedef struct DataKeys {
  char* fileData; /* contents of the entire file */
  int64_t fileLen;
  int32_t numThreads;
  ThreadEntry* threads;
  int32_t numMethods;
  MethodEntry* methods; /* 2 extra methods: "toplevel" and "unknown" */
} DataKeys;

#define TOPLEVEL_INDEX 0
#define UNKNOWN_INDEX 1

typedef struct StackEntry {
  MethodEntry* method;
  uint64_t entryTime;
} StackEntry;

typedef struct CallStack {
  int32_t top;
  StackEntry calls[MAX_STACK_DEPTH];
  uint64_t lastEventTime;
  uint64_t threadStartTime;
} CallStack;

typedef struct DiffEntry {
  MethodEntry* method1;
  MethodEntry* method2;
  int64_t differenceExclusive;
  int64_t differenceInclusive;
  double differenceExclusivePercentage;
  double differenceInclusivePercentage;
} DiffEntry;

// Global options
typedef struct Options {
  const char* traceFileName;
  const char* diffFileName;
  const char* graphFileName;
  int32_t keepDotFile;
  int32_t dump;
  int32_t outputHtml;
  const char* sortableUrl;
  int32_t threshold;
} Options;

typedef struct TraceData {
  int32_t numClasses;
  ClassEntry* classes;
  CallStack* stacks[MAX_THREADS];
  int32_t depth[MAX_THREADS];
  int32_t numUniqueMethods;
  UniqueMethodEntry* uniqueMethods;
} TraceData;

static Options gOptions;

/* Escapes characters in the source string that are html special entities.
 * The escaped string is written to "dest" which must be large enough to
 * hold the result.  A pointer to "dest" is returned.  The characters and
 * their corresponding escape sequences are:
 *  '<'  &lt;
 *  '>'  &gt;
 *  '&'  &amp;
 */
char* htmlEscape(const char* src, char* dest, int32_t len) {
  char* destStart = dest;

  if (src == nullptr) return nullptr;

  int32_t nbytes = 0;
  while (*src) {
    if (*src == '<') {
      nbytes += 4;
      if (nbytes >= len) break;
      *dest++ = '&';
      *dest++ = 'l';
      *dest++ = 't';
      *dest++ = ';';
    } else if (*src == '>') {
      nbytes += 4;
      if (nbytes >= len) break;
      *dest++ = '&';
      *dest++ = 'g';
      *dest++ = 't';
      *dest++ = ';';
    } else if (*src == '&') {
      nbytes += 5;
      if (nbytes >= len) break;
      *dest++ = '&';
      *dest++ = 'a';
      *dest++ = 'm';
      *dest++ = 'p';
      *dest++ = ';';
    } else {
      nbytes += 1;
      if (nbytes >= len) break;
      *dest++ = *src;
    }
    src += 1;
  }
  if (nbytes >= len) {
    fprintf(stderr, "htmlEscape(): buffer overflow\n");
    exit(1);
  }
  *dest = 0;

  return destStart;
}

/* Initializes a MethodEntry
 */
void initMethodEntry(MethodEntry* method, int64_t methodId, const char* className,
                     const char* methodName, const char* signature, const char* fileName,
                     const char* lineNumStr) {
  method->methodId = methodId;
  method->className = className;
  method->methodName = methodName;
  method->signature = signature;
  method->fileName = fileName;
  method->lineNum = (lineNumStr != nullptr) ? atoi(lineNumStr) : -1;
  method->elapsedExclusive = 0;
  method->elapsedInclusive = 0;
  method->topExclusive = 0;
  method->recursiveInclusive = 0;
  method->parents[0] = nullptr;
  method->parents[1] = nullptr;
  method->children[0] = nullptr;
  method->children[1] = nullptr;
  method->numCalls[0] = 0;
  method->numCalls[1] = 0;
  method->index = 0;
  method->recursiveEntries = 0;
}

/*
 * This comparison function is called from qsort() to sort
 * methods into decreasing order of exclusive elapsed time.
 */
int32_t compareElapsedExclusive(const void* a, const void* b) {
  const MethodEntry* methodA = *(const MethodEntry**) a;
  const MethodEntry* methodB = *(const MethodEntry**) b;
  uint64_t elapsed1 = methodA->elapsedExclusive;
  uint64_t elapsed2 = methodB->elapsedExclusive;
  if (elapsed1 < elapsed2) return 1;
  if (elapsed1 > elapsed2) return -1;

  /* If the elapsed times of two methods are equal, then sort them
   * into alphabetical order.
   */
  int32_t result = strcmp(methodA->className, methodB->className);
  if (result == 0) {
    if (methodA->methodName == nullptr || methodB->methodName == nullptr) {
      int64_t idA = methodA->methodId;
      int64_t idB = methodB->methodId;
      if (idA < idB) return -1;
      if (idA > idB) return 1;
      return 0;
    }
    result = strcmp(methodA->methodName, methodB->methodName);
    if (result == 0) result = strcmp(methodA->signature, methodB->signature);
  }
  return result;
}

/*
 * This comparison function is called from qsort() to sort
 * methods into decreasing order of inclusive elapsed time.
 */
int32_t compareElapsedInclusive(const void* a, const void* b) {
  const MethodEntry* methodA = *(MethodEntry const**) a;
  const MethodEntry* methodB = *(MethodEntry const**) b;
  uint64_t elapsed1 = methodA->elapsedInclusive;
  uint64_t elapsed2 = methodB->elapsedInclusive;
  if (elapsed1 < elapsed2) return 1;
  if (elapsed1 > elapsed2) return -1;

  /* If the elapsed times of two methods are equal, then sort them
   * into alphabetical order.
   */
  int32_t result = strcmp(methodA->className, methodB->className);
  if (result == 0) {
    if (methodA->methodName == nullptr || methodB->methodName == nullptr) {
      int64_t idA = methodA->methodId;
      int64_t idB = methodB->methodId;
      if (idA < idB) return -1;
      if (idA > idB) return 1;
      return 0;
    }
    result = strcmp(methodA->methodName, methodB->methodName);
    if (result == 0) result = strcmp(methodA->signature, methodB->signature);
  }
  return result;
}

/*
 * This comparison function is called from qsort() to sort
 * TimedMethods into decreasing order of inclusive elapsed time.
 */
int32_t compareTimedMethod(const void* a, const void* b) {
  const TimedMethod* timedA = (TimedMethod const*) a;
  const TimedMethod* timedB = (TimedMethod const*) b;
  uint64_t elapsed1 = timedA->elapsedInclusive;
  uint64_t elapsed2 = timedB->elapsedInclusive;
  if (elapsed1 < elapsed2) return 1;
  if (elapsed1 > elapsed2) return -1;

  /* If the elapsed times of two methods are equal, then sort them
   * into alphabetical order.
   */
  MethodEntry* methodA = timedA->method;
  MethodEntry* methodB = timedB->method;
  int32_t result = strcmp(methodA->className, methodB->className);
  if (result == 0) {
    if (methodA->methodName == nullptr || methodB->methodName == nullptr) {
      int64_t idA = methodA->methodId;
      int64_t idB = methodB->methodId;
      if (idA < idB) return -1;
      if (idA > idB) return 1;
      return 0;
    }
    result = strcmp(methodA->methodName, methodB->methodName);
    if (result == 0) result = strcmp(methodA->signature, methodB->signature);
  }
  return result;
}

/*
 * This comparison function is called from qsort() to sort
 * MethodEntry pointers into alphabetical order of class names.
 */
int32_t compareClassNames(const void* a, const void* b) {
  const MethodEntry* methodA = *(const MethodEntry**) a;
  const MethodEntry* methodB = *(const MethodEntry**) b;
  int32_t result = strcmp(methodA->className, methodB->className);
  if (result == 0) {
    int64_t idA = methodA->methodId;
    int64_t idB = methodB->methodId;
    if (idA < idB) return -1;
    if (idA > idB) return 1;
    return 0;
  }
  return result;
}

/*
 * This comparison function is called from qsort() to sort
 * classes into decreasing order of exclusive elapsed time.
 */
int32_t compareClassExclusive(const void* a, const void* b) {
  const ClassEntry* classA = *(const ClassEntry**) a;
  const ClassEntry* classB = *(const ClassEntry**) b;
  uint64_t elapsed1 = classA->elapsedExclusive;
  uint64_t elapsed2 = classB->elapsedExclusive;
  if (elapsed1 < elapsed2) return 1;
  if (elapsed1 > elapsed2) return -1;

  /* If the elapsed times of two classs are equal, then sort them
   * into alphabetical order.
   */
  int32_t result = strcmp(classA->className, classB->className);
  if (result == 0) {
    /* Break ties with the first method id.  This is probably not
     * needed.
     */
    int64_t idA = classA->methods[0]->methodId;
    int64_t idB = classB->methods[0]->methodId;
    if (idA < idB) return -1;
    if (idA > idB) return 1;
    return 0;
  }
  return result;
}

/*
 * This comparison function is called from qsort() to sort
 * MethodEntry pointers into alphabetical order by method name,
 * then by class name.
 */
int32_t compareMethodNames(const void* a, const void* b) {
  const MethodEntry* methodA = *(const MethodEntry**) a;
  const MethodEntry* methodB = *(const MethodEntry**) b;
  if (methodA->methodName == nullptr || methodB->methodName == nullptr) {
    return compareClassNames(a, b);
  }
  int32_t result = strcmp(methodA->methodName, methodB->methodName);
  if (result == 0) {
    result = strcmp(methodA->className, methodB->className);
    if (result == 0) {
      int64_t idA = methodA->methodId;
      int64_t idB = methodB->methodId;
      if (idA < idB) return -1;
      if (idA > idB) return 1;
      return 0;
    }
  }
  return result;
}

/*
 * This comparison function is called from qsort() to sort
 * unique methods into decreasing order of exclusive elapsed time.
 */
int32_t compareUniqueExclusive(const void* a, const void* b) {
  const UniqueMethodEntry* uniqueA = *(const UniqueMethodEntry**) a;
  const UniqueMethodEntry* uniqueB = *(const UniqueMethodEntry**) b;
  uint64_t elapsed1 = uniqueA->elapsedExclusive;
  uint64_t elapsed2 = uniqueB->elapsedExclusive;
  if (elapsed1 < elapsed2) return 1;
  if (elapsed1 > elapsed2) return -1;

  /* If the elapsed times of two methods are equal, then sort them
   * into alphabetical order.
   */
  int32_t result = strcmp(uniqueA->methods[0]->className, uniqueB->methods[0]->className);
  if (result == 0) {
    int64_t idA = uniqueA->methods[0]->methodId;
    int64_t idB = uniqueB->methods[0]->methodId;
    if (idA < idB) return -1;
    if (idA > idB) return 1;
    return 0;
  }
  return result;
}

/*
 * Free a DataKeys struct.
 */
void freeDataKeys(DataKeys* pKeys) {
  if (pKeys == nullptr) return;

  delete[] pKeys->fileData;
  delete[] pKeys->threads;
  delete[] pKeys->methods;
  delete pKeys;
}

/*
 * Find the offset to the next occurrence of the specified character.
 *
 * "data" should point somewhere within the current line.  "len" is the
 * number of bytes left in the buffer.
 *
 * Returns -1 if we hit the end of the buffer.
 */
int32_t findNextChar(const char* data, int32_t len, char lookFor) {
  const char* start = data;

  while (len > 0) {
    if (*data == lookFor) return data - start;

    data++;
    len--;
  }

  return -1;
}

/*
 * Count the number of lines until the next token.
 *
 * Returns -1 if none found before EOF.
 */
int32_t countLinesToToken(const char* data, int32_t len) {
  int32_t count = 0;
  int32_t next;

  while (*data != TOKEN_CHAR) {
    next = findNextChar(data, len, '\n');
    if (next < 0) return -1;
    count++;
    data += next + 1;
    len -= next + 1;
  }

  return count;
}

/*
 * Make sure we're at the start of the right section.
 *
 * Returns the length of the token line, or -1 if something is wrong.
 */
int32_t checkToken(const char* data, int32_t len, const char* cmpStr) {
  int32_t cmpLen = strlen(cmpStr);
  int32_t next;

  if (*data != TOKEN_CHAR) {
    fprintf(stderr, "ERROR: not at start of %s (found '%.10s')\n", cmpStr, data);
    return -1;
  }

  next = findNextChar(data, len, '\n');
  if (next < cmpLen + 1) return -1;

  if (strncmp(data + 1, cmpStr, cmpLen) != 0) {
    fprintf(stderr, "ERROR: '%s' not found (got '%.7s')\n", cmpStr, data + 1);
    return -1;
  }

  return next + 1;
}

/*
 * Parse the "*version" section.
 */
int64_t parseVersion(DataKeys* pKeys, int64_t offset, int32_t verbose) {
  if (offset < 0) return -1;

  char* data = pKeys->fileData + offset;
  char* dataEnd = pKeys->fileData + pKeys->fileLen;
  int32_t next = checkToken(data, dataEnd - data, "version");
  if (next <= 0) return -1;

  data += next;

  /*
   * Count the number of items in the "version" section.
   */
  int32_t count = countLinesToToken(data, dataEnd - data);
  if (count <= 0) {
    fprintf(stderr, "ERROR: failed while reading version (found %d)\n", count);
    return -1;
  }

  /* find the end of the line */
  next = findNextChar(data, dataEnd - data, '\n');
  if (next < 0) return -1;

  data[next] = '\0';
  versionNumber = strtoul(data, nullptr, 0);
  if (verbose) printf("VERSION: %d\n", versionNumber);

  data += next + 1;

  /* skip over the rest of the stuff, which is "name=value" lines */
  for (int32_t i = 1; i < count; i++) {
    next = findNextChar(data, dataEnd - data, '\n');
    if (next < 0) return -1;
    // data[next] = '\0';
    // printf("IGNORING: '%s'\n", data);
    data += next + 1;
  }

  return data - pKeys->fileData;
}

/*
 * Parse the "*threads" section.
 */
int64_t parseThreads(DataKeys* pKeys, int64_t offset) {
  if (offset < 0) return -1;

  char* data = pKeys->fileData + offset;
  char* dataEnd = pKeys->fileData + pKeys->fileLen;
  int32_t next = checkToken(data, dataEnd - data, "threads");

  data += next;

  /*
   * Count the number of thread entries (one per line).
   */
  int32_t count = countLinesToToken(data, dataEnd - data);
  if (count <= 0) {
    fprintf(stderr, "ERROR: failed while reading threads (found %d)\n", count);
    return -1;
  }

  // printf("+++ found %d threads\n", count);
  pKeys->threads = new ThreadEntry[count];
  if (pKeys->threads == nullptr) return -1;

  /*
   * Extract all entries.
   */
  for (int32_t i = 0; i < count; i++) {
    next = findNextChar(data, dataEnd - data, '\n');
    assert(next > 0);
    data[next] = '\0';

    int32_t tab = findNextChar(data, next, '\t');
    data[tab] = '\0';

    pKeys->threads[i].threadId = atoi(data);
    pKeys->threads[i].threadName = data + tab + 1;

    data += next + 1;
  }

  pKeys->numThreads = count;
  return data - pKeys->fileData;
}

/*
 * Parse the "*methods" section.
 */
int64_t parseMethods(DataKeys* pKeys, int64_t offset) {
  if (offset < 0) return -1;

  char* data = pKeys->fileData + offset;
  char* dataEnd = pKeys->fileData + pKeys->fileLen;
  int32_t next = checkToken(data, dataEnd - data, "methods");
  if (next < 0) return -1;

  data += next;

  /*
   * Count the number of method entries (one per line).
   */
  int32_t count = countLinesToToken(data, dataEnd - data);
  if (count <= 0) {
    fprintf(stderr, "ERROR: failed while reading methods (found %d)\n", count);
    return -1;
  }

  /* Reserve an extra method at location 0 for the "toplevel" method,
   * and another extra method for all other "unknown" methods.
   */
  count += 2;
  pKeys->methods = new MethodEntry[count];
  if (pKeys->methods == nullptr) return -1;
  initMethodEntry(&pKeys->methods[TOPLEVEL_INDEX], -2, "(toplevel)", nullptr, nullptr,
                  nullptr, nullptr);
  initMethodEntry(&pKeys->methods[UNKNOWN_INDEX], -1, "(unknown)", nullptr, nullptr,
                  nullptr, nullptr);

  /*
   * Extract all entries, starting with index 2.
   */
  for (int32_t i = UNKNOWN_INDEX + 1; i < count; i++) {
    next = findNextChar(data, dataEnd - data, '\n');
    assert(next > 0);
    data[next] = '\0';

    int32_t tab1 = findNextChar(data, next, '\t');
    int32_t tab2 = findNextChar(data + (tab1 + 1), next - (tab1 + 1), '\t');
    int32_t tab3 = findNextChar(data + (tab1 + tab2 + 2), next - (tab1 + tab2 + 2), '\t');
    int32_t tab4 = findNextChar(data + (tab1 + tab2 + tab3 + 3),
                                next - (tab1 + tab2 + tab3 + 3), '\t');
    int32_t tab5 = findNextChar(data + (tab1 + tab2 + tab3 + tab4 + 4),
                                next - (tab1 + tab2 + tab3 + tab4 + 4), '\t');
    if (tab1 < 0) {
      fprintf(stderr, "ERROR: missing field on method line: '%s'\n", data);
      return -1;
    }
    assert(data[tab1] == '\t');
    data[tab1] = '\0';

    char* endptr;
    int64_t id = strtoul(data, &endptr, 0);
    if (*endptr != '\0') {
      fprintf(stderr, "ERROR: bad method ID '%s'\n", data);
      return -1;
    }

    // Allow files that specify just a function name, instead of requiring
    // "class \t method \t signature"
    if (tab2 > 0 && tab3 > 0) {
      tab2 += tab1 + 1;
      tab3 += tab2 + 1;
      assert(data[tab2] == '\t');
      assert(data[tab3] == '\t');
      data[tab2] = data[tab3] = '\0';

      // This is starting to get awkward.  Allow filename and line #.
      if (tab4 > 0 && tab5 > 0) {
        tab4 += tab3 + 1;
        tab5 += tab4 + 1;

        assert(data[tab4] == '\t');
        assert(data[tab5] == '\t');
        data[tab4] = data[tab5] = '\0';

        initMethodEntry(&pKeys->methods[i], id, data + tab1 + 1,
                        data + tab2 + 1, data + tab3 + 1, data + tab4 + 1,
                        data + tab5 + 1);
      } else {
        initMethodEntry(&pKeys->methods[i], id, data + tab1 + 1,
                        data + tab2 + 1, data + tab3 + 1, nullptr, nullptr);
      }
    } else {
      initMethodEntry(&pKeys->methods[i], id, data + tab1 + 1, nullptr, nullptr, nullptr,
                      nullptr);
    }

    data += next + 1;
  }

  pKeys->numMethods = count;
  return data - pKeys->fileData;
}

/*
 * Parse the "*end" section.
 */
int64_t parseEnd(DataKeys* pKeys, int64_t offset) {
  if (offset < 0) return -1;

  char* data = pKeys->fileData + offset;
  char* dataEnd = pKeys->fileData + pKeys->fileLen;
  int32_t next = checkToken(data, dataEnd - data, "end");
  if (next < 0) return -1;

  data += next;

  return data - pKeys->fileData;
}

/*
 * Sort the thread list entries.
 */
static int32_t compareThreads(const void* thread1, const void* thread2) {
  return ((const ThreadEntry*) thread1)->threadId -
         ((const ThreadEntry*) thread2)->threadId;
}

void sortThreadList(DataKeys* pKeys) {
  qsort(pKeys->threads, pKeys->numThreads, sizeof(pKeys->threads[0]), compareThreads);
}

/*
 * Sort the method list entries.
 */
static int32_t compareMethods(const void* meth1, const void* meth2) {
  int64_t id1 = ((const MethodEntry*) meth1)->methodId;
  int64_t id2 = ((const MethodEntry*) meth2)->methodId;
  if (id1 < id2) return -1;
  if (id1 > id2) return 1;
  return 0;
}

void sortMethodList(DataKeys* pKeys) {
  qsort(pKeys->methods, pKeys->numMethods, sizeof(MethodEntry), compareMethods);
}

/*
 * Parse the key section, and return a copy of the parsed contents.
 */
DataKeys* parseKeys(FILE* fp, int32_t verbose) {
  int64_t offset;
  DataKeys* pKeys = new DataKeys();
  if (pKeys == nullptr) return nullptr;
  memset(pKeys, 0, sizeof(DataKeys));

  /*
   * We load the entire file into memory.  We do this, rather than memory-
   * mapping it, because we want to change some whitespace to NULs.
   */
  if (fseek(fp, 0L, SEEK_END) != 0) {
    perror("fseek");
    freeDataKeys(pKeys);
    return nullptr;
  }
  pKeys->fileLen = ftell(fp);
  if (pKeys->fileLen == 0) {
    fprintf(stderr, "Key file is empty.\n");
    freeDataKeys(pKeys);
    return nullptr;
  }
  rewind(fp);

  pKeys->fileData = new char[pKeys->fileLen];
  if (pKeys->fileData == nullptr) {
    fprintf(stderr, "ERROR: unable to alloc %" PRIu64 " bytes\n", pKeys->fileLen);
    freeDataKeys(pKeys);
    return nullptr;
  }

  if (fread(pKeys->fileData, 1, pKeys->fileLen, fp) != (size_t)pKeys->fileLen) {
    fprintf(stderr, "ERROR: unable to read %" PRIu64 " bytes from trace file\n", pKeys->fileLen);
    freeDataKeys(pKeys);
    return nullptr;
  }

  offset = 0;
  offset = parseVersion(pKeys, offset, verbose);
  offset = parseThreads(pKeys, offset);
  offset = parseMethods(pKeys, offset);
  offset = parseEnd(pKeys, offset);
  if (offset < 0) {
    freeDataKeys(pKeys);
    return nullptr;
  }

  /*
   * Although it is tempting to reduce our allocation now that we know where the
   * end of the key section is, there is a pitfall. The method names and
   * signatures in the method list contain pointers into the fileData area.
   * Realloc or free will result in corruption.
   */

  /* Leave fp pointing to the beginning of the data section. */
  fseek(fp, offset, SEEK_SET);

  sortThreadList(pKeys);
  sortMethodList(pKeys);

  /*
   * Dump list of threads.
   */
  if (verbose) {
    printf("Threads (%d):\n", pKeys->numThreads);
    for (int32_t i = 0; i < pKeys->numThreads; i++) {
      printf("%2d %s\n", pKeys->threads[i].threadId, pKeys->threads[i].threadName);
    }
  }

#if 0
  /*
   * Dump list of methods.
   */
  if (verbose) {
    printf("Methods (%d):\n", pKeys->numMethods);
    for (int32_t i = 0; i < pKeys->numMethods; i++) {
      printf("0x%08x %s : %s : %s\n",
             pKeys->methods[i].methodId, pKeys->methods[i].className,
             pKeys->methods[i].methodName, pKeys->methods[i].signature);
    }
  }
#endif

  return pKeys;
}

/*
 * Read values from the binary data file.
 */

/*
 * Make the return value "uint32_t" instead of "uint16_t" so that we can detect EOF.
 */
uint32_t read2LE(FILE* fp) {
  uint32_t val = getc(fp);
  val |= getc(fp) << 8;
  return val;
}
uint32_t read4LE(FILE* fp) {
  uint32_t val = getc(fp);
  val |= getc(fp) << 8;
  val |= getc(fp) << 16;
  val |= getc(fp) << 24;
  return val;
}
uint64_t read8LE(FILE* fp) {
  uint64_t val = getc(fp);
  val |= (uint64_t) getc(fp) << 8;
  val |= (uint64_t) getc(fp) << 16;
  val |= (uint64_t) getc(fp) << 24;
  val |= (uint64_t) getc(fp) << 32;
  val |= (uint64_t) getc(fp) << 40;
  val |= (uint64_t) getc(fp) << 48;
  val |= (uint64_t) getc(fp) << 56;
  return val;
}

/*
 * Parse the header of the data section.
 *
 * Returns with the file positioned at the start of the record data.
 */
int32_t parseDataHeader(FILE* fp, DataHeader* pHeader) {
  pHeader->magic = read4LE(fp);
  pHeader->version = read2LE(fp);
  pHeader->offsetToData = read2LE(fp);
  pHeader->startWhen = read8LE(fp);
  int32_t bytesToRead = pHeader->offsetToData - 16;
  if (pHeader->version == 1) {
    pHeader->recordSize = 9;
  } else if (pHeader->version == 2) {
    pHeader->recordSize = 10;
  } else if (pHeader->version == 3) {
    pHeader->recordSize = read2LE(fp);
    bytesToRead -= 2;
  } else {
    fprintf(stderr, "Unsupported trace file version: %d\n", pHeader->version);
    return -1;
  }

  if (fseek(fp, bytesToRead, SEEK_CUR) != 0) {
    return -1;
  }

  return 0;
}

/*
 * Look up a method by it's method ID.
 *
 * Returns nullptr if no matching method was found.
 */
MethodEntry* lookupMethod(DataKeys* pKeys, int64_t methodId) {
  int32_t lo = 0;
  int32_t hi = pKeys->numMethods - 1;

  while (hi >= lo) {
    int32_t mid = (hi + lo) / 2;

    int64_t id = pKeys->methods[mid].methodId;
    if (id == methodId) /* match */
      return &pKeys->methods[mid];
    else if (id < methodId) /* too low */
      lo = mid + 1;
    else /* too high */
      hi = mid - 1;
  }

  return nullptr;
}

/*
 * Reads the next data record, and assigns the data values to threadId,
 * methodVal and elapsedTime.  On end-of-file, the threadId, methodVal,
 * and elapsedTime are unchanged.  Returns 1 on end-of-file, otherwise
 * returns 0.
 */
int32_t readDataRecord(FILE* dataFp, DataHeader* dataHeader, int32_t* threadId,
                   uint32_t* methodVal, uint64_t* elapsedTime) {
  int32_t id;
  int32_t bytesToRead = dataHeader->recordSize;
  if (dataHeader->version == 1) {
    id = getc(dataFp);
    bytesToRead -= 1;
  } else {
    id = read2LE(dataFp);
    bytesToRead -= 2;
  }
  if (id == EOF) return 1;
  *threadId = id;

  *methodVal = read4LE(dataFp);
  *elapsedTime = read4LE(dataFp);
  bytesToRead -= 8;

  while (bytesToRead-- > 0) {
    getc(dataFp);
  }

  if (feof(dataFp)) {
    fprintf(stderr, "WARNING: hit EOF mid-record\n");
    return 1;
  }
  return 0;
}

/*
 * Read the key file and use it to produce formatted output from the
 * data file.
 */
void dumpTrace() {
  static const char* actionStr[] = {"ent", "xit", "unr", "???"};
  MethodEntry bogusMethod = {
      0, "???", "???",        "???",        "???",  -1, 0, 0,
      0, 0,     {nullptr, nullptr}, {nullptr, nullptr}, {0, 0}, 0,  0, -1};
  char bogusBuf[80];
  TraceData traceData;

  // printf("Dumping '%s' '%s'\n", dataFileName, keyFileName);

  char spaces[MAX_STACK_DEPTH + 1];
  memset(spaces, '.', MAX_STACK_DEPTH);
  spaces[MAX_STACK_DEPTH] = '\0';

  for (int32_t i = 0; i < MAX_THREADS; i++)
    traceData.depth[i] = 2;  // adjust for return from start function

  FILE* dataFp = fopen(gOptions.traceFileName, "rb");
  if (dataFp == nullptr) return;

  DataKeys* pKeys = parseKeys(dataFp, 1);
  if (pKeys == nullptr) {
    fclose(dataFp);
    return;
  }

  DataHeader dataHeader;
  if (parseDataHeader(dataFp, &dataHeader) < 0) {
    fclose(dataFp);
    freeDataKeys(pKeys);
    return;
  }

  printf("Trace (threadID action usecs class.method signature):\n");

  while (1) {
    /*
     * Extract values from file.
     */
    int32_t threadId;
    uint32_t methodVal;
    uint64_t elapsedTime;
    if (readDataRecord(dataFp, &dataHeader, &threadId, &methodVal, &elapsedTime))
      break;

    int32_t action = METHOD_ACTION(methodVal);
    int64_t methodId = METHOD_ID(methodVal);

    /*
     * Generate a line of output.
     */
    int64_t lastEnter = 0;
    int32_t mismatch = 0;
    if (action == METHOD_TRACE_ENTER) {
      traceData.depth[threadId]++;
      lastEnter = methodId;
    } else {
      /* quick test for mismatched adjacent enter/exit */
      if (lastEnter != 0 && lastEnter != methodId) mismatch = 1;
    }

    int32_t printDepth = traceData.depth[threadId];
    char depthNote = ' ';
    if (printDepth < 0) {
      printDepth = 0;
      depthNote = '-';
    } else if (printDepth > MAX_STACK_DEPTH) {
      printDepth = MAX_STACK_DEPTH;
      depthNote = '+';
    }

    MethodEntry* method = lookupMethod(pKeys, methodId);
    if (method == nullptr) {
      method = &bogusMethod;
      sprintf(bogusBuf, "methodId: %#" PRIx64 "", methodId);
      method->signature = bogusBuf;
    }

    if (method->methodName) {
      printf("%2d %s%c %8" PRIu64 "%c%s%s.%s %s\n", threadId, actionStr[action],
             mismatch ? '!' : ' ', elapsedTime, depthNote,
             spaces + (MAX_STACK_DEPTH - printDepth), method->className,
             method->methodName, method->signature);
    } else {
      printf("%2d %s%c %8" PRIu64 "%c%s%s\n", threadId, actionStr[action],
             mismatch ? '!' : ' ', elapsedTime, depthNote,
             spaces + (MAX_STACK_DEPTH - printDepth), method->className);
    }

    if (action != METHOD_TRACE_ENTER) {
      traceData.depth[threadId]--; /* METHOD_TRACE_EXIT or METHOD_TRACE_UNROLL */
      lastEnter = 0;
    }

    mismatch = 0;
  }

  fclose(dataFp);
  freeDataKeys(pKeys);
}

/* This routine adds the given time to the parent and child methods.
 * This is called when the child routine exits, after the child has
 * been popped from the stack.  The elapsedTime parameter is the
 * duration of the child routine, including time spent in called routines.
 */
void addInclusiveTime(MethodEntry* parent, MethodEntry* child, uint64_t elapsedTime) {
#if 0
  bool verbose = false;
  if (strcmp(child->className, debugClassName) == 0)
    verbose = true;
#endif

  int32_t childIsRecursive = (child->recursiveEntries > 0);
  int32_t parentIsRecursive = (parent->recursiveEntries > 1);

  if (child->recursiveEntries == 0) {
    child->elapsedInclusive += elapsedTime;
  } else if (child->recursiveEntries == 1) {
    child->recursiveInclusive += elapsedTime;
  }
  child->numCalls[childIsRecursive] += 1;

#if 0
  if (verbose) {
    fprintf(stderr,
            "%s %d elapsedTime: %lld eI: %lld, rI: %lld\n",
            child->className, child->recursiveEntries,
            elapsedTime, child->elapsedInclusive,
            child->recursiveInclusive);
  }
#endif

  /* Find the child method in the parent */
  TimedMethod* pTimed;
  TimedMethod* children = parent->children[parentIsRecursive];
  for (pTimed = children; pTimed; pTimed = pTimed->next) {
    if (pTimed->method == child) {
      pTimed->elapsedInclusive += elapsedTime;
      pTimed->numCalls += 1;
      break;
    }
  }
  if (pTimed == nullptr) {
    /* Allocate a new TimedMethod */
    pTimed = new TimedMethod();
    pTimed->elapsedInclusive = elapsedTime;
    pTimed->numCalls = 1;
    pTimed->method = child;

    /* Add it to the front of the list */
    pTimed->next = children;
    parent->children[parentIsRecursive] = pTimed;
  }

  /* Find the parent method in the child */
  TimedMethod* parents = child->parents[childIsRecursive];
  for (pTimed = parents; pTimed; pTimed = pTimed->next) {
    if (pTimed->method == parent) {
      pTimed->elapsedInclusive += elapsedTime;
      pTimed->numCalls += 1;
      break;
    }
  }
  if (pTimed == nullptr) {
    /* Allocate a new TimedMethod */
    pTimed = new TimedMethod();
    pTimed->elapsedInclusive = elapsedTime;
    pTimed->numCalls = 1;
    pTimed->method = parent;

    /* Add it to the front of the list */
    pTimed->next = parents;
    child->parents[childIsRecursive] = pTimed;
  }

#if 0
  if (verbose) {
    fprintf(stderr,
            "  %s %d eI: %lld\n",
            parent->className, parent->recursiveEntries,
            pTimed->elapsedInclusive);
  }
#endif
}

/* Sorts a linked list and returns a newly allocated array containing
 * the sorted entries.
 */
TimedMethod* sortTimedMethodList(TimedMethod* list, int32_t* num) {
  /* Count the elements */
  TimedMethod* pTimed;
  int32_t num_entries = 0;
  for (pTimed = list; pTimed; pTimed = pTimed->next) num_entries += 1;
  *num = num_entries;
  if (num_entries == 0) return nullptr;

  /* Copy all the list elements to a new array and sort them */
  int32_t ii;
  TimedMethod* sorted = new TimedMethod[num_entries];
  for (ii = 0, pTimed = list; pTimed; pTimed = pTimed->next, ++ii)
    memcpy(&sorted[ii], pTimed, sizeof(TimedMethod));
  qsort(sorted, num_entries, sizeof(TimedMethod), compareTimedMethod);

  /* Fix up the "next" pointers so that they work. */
  for (ii = 0; ii < num_entries - 1; ++ii) sorted[ii].next = &sorted[ii + 1];
  sorted[num_entries - 1].next = nullptr;

  return sorted;
}

/* Define flag values for printInclusiveMethod() */
static const int32_t kIsRecursive = 1;

/* This prints the inclusive stats for all the parents or children of a
 * method, depending on the list that is passed in.
 */
void printInclusiveMethod(MethodEntry* method, TimedMethod* list, int32_t numCalls, int32_t flags) {
  char buf[80];
  const char* anchor_close = "";
  const char* spaces = "      "; /* 6 spaces */
  int32_t num_spaces = strlen(spaces);
  const char* space_ptr = &spaces[num_spaces];
  char classBuf[HTML_BUFSIZE], methodBuf[HTML_BUFSIZE];
  char signatureBuf[HTML_BUFSIZE];

  if (gOptions.outputHtml) anchor_close = "</a>";

  int32_t num;
  TimedMethod* sorted = sortTimedMethodList(list, &num);
  double methodTotal = method->elapsedInclusive;
  for (TimedMethod* pTimed = sorted; pTimed; pTimed = pTimed->next) {
    MethodEntry* relative = pTimed->method;
    const char* className = relative->className;
    const char* methodName = relative->methodName;
    const char* signature = relative->signature;
    double per = 100.0 * pTimed->elapsedInclusive / methodTotal;
    sprintf(buf, "[%d]", relative->index);
    if (gOptions.outputHtml) {
      int32_t len = strlen(buf);
      if (len > num_spaces) len = num_spaces;
      sprintf(buf, "<a href=\"#m%d\">[%d]", relative->index, relative->index);
      space_ptr = &spaces[len];
      className = htmlEscape(className, classBuf, HTML_BUFSIZE);
      methodName = htmlEscape(methodName, methodBuf, HTML_BUFSIZE);
      signature = htmlEscape(signature, signatureBuf, HTML_BUFSIZE);
    }
    int32_t nCalls = numCalls;
    if (nCalls == 0) nCalls = relative->numCalls[0] + relative->numCalls[1];
    if (relative->methodName) {
      if (flags & kIsRecursive) {
        // Don't display percentages for recursive functions
        printf("%6s %5s   %6s %s%6s%s %6d/%-6d %9" PRIu64 " %s.%s %s\n", "", "",
               "", space_ptr, buf, anchor_close, pTimed->numCalls, nCalls,
               pTimed->elapsedInclusive, className, methodName, signature);
      } else {
        printf("%6s %5s   %5.1f%% %s%6s%s %6d/%-6d %9" PRIu64 " %s.%s %s\n", "",
               "", per, space_ptr, buf, anchor_close, pTimed->numCalls, nCalls,
               pTimed->elapsedInclusive, className, methodName, signature);
      }
    } else {
      if (flags & kIsRecursive) {
        // Don't display percentages for recursive functions
        printf("%6s %5s   %6s %s%6s%s %6d/%-6d %9" PRIu64 " %s\n", "", "", "",
               space_ptr, buf, anchor_close, pTimed->numCalls, nCalls,
               pTimed->elapsedInclusive, className);
      } else {
        printf("%6s %5s   %5.1f%% %s%6s%s %6d/%-6d %9" PRIu64 " %s\n", "", "",
               per, space_ptr, buf, anchor_close, pTimed->numCalls, nCalls,
               pTimed->elapsedInclusive, className);
      }
    }
  }
}

void countRecursiveEntries(CallStack* pStack, int32_t top, MethodEntry* method) {
  method->recursiveEntries = 0;
  for (int32_t ii = 0; ii < top; ++ii) {
    if (pStack->calls[ii].method == method) method->recursiveEntries += 1;
  }
}

void stackDump(CallStack* pStack, int32_t top) {
  for (int32_t ii = 0; ii < top; ++ii) {
    MethodEntry* method = pStack->calls[ii].method;
    uint64_t entryTime = pStack->calls[ii].entryTime;
    if (method->methodName) {
      fprintf(stderr, "  %2d: %8" PRIu64 " %s.%s %s\n", ii, entryTime,
              method->className, method->methodName, method->signature);
    } else {
      fprintf(stderr, "  %2d: %8" PRIu64 " %s\n", ii, entryTime, method->className);
    }
  }
}

void outputTableOfContents() {
  printf("<a name=\"contents\"></a>\n");
  printf("<h2>Table of Contents</h2>\n");
  printf("<ul>\n");
  printf("  <li><a href=\"#exclusive\">Exclusive profile</a></li>\n");
  printf("  <li><a href=\"#inclusive\">Inclusive profile</a></li>\n");
  printf("  <li><a href=\"#class\">Class/method profile</a></li>\n");
  printf("  <li><a href=\"#method\">Method/class profile</a></li>\n");
  printf("</ul>\n\n");
}

void outputNavigationBar() {
  printf("<a href=\"#contents\">[Top]</a>\n");
  printf("<a href=\"#exclusive\">[Exclusive]</a>\n");
  printf("<a href=\"#inclusive\">[Inclusive]</a>\n");
  printf("<a href=\"#class\">[Class]</a>\n");
  printf("<a href=\"#method\">[Method]</a>\n");
  printf("<br><br>\n");
}

void printExclusiveProfile(MethodEntry** pMethods, int32_t numMethods, uint64_t sumThreadTime) {
  char classBuf[HTML_BUFSIZE], methodBuf[HTML_BUFSIZE];
  char signatureBuf[HTML_BUFSIZE];
  const char* anchor_close = "";
  char anchor_buf[80];
  anchor_buf[0] = 0;
  if (gOptions.outputHtml) {
    anchor_close = "</a>";
    printf("<a name=\"exclusive\"></a>\n");
    printf("<hr>\n");
    outputNavigationBar();
  } else {
    printf("\n%s\n", profileSeparator);
  }

  /* First, sort the methods into decreasing order of inclusive
   * elapsed time so that we can assign the method indices.
   */
  qsort(pMethods, numMethods, sizeof(MethodEntry*), compareElapsedInclusive);

  for (int32_t ii = 0; ii < numMethods; ++ii) pMethods[ii]->index = ii;

  /* Sort the methods into decreasing order of exclusive elapsed time. */
  qsort(pMethods, numMethods, sizeof(MethodEntry*), compareElapsedExclusive);

  printf("Total cycles: %" PRIu64 "\n\n", sumThreadTime);
  if (gOptions.outputHtml) {
    printf("<br><br>\n");
  }
  printf("Exclusive elapsed times for each method, not including time spent in\n");
  printf("children, sorted by exclusive time.\n\n");
  if (gOptions.outputHtml) {
    printf("<br><br>\n<pre>\n");
  }

  printf("    Usecs  self %%  sum %%  Method\n");

  double sum = 0;
  double total = sumThreadTime;
  for (int32_t ii = 0; ii < numMethods; ++ii) {
    MethodEntry* method = pMethods[ii];
    /* Don't show methods with zero cycles */
    if (method->elapsedExclusive == 0) break;
    const char* className = method->className;
    const char* methodName = method->methodName;
    const char* signature = method->signature;
    sum += method->elapsedExclusive;
    double per = 100.0 * method->elapsedExclusive / total;
    double sum_per = 100.0 * sum / total;
    if (gOptions.outputHtml) {
      sprintf(anchor_buf, "<a href=\"#m%d\">", method->index);
      className = htmlEscape(className, classBuf, HTML_BUFSIZE);
      methodName = htmlEscape(methodName, methodBuf, HTML_BUFSIZE);
      signature = htmlEscape(signature, signatureBuf, HTML_BUFSIZE);
    }
    if (method->methodName) {
      printf("%9" PRIu64 "  %6.2f %6.2f  %s[%d]%s %s.%s %s\n",
             method->elapsedExclusive, per, sum_per, anchor_buf, method->index,
             anchor_close, className, methodName, signature);
    } else {
      printf("%9" PRIu64 "  %6.2f %6.2f  %s[%d]%s %s\n",
             method->elapsedExclusive, per, sum_per, anchor_buf, method->index,
             anchor_close, className);
    }
  }
  if (gOptions.outputHtml) {
    printf("</pre>\n");
  }
}

/* check to make sure that the child method meets the threshold of the parent */
int32_t checkThreshold(MethodEntry* parent, MethodEntry* child) {
  double parentTime = parent->elapsedInclusive;
  double childTime = child->elapsedInclusive;
  int64_t percentage = (childTime / parentTime) * 100.0;
  return (percentage < gOptions.threshold) ? 0 : 1;
}

void createLabels(FILE* file, MethodEntry* method) {
  fprintf(file,
          "node%d[label = \"[%d] %s.%s (%" PRIu64 ", %" PRIu64 ", %d)\"]\n",
          method->index, method->index, method->className, method->methodName,
          method->elapsedInclusive / 1000, method->elapsedExclusive / 1000,
          method->numCalls[0]);

  method->graphState = GRAPH_LABEL_VISITED;

  for (TimedMethod* child = method->children[0]; child; child = child->next) {
    MethodEntry* childMethod = child->method;

    if ((childMethod->graphState & GRAPH_LABEL_VISITED) == 0 &&
        checkThreshold(method, childMethod)) {
      createLabels(file, child->method);
    }
  }
}

void createLinks(FILE* file, MethodEntry* method) {
  method->graphState |= GRAPH_NODE_VISITED;

  for (TimedMethod* child = method->children[0]; child; child = child->next) {
    MethodEntry* childMethod = child->method;
    if (checkThreshold(method, child->method)) {
      fprintf(file, "node%d -> node%d\n", method->index, child->method->index);
      // only visit children that haven't been visited before
      if ((childMethod->graphState & GRAPH_NODE_VISITED) == 0) {
        createLinks(file, child->method);
      }
    }
  }
}

void createInclusiveProfileGraphNew(DataKeys* dataKeys) {
  // create a temporary file in /tmp
  char path[FILENAME_MAX];
  if (gOptions.keepDotFile) {
    snprintf(path, FILENAME_MAX, "%s.dot", gOptions.graphFileName);
  } else {
    snprintf(path, FILENAME_MAX, "dot-%d-%d.dot", (int32_t)time(nullptr), rand());
  }

  FILE* file = fopen(path, "w+");

  fprintf(file, "digraph g {\nnode [shape = record,height=.1];\n");

  createLabels(file, dataKeys->methods);
  createLinks(file, dataKeys->methods);

  fprintf(file, "}");
  fclose(file);

  // now that we have the dot file generate the image
  char command[1024];
  snprintf(command, 1024, "dot -Tpng -o \"%s\" \"%s\"", gOptions.graphFileName, path);

  system(command);

  if (!gOptions.keepDotFile) {
    remove(path);
  }
}

void printInclusiveProfile(MethodEntry** pMethods, int32_t numMethods, uint64_t sumThreadTime) {
  char classBuf[HTML_BUFSIZE], methodBuf[HTML_BUFSIZE];
  char signatureBuf[HTML_BUFSIZE];
  char anchor_buf[80];
  const char* anchor_close = "";
  anchor_buf[0] = 0;
  if (gOptions.outputHtml) {
    anchor_close = "</a>";
    printf("<a name=\"inclusive\"></a>\n");
    printf("<hr>\n");
    outputNavigationBar();
  } else {
    printf("\n%s\n", profileSeparator);
  }

  /* Sort the methods into decreasing order of inclusive elapsed time. */
  qsort(pMethods, numMethods, sizeof(MethodEntry*), compareElapsedInclusive);

  printf("\nInclusive elapsed times for each method and its parents and children,\n");
  printf("sorted by inclusive time.\n\n");

  if (gOptions.outputHtml) {
    printf("<br><br>\n<pre>\n");
  }

  printf("index  %%/total %%/self  index     calls         usecs name\n");

  double total = sumThreadTime;
  for (int32_t ii = 0; ii < numMethods; ++ii) {
    char buf[40];

    MethodEntry* method = pMethods[ii];
    /* Don't show methods with zero cycles */
    if (method->elapsedInclusive == 0) break;

    const char* className = method->className;
    const char* methodName = method->methodName;
    const char* signature = method->signature;

    if (gOptions.outputHtml) {
      printf("<a name=\"m%d\"></a>", method->index);
      className = htmlEscape(className, classBuf, HTML_BUFSIZE);
      methodName = htmlEscape(methodName, methodBuf, HTML_BUFSIZE);
      signature = htmlEscape(signature, signatureBuf, HTML_BUFSIZE);
    }
    printf("----------------------------------------------------\n");

    /* Sort and print the parents */
    int32_t numCalls = method->numCalls[0] + method->numCalls[1];
    printInclusiveMethod(method, method->parents[0], numCalls, 0);
    if (method->parents[1]) {
      printf("               +++++++++++++++++++++++++\n");
      printInclusiveMethod(method, method->parents[1], numCalls, kIsRecursive);
    }

    double per = 100.0 * method->elapsedInclusive / total;
    sprintf(buf, "[%d]", ii);
    if (method->methodName) {
      printf("%-6s %5.1f%%   %5s %6s %6d+%-6d %9" PRIu64 " %s.%s %s\n", buf,
             per, "", "", method->numCalls[0], method->numCalls[1],
             method->elapsedInclusive, className, methodName, signature);
    } else {
      printf("%-6s %5.1f%%   %5s %6s %6d+%-6d %9" PRIu64 " %s\n", buf, per, "",
             "", method->numCalls[0], method->numCalls[1],
             method->elapsedInclusive, className);
    }
    double excl_per = 100.0 * method->topExclusive / method->elapsedInclusive;
    printf("%6s %5s   %5.1f%% %6s %6s %6s %9" PRIu64 "\n", "", "", excl_per,
           "excl", "", "", method->topExclusive);

    /* Sort and print the children */
    printInclusiveMethod(method, method->children[0], 0, 0);
    if (method->children[1]) {
      printf("               +++++++++++++++++++++++++\n");
      printInclusiveMethod(method, method->children[1], 0, kIsRecursive);
    }
  }
  if (gOptions.outputHtml) {
    printf("</pre>\n");
  }
}

void createClassList(TraceData* traceData, MethodEntry** pMethods, int32_t numMethods) {
  /* Sort the methods into alphabetical order to find the unique class
   * names.
   */
  qsort(pMethods, numMethods, sizeof(MethodEntry*), compareClassNames);

  /* Count the number of unique class names. */
  const char* currentClassName = "";
  const char* firstClassName = nullptr;
  traceData->numClasses = 0;
  for (int32_t ii = 0; ii < numMethods; ++ii) {
    if (pMethods[ii]->methodName == nullptr) {
      continue;
    }
    if (strcmp(pMethods[ii]->className, currentClassName) != 0) {
      // Remember the first one
      if (firstClassName == nullptr) {
        firstClassName = pMethods[ii]->className;
      }
      traceData->numClasses += 1;
      currentClassName = pMethods[ii]->className;
    }
  }

  if (traceData->numClasses == 0) {
    traceData->classes = nullptr;
    return;
  }

  /* Allocate space for all of the unique class names */
  traceData->classes = new ClassEntry[traceData->numClasses];

  /* Initialize the classes array */
  memset(traceData->classes, 0, sizeof(ClassEntry) * traceData->numClasses);
  ClassEntry* pClass = traceData->classes;
  pClass->className = currentClassName = firstClassName;
  int32_t prevNumMethods = 0;
  for (int32_t ii = 0; ii < numMethods; ++ii) {
    if (pMethods[ii]->methodName == nullptr) {
      continue;
    }
    if (strcmp(pMethods[ii]->className, currentClassName) != 0) {
      pClass->numMethods = prevNumMethods;
      (++pClass)->className = currentClassName = pMethods[ii]->className;
      prevNumMethods = 0;
    }
    prevNumMethods += 1;
  }
  pClass->numMethods = prevNumMethods;

  /* Create the array of MethodEntry pointers for each class */
  pClass = nullptr;
  currentClassName = "";
  int32_t nextMethod = 0;
  for (int32_t ii = 0; ii < numMethods; ++ii) {
    if (pMethods[ii]->methodName == nullptr) {
      continue;
    }
    if (strcmp(pMethods[ii]->className, currentClassName) != 0) {
      currentClassName = pMethods[ii]->className;
      if (pClass == nullptr)
        pClass = traceData->classes;
      else
        pClass++;
      /* Allocate space for the methods array */
      pClass->methods = new MethodEntry*[pClass->numMethods];
      nextMethod = 0;
    }
    pClass->methods[nextMethod++] = pMethods[ii];
  }
}

/* Prints a number of html non-breaking spaces according so that the length
 * of the string "buf" is at least "width" characters wide.  If width is
 * negative, then trailing spaces are added instead of leading spaces.
 */
void printHtmlField(char* buf, int32_t width) {
  int32_t leadingSpaces = 1;
  if (width < 0) {
    width = -width;
    leadingSpaces = 0;
  }
  int32_t len = strlen(buf);
  int32_t numSpaces = width - len;
  if (numSpaces <= 0) {
    printf("%s", buf);
    return;
  }
  if (leadingSpaces == 0) printf("%s", buf);
  for (int32_t ii = 0; ii < numSpaces; ++ii) printf("&nbsp;");
  if (leadingSpaces == 1) printf("%s", buf);
}

void printClassProfiles(TraceData* traceData, uint64_t sumThreadTime) {
  char classBuf[HTML_BUFSIZE];
  char methodBuf[HTML_BUFSIZE];
  char signatureBuf[HTML_BUFSIZE];

  if (gOptions.outputHtml) {
    printf("<a name=\"class\"></a>\n");
    printf("<hr>\n");
    outputNavigationBar();
  } else {
    printf("\n%s\n", profileSeparator);
  }

  if (traceData->numClasses == 0) {
    printf("\nNo classes.\n");
    if (gOptions.outputHtml) {
      printf("<br><br>\n");
    }
    return;
  }

  printf("\nExclusive elapsed time for each class, summed over all the methods\n");
  printf("in the class.\n\n");
  if (gOptions.outputHtml) {
    printf("<br><br>\n");
  }

  /* For each class, sum the exclusive times in all of the methods
   * in that class.  Also sum the number of method calls.  Also
   * sort the methods so the most expensive appear at the top.
   */
  ClassEntry* pClass = traceData->classes;
  for (int32_t ii = 0; ii < traceData->numClasses; ++ii, ++pClass) {
    // printf("%s %d methods\n", pClass->className, pClass->numMethods);
    int32_t numMethods = pClass->numMethods;
    for (int32_t jj = 0; jj < numMethods; ++jj) {
      MethodEntry* method = pClass->methods[jj];
      pClass->elapsedExclusive += method->elapsedExclusive;
      pClass->numCalls[0] += method->numCalls[0];
      pClass->numCalls[1] += method->numCalls[1];
    }

    /* Sort the methods into decreasing order of exclusive time */
    qsort(pClass->methods, numMethods, sizeof(MethodEntry*), compareElapsedExclusive);
  }

  /* Allocate an array of pointers to the classes for more efficient sorting. */
  ClassEntry** pClasses = new ClassEntry*[traceData->numClasses];
  for (int32_t ii = 0; ii < traceData->numClasses; ++ii)
    pClasses[ii] = &traceData->classes[ii];

  /* Sort the classes into decreasing order of exclusive time */
  qsort(pClasses, traceData->numClasses, sizeof(ClassEntry*), compareClassExclusive);

  if (gOptions.outputHtml) {
    printf(
        "<div class=\"header\"><span "
        "class=\"parent\">&nbsp;</span>&nbsp;&nbsp;&nbsp;");
    printf("Cycles %%/total Cumul.%% &nbsp;Calls+Recur&nbsp; Class</div>\n");
  } else {
    printf("   Cycles %%/total Cumul.%%  Calls+Recur  Class\n");
  }

  double sum = 0;
  double total = sumThreadTime;
  for (int32_t ii = 0; ii < traceData->numClasses; ++ii) {
    /* Skip classes with zero cycles */
    pClass = pClasses[ii];
    if (pClass->elapsedExclusive == 0) break;

    sum += pClass->elapsedExclusive;
    double per = 100.0 * pClass->elapsedExclusive / total;
    double sum_per = 100.0 * sum / total;
    const char* className = pClass->className;
    if (gOptions.outputHtml) {
      char buf[80];

      className = htmlEscape(className, classBuf, HTML_BUFSIZE);
      printf(
          "<div class=\"link\" onClick=\"javascript:toggle('d%d')\" "
          "onMouseOver=\"javascript:onMouseOver(this)\" "
          "onMouseOut=\"javascript:onMouseOut(this)\"><span class=\"parent\" "
          "id=\"xd%d\">+</span>",
          ii, ii);
      sprintf(buf, "%" PRIu64, pClass->elapsedExclusive);
      printHtmlField(buf, 9);
      printf(" ");
      sprintf(buf, "%.1f", per);
      printHtmlField(buf, 7);
      printf(" ");
      sprintf(buf, "%.1f", sum_per);
      printHtmlField(buf, 7);
      printf(" ");
      sprintf(buf, "%d", pClass->numCalls[0]);
      printHtmlField(buf, 6);
      printf("+");
      sprintf(buf, "%d", pClass->numCalls[1]);
      printHtmlField(buf, -6);
      printf(" ");
      printf("%s", className);
      printf("</div>\n");
      printf("<div class=\"parent\" id=\"d%d\">\n", ii);
    } else {
      printf("---------------------------------------------\n");
      printf("%9" PRIu64 " %7.1f %7.1f %6d+%-6d %s\n", pClass->elapsedExclusive,
             per, sum_per, pClass->numCalls[0], pClass->numCalls[1], className);
    }

    int32_t numMethods = pClass->numMethods;
    double classExclusive = pClass->elapsedExclusive;
    double sumMethods = 0;
    for (int32_t jj = 0; jj < numMethods; ++jj) {
      MethodEntry* method = pClass->methods[jj];
      const char* methodName = method->methodName;
      const char* signature = method->signature;
      per = 100.0 * method->elapsedExclusive / classExclusive;
      sumMethods += method->elapsedExclusive;
      sum_per = 100.0 * sumMethods / classExclusive;
      if (gOptions.outputHtml) {
        char buf[80];

        methodName = htmlEscape(methodName, methodBuf, HTML_BUFSIZE);
        signature = htmlEscape(signature, signatureBuf, HTML_BUFSIZE);
        printf("<div class=\"leaf\"><span class=\"leaf\">&nbsp;</span>");
        sprintf(buf, "%" PRIu64, method->elapsedExclusive);
        printHtmlField(buf, 9);
        printf("&nbsp;");
        sprintf(buf, "%" PRIu64, method->elapsedInclusive);
        printHtmlField(buf, 9);
        printf("&nbsp;");
        sprintf(buf, "%.1f", per);
        printHtmlField(buf, 7);
        printf("&nbsp;");
        sprintf(buf, "%.1f", sum_per);
        printHtmlField(buf, 7);
        printf("&nbsp;");
        sprintf(buf, "%d", method->numCalls[0]);
        printHtmlField(buf, 6);
        printf("+");
        sprintf(buf, "%d", method->numCalls[1]);
        printHtmlField(buf, -6);
        printf("&nbsp;");
        printf("<a href=\"#m%d\">[%d]</a>&nbsp;%s&nbsp;%s", method->index,
               method->index, methodName, signature);
        printf("</div>\n");
      } else {
        printf("%9" PRIu64 " %9" PRIu64 " %7.1f %7.1f %6d+%-6d [%d] %s %s\n",
               method->elapsedExclusive, method->elapsedInclusive, per, sum_per,
               method->numCalls[0], method->numCalls[1], method->index,
               methodName, signature);
      }
    }
    if (gOptions.outputHtml) {
      printf("</div>\n");
    }
  }
}

void createUniqueMethodList(TraceData* traceData, MethodEntry** pMethods, int32_t numMethods) {
  /* Sort the methods into alphabetical order of method names
   * to find the unique method names.
   */
  qsort(pMethods, numMethods, sizeof(MethodEntry*), compareMethodNames);

  /* Count the number of unique method names, ignoring class and signature. */
  const char* currentMethodName = "";
  traceData->numUniqueMethods = 0;
  for (int32_t ii = 0; ii < numMethods; ++ii) {
    if (pMethods[ii]->methodName == nullptr) continue;
    if (strcmp(pMethods[ii]->methodName, currentMethodName) != 0) {
      traceData->numUniqueMethods += 1;
      currentMethodName = pMethods[ii]->methodName;
    }
  }
  if (traceData->numUniqueMethods == 0) return;

  /* Allocate space for pointers to all of the unique methods */
  traceData->uniqueMethods = new UniqueMethodEntry[traceData->numUniqueMethods];

  /* Initialize the uniqueMethods array */
  memset(traceData->uniqueMethods, 0, sizeof(UniqueMethodEntry) * traceData->numUniqueMethods);
  UniqueMethodEntry* pUnique = traceData->uniqueMethods;
  currentMethodName = nullptr;
  int32_t prevNumMethods = 0;
  for (int32_t ii = 0; ii < numMethods; ++ii) {
    if (pMethods[ii]->methodName == nullptr) continue;
    if (currentMethodName == nullptr) currentMethodName = pMethods[ii]->methodName;
    if (strcmp(pMethods[ii]->methodName, currentMethodName) != 0) {
      currentMethodName = pMethods[ii]->methodName;
      pUnique->numMethods = prevNumMethods;
      pUnique++;
      prevNumMethods = 0;
    }
    prevNumMethods += 1;
  }
  pUnique->numMethods = prevNumMethods;

  /* Create the array of MethodEntry pointers for each unique method */
  pUnique = nullptr;
  currentMethodName = "";
  int32_t nextMethod = 0;
  for (int32_t ii = 0; ii < numMethods; ++ii) {
    if (pMethods[ii]->methodName == nullptr) continue;
    if (strcmp(pMethods[ii]->methodName, currentMethodName) != 0) {
      currentMethodName = pMethods[ii]->methodName;
      if (pUnique == nullptr)
        pUnique = traceData->uniqueMethods;
      else
        pUnique++;
      /* Allocate space for the methods array */
      pUnique->methods = new MethodEntry*[pUnique->numMethods];
      nextMethod = 0;
    }
    pUnique->methods[nextMethod++] = pMethods[ii];
  }
}

void printMethodProfiles(TraceData* traceData, uint64_t sumThreadTime) {
  char classBuf[HTML_BUFSIZE], methodBuf[HTML_BUFSIZE];
  char signatureBuf[HTML_BUFSIZE];

  if (traceData->numUniqueMethods == 0) return;

  if (gOptions.outputHtml) {
    printf("<a name=\"method\"></a>\n");
    printf("<hr>\n");
    outputNavigationBar();
  } else {
    printf("\n%s\n", profileSeparator);
  }

  printf("\nExclusive elapsed time for each method, summed over all the classes\n");
  printf("that contain a method with the same name.\n\n");
  if (gOptions.outputHtml) {
    printf("<br><br>\n");
  }

  /* For each unique method, sum the exclusive times in all of the methods
   * with the same name.  Also sum the number of method calls.  Also
   * sort the methods so the most expensive appear at the top.
   */
  UniqueMethodEntry* pUnique = traceData->uniqueMethods;
  for (int32_t ii = 0; ii < traceData->numUniqueMethods; ++ii, ++pUnique) {
    int32_t numMethods = pUnique->numMethods;
    for (int32_t jj = 0; jj < numMethods; ++jj) {
      MethodEntry* method = pUnique->methods[jj];
      pUnique->elapsedExclusive += method->elapsedExclusive;
      pUnique->numCalls[0] += method->numCalls[0];
      pUnique->numCalls[1] += method->numCalls[1];
    }

    /* Sort the methods into decreasing order of exclusive time */
    qsort(pUnique->methods, numMethods, sizeof(MethodEntry*), compareElapsedExclusive);
  }

  /* Allocate an array of pointers to the methods for more efficient sorting. */
  UniqueMethodEntry** pUniqueMethods = new UniqueMethodEntry*[traceData->numUniqueMethods];
  for (int32_t ii = 0; ii < traceData->numUniqueMethods; ++ii)
    pUniqueMethods[ii] = &traceData->uniqueMethods[ii];

  /* Sort the methods into decreasing order of exclusive time */
  qsort(pUniqueMethods, traceData->numUniqueMethods, sizeof(UniqueMethodEntry*),
        compareUniqueExclusive);

  if (gOptions.outputHtml) {
    printf(
        "<div class=\"header\"><span "
        "class=\"parent\">&nbsp;</span>&nbsp;&nbsp;&nbsp;");
    printf("Cycles %%/total Cumul.%% &nbsp;Calls+Recur&nbsp; Method</div>\n");
  } else {
    printf("   Cycles %%/total Cumul.%%  Calls+Recur  Method\n");
  }

  double sum = 0;
  double total = sumThreadTime;
  for (int32_t ii = 0; ii < traceData->numUniqueMethods; ++ii) {
    /* Skip methods with zero cycles */
    pUnique = pUniqueMethods[ii];
    if (pUnique->elapsedExclusive == 0) break;

    sum += pUnique->elapsedExclusive;
    double per = 100.0 * pUnique->elapsedExclusive / total;
    double sum_per = 100.0 * sum / total;
    const char* methodName = pUnique->methods[0]->methodName;
    if (gOptions.outputHtml) {
      char buf[80];

      methodName = htmlEscape(methodName, methodBuf, HTML_BUFSIZE);
      printf(
          "<div class=\"link\" onClick=\"javascript:toggle('e%d')\" "
          "onMouseOver=\"javascript:onMouseOver(this)\" "
          "onMouseOut=\"javascript:onMouseOut(this)\"><span class=\"parent\" "
          "id=\"xe%d\">+</span>",
          ii, ii);
      sprintf(buf, "%" PRIu64, pUnique->elapsedExclusive);
      printHtmlField(buf, 9);
      printf(" ");
      sprintf(buf, "%.1f", per);
      printHtmlField(buf, 7);
      printf(" ");
      sprintf(buf, "%.1f", sum_per);
      printHtmlField(buf, 7);
      printf(" ");
      sprintf(buf, "%d", pUnique->numCalls[0]);
      printHtmlField(buf, 6);
      printf("+");
      sprintf(buf, "%d", pUnique->numCalls[1]);
      printHtmlField(buf, -6);
      printf(" ");
      printf("%s", methodName);
      printf("</div>\n");
      printf("<div class=\"parent\" id=\"e%d\">\n", ii);
    } else {
      printf("---------------------------------------------\n");
      printf("%9" PRIu64 " %7.1f %7.1f %6d+%-6d %s\n",
             pUnique->elapsedExclusive, per, sum_per, pUnique->numCalls[0],
             pUnique->numCalls[1], methodName);
    }
    int32_t numMethods = pUnique->numMethods;
    double methodExclusive = pUnique->elapsedExclusive;
    double sumMethods = 0;
    for (int32_t jj = 0; jj < numMethods; ++jj) {
      MethodEntry* method = pUnique->methods[jj];
      const char* className = method->className;
      const char* signature = method->signature;
      per = 100.0 * method->elapsedExclusive / methodExclusive;
      sumMethods += method->elapsedExclusive;
      sum_per = 100.0 * sumMethods / methodExclusive;
      if (gOptions.outputHtml) {
        char buf[80];

        className = htmlEscape(className, classBuf, HTML_BUFSIZE);
        signature = htmlEscape(signature, signatureBuf, HTML_BUFSIZE);
        printf("<div class=\"leaf\"><span class=\"leaf\">&nbsp;</span>");
        sprintf(buf, "%" PRIu64, method->elapsedExclusive);
        printHtmlField(buf, 9);
        printf("&nbsp;");
        sprintf(buf, "%" PRIu64, method->elapsedInclusive);
        printHtmlField(buf, 9);
        printf("&nbsp;");
        sprintf(buf, "%.1f", per);
        printHtmlField(buf, 7);
        printf("&nbsp;");
        sprintf(buf, "%.1f", sum_per);
        printHtmlField(buf, 7);
        printf("&nbsp;");
        sprintf(buf, "%d", method->numCalls[0]);
        printHtmlField(buf, 6);
        printf("+");
        sprintf(buf, "%d", method->numCalls[1]);
        printHtmlField(buf, -6);
        printf("&nbsp;");
        printf("<a href=\"#m%d\">[%d]</a>&nbsp;%s.%s&nbsp;%s", method->index,
               method->index, className, methodName, signature);
        printf("</div>\n");
      } else {
        printf("%9" PRIu64 " %9" PRIu64 " %7.1f %7.1f %6d+%-6d [%d] %s.%s %s\n",
               method->elapsedExclusive, method->elapsedInclusive, per, sum_per,
               method->numCalls[0], method->numCalls[1], method->index,
               className, methodName, signature);
      }
    }
    if (gOptions.outputHtml) {
      printf("</div>\n");
    }
  }
}

/*
 * Read the key and data files and return the MethodEntries for those files
 */
DataKeys* parseDataKeys(TraceData* traceData, const char* traceFileName, uint64_t* threadTime) {
  MethodEntry* caller;

  FILE* dataFp = fopen(traceFileName, "rb");
  if (dataFp == nullptr) return nullptr;

  DataKeys* dataKeys = parseKeys(dataFp, 0);
  if (dataKeys == nullptr) {
    fclose(dataFp);
    return nullptr;
  }

  DataHeader dataHeader;
  if (parseDataHeader(dataFp, &dataHeader) < 0) {
    fclose(dataFp);
    return dataKeys;
  }

#if 0
  FILE* dumpStream = fopen("debug", "w");
#endif
  while (1) {
    /*
     * Extract values from file.
     */
    int32_t threadId;
    uint32_t methodVal;
    uint64_t currentTime;
    if (readDataRecord(dataFp, &dataHeader, &threadId, &methodVal, &currentTime))
      break;

    int32_t action = METHOD_ACTION(methodVal);
    int64_t methodId = METHOD_ID(methodVal);

    /* Get the call stack for this thread */
    CallStack* pStack = traceData->stacks[threadId];

    /* If there is no call stack yet for this thread, then allocate one */
    if (pStack == nullptr) {
      pStack = new CallStack();
      pStack->top = 0;
      pStack->lastEventTime = currentTime;
      pStack->threadStartTime = currentTime;
      traceData->stacks[threadId] = pStack;
    }

    /* Lookup the current method */
    MethodEntry* method = lookupMethod(dataKeys, methodId);
    if (method == nullptr) method = &dataKeys->methods[UNKNOWN_INDEX];

#if 0
    if (method->methodName) {
      fprintf(dumpStream, "%2d %-8llu %d %8llu r %d c %d %s.%s %s\n",
              threadId, currentTime, action, pStack->threadStartTime,
              method->recursiveEntries,
              pStack->top, method->className, method->methodName,
              method->signature);
    } else {
      fprintf(dumpStream, "%2d %-8llu %d %8llu r %d c %d %s\n",
              threadId, currentTime, action, pStack->threadStartTime,
              method->recursiveEntries,
              pStack->top, method->className);
    }
#endif

    if (action == METHOD_TRACE_ENTER) {
      /* This is a method entry */
      if (pStack->top >= MAX_STACK_DEPTH) {
        fprintf(stderr, "Stack overflow (exceeded %d frames)\n",
                MAX_STACK_DEPTH);
        exit(1);
      }

      /* Get the caller method */
      if (pStack->top >= 1)
        caller = pStack->calls[pStack->top - 1].method;
      else
        caller = &dataKeys->methods[TOPLEVEL_INDEX];
      countRecursiveEntries(pStack, pStack->top, caller);
      caller->elapsedExclusive += currentTime - pStack->lastEventTime;
#if 0
      if (caller->elapsedExclusive > 10000000)
        fprintf(dumpStream, "%llu current %llu last %llu diff %llu\n",
                caller->elapsedExclusive, currentTime,
                pStack->lastEventTime,
                currentTime - pStack->lastEventTime);
#endif
      if (caller->recursiveEntries <= 1) {
        caller->topExclusive += currentTime - pStack->lastEventTime;
      }

      /* Push the method on the stack for this thread */
      pStack->calls[pStack->top].method = method;
      pStack->calls[pStack->top++].entryTime = currentTime;
    } else {
      /* This is a method exit */
      uint64_t entryTime = 0;

      /* Pop the method off the stack for this thread */
      if (pStack->top > 0) {
        pStack->top -= 1;
        entryTime = pStack->calls[pStack->top].entryTime;
        if (method != pStack->calls[pStack->top].method) {
          if (method->methodName) {
            fprintf(stderr, "Exit from method %s.%s %s does not match stack:\n",
                    method->className, method->methodName, method->signature);
          } else {
            fprintf(stderr, "Exit from method %s does not match stack:\n",
                    method->className);
          }
          stackDump(pStack, pStack->top + 1);
          exit(1);
        }
      }

      /* Get the caller method */
      if (pStack->top >= 1)
        caller = pStack->calls[pStack->top - 1].method;
      else
        caller = &dataKeys->methods[TOPLEVEL_INDEX];
      countRecursiveEntries(pStack, pStack->top, caller);
      countRecursiveEntries(pStack, pStack->top, method);
      uint64_t elapsed = currentTime - entryTime;
      addInclusiveTime(caller, method, elapsed);
      method->elapsedExclusive += currentTime - pStack->lastEventTime;
      if (method->recursiveEntries == 0) {
        method->topExclusive += currentTime - pStack->lastEventTime;
      }
    }
    /* Remember the time of the last entry or exit event */
    pStack->lastEventTime = currentTime;
  }

  /* If we have calls on the stack when the trace ends, then clean
   * up the stack and add time to the callers by pretending that we
   * are exiting from their methods now.
   */
  uint64_t sumThreadTime = 0;
  for (int32_t threadId = 0; threadId < MAX_THREADS; ++threadId) {
    CallStack* pStack = traceData->stacks[threadId];

    /* If this thread never existed, then continue with next thread */
    if (pStack == nullptr) continue;

    /* Also, add up the time taken by all of the threads */
    sumThreadTime += pStack->lastEventTime - pStack->threadStartTime;

    for (int32_t ii = 0; ii < pStack->top; ++ii) {
      if (ii == 0)
        caller = &dataKeys->methods[TOPLEVEL_INDEX];
      else
        caller = pStack->calls[ii - 1].method;
      MethodEntry* method = pStack->calls[ii].method;
      countRecursiveEntries(pStack, ii, caller);
      countRecursiveEntries(pStack, ii, method);

      uint64_t entryTime = pStack->calls[ii].entryTime;
      uint64_t elapsed = pStack->lastEventTime - entryTime;
      addInclusiveTime(caller, method, elapsed);
    }
  }
  caller = &dataKeys->methods[TOPLEVEL_INDEX];
  caller->elapsedInclusive = sumThreadTime;

#if 0
  fclose(dumpStream);
#endif

  if (threadTime != nullptr) {
    *threadTime = sumThreadTime;
  }

  fclose(dataFp);
  return dataKeys;
}

MethodEntry** parseMethodEntries(DataKeys* dataKeys) {
  /* Create a new array of pointers to the methods and sort the pointers
   * instead of the actual MethodEntry structs.  We need to do this
   * because there are other lists that contain pointers to the
   * MethodEntry structs.
   */
  MethodEntry** pMethods = new MethodEntry*[dataKeys->numMethods];
  for (int32_t ii = 0; ii < dataKeys->numMethods; ++ii) {
    MethodEntry* entry = &dataKeys->methods[ii];
    pMethods[ii] = entry;
  }

  return pMethods;
}

/*
 * Produce a function profile from the following methods
 */
void profileTrace(TraceData* traceData, MethodEntry** pMethods, int32_t numMethods,
                  uint64_t sumThreadTime) {
  /* Print the html header, if necessary */
  if (gOptions.outputHtml) {
    printf(htmlHeader, gOptions.sortableUrl);
    outputTableOfContents();
  }

  printExclusiveProfile(pMethods, numMethods, sumThreadTime);
  printInclusiveProfile(pMethods, numMethods, sumThreadTime);

  createClassList(traceData, pMethods, numMethods);
  printClassProfiles(traceData, sumThreadTime);

  createUniqueMethodList(traceData, pMethods, numMethods);
  printMethodProfiles(traceData, sumThreadTime);

  if (gOptions.outputHtml) {
    printf("%s", htmlFooter);
  }
}

int32_t compareMethodNamesForDiff(const void* a, const void* b) {
  const MethodEntry* methodA = *(const MethodEntry**) a;
  const MethodEntry* methodB = *(const MethodEntry**) b;
  if (methodA->methodName == nullptr || methodB->methodName == nullptr) {
    return compareClassNames(a, b);
  }
  int32_t result = strcmp(methodA->methodName, methodB->methodName);
  if (result == 0) {
    result = strcmp(methodA->signature, methodB->signature);
    if (result == 0) {
      return strcmp(methodA->className, methodB->className);
    }
  }
  return result;
}

int32_t findMatch(MethodEntry** methods, int32_t size, MethodEntry* matchThis) {
  for (int32_t i = 0; i < size; i++) {
    MethodEntry* method = methods[i];

    if (method != nullptr && !compareMethodNamesForDiff(&method, &matchThis)) {
      // printf("%s.%s == %s.%s<br>\n", matchThis->className, matchThis->methodName,
      //        method->className, method->methodName);

      return i;
      // if (!compareMethodNames(&method, &matchThis)) return i;
    }
  }

  return -1;
}

int32_t compareDiffEntriesExculsive(const void* a, const void* b) {
  const DiffEntry* entryA = (const DiffEntry*) a;
  const DiffEntry* entryB = (const DiffEntry*) b;

  if (entryA->differenceExclusive < entryB->differenceExclusive) {
    return 1;
  } else if (entryA->differenceExclusive > entryB->differenceExclusive) {
    return -1;
  }

  return 0;
}

int32_t compareDiffEntriesInculsive(const void* a, const void* b) {
  const DiffEntry* entryA = (const DiffEntry*) a;
  const DiffEntry* entryB = (const DiffEntry*) b;

  if (entryA->differenceInclusive < entryB->differenceInclusive) {
    return 1;
  } else if (entryA->differenceInclusive > entryB->differenceInclusive) {
    return -1;
  }

  return 0;
}

void printMissingMethod(MethodEntry* method) {
  char classBuf[HTML_BUFSIZE];
  char methodBuf[HTML_BUFSIZE];

  char* className = htmlEscape(method->className, classBuf, HTML_BUFSIZE);
  char* methodName = htmlEscape(method->methodName, methodBuf, HTML_BUFSIZE);

  if (gOptions.outputHtml) printf("<tr><td>\n");

  printf("%s.%s ", className, methodName);
  if (gOptions.outputHtml) printf("</td><td>");

  printf("%" PRIu64 " ", method->elapsedExclusive);
  if (gOptions.outputHtml) printf("</td><td>");

  printf("%" PRIu64 " ", method->elapsedInclusive);
  if (gOptions.outputHtml) printf("</td><td>");

  printf("%d\n", method->numCalls[0]);
  if (gOptions.outputHtml) printf("</td><td>\n");
}

void createDiff(DataKeys* d1, DataKeys* d2) {
  MethodEntry** methods1 = parseMethodEntries(d1);
  MethodEntry** methods2 = parseMethodEntries(d2);

  // sort and assign the indicies
  qsort(methods1, d1->numMethods, sizeof(MethodEntry*), compareElapsedInclusive);
  for (int32_t i = 0; i < d1->numMethods; ++i) {
    methods1[i]->index = i;
  }

  qsort(methods2, d2->numMethods, sizeof(MethodEntry*), compareElapsedInclusive);
  for (int32_t i = 0; i < d2->numMethods; ++i) {
    methods2[i]->index = i;
  }

  int32_t max = (d1->numMethods < d2->numMethods) ? d2->numMethods : d1->numMethods;
  max++;
  DiffEntry* diffs = new DiffEntry[max];
  memset(diffs, 0, max * sizeof(DiffEntry));
  DiffEntry* ptr = diffs;

  // printf("<br>d1->numMethods: %d d1->numMethods: %d<br>\n",
  //        d1->numMethods, d2->numMethods);

  int32_t matches = 0;

  for (int32_t i = 0; i < d1->numMethods; i++) {
    int32_t match = findMatch(methods2, d2->numMethods, methods1[i]);
    if (match >= 0) {
      ptr->method1 = methods1[i];
      ptr->method2 = methods2[match];

      uint64_t e1 = ptr->method1->elapsedExclusive;
      uint64_t e2 = ptr->method2->elapsedExclusive;
      if (e1 > 0) {
        ptr->differenceExclusive = e2 - e1;
        ptr->differenceExclusivePercentage = (static_cast<double>(e2) /
                                              static_cast<double>(e1)) * 100.0;
      }

      uint64_t i1 = ptr->method1->elapsedInclusive;
      uint64_t i2 = ptr->method2->elapsedInclusive;
      if (i1 > 0) {
        ptr->differenceInclusive = i2 - i1;
        ptr->differenceInclusivePercentage = (static_cast<double>(i2) /
                                              static_cast<double>(i1)) * 100.0;
      }

      // clear these out so we don't find them again and we know which ones
      // we have left over
      methods1[i] = nullptr;
      methods2[match] = nullptr;
      ptr++;

      matches++;
    }
  }
  ptr->method1 = nullptr;
  ptr->method2 = nullptr;

  qsort(diffs, matches, sizeof(DiffEntry), compareDiffEntriesExculsive);
  ptr = diffs;

  if (gOptions.outputHtml) {
    printf(htmlHeader, gOptions.sortableUrl);
    printf("<h3>Table of Contents</h3>\n");
    printf("<ul>\n");
    printf("<li><a href='#exclusive'>Exclusive</a>\n");
    printf("<li><a href='#inclusive'>Inclusive</a>\n");
    printf("</ul>\n");
    printf("Run 1: %s<br>\n", gOptions.diffFileName);
    printf("Run 2: %s<br>\n", gOptions.traceFileName);
    printf("<a name=\"exclusive\"></a><h3 id=\"exclusive\">Exclusive</h3>\n");
    printf(tableHeader, "exclusive_table");
  }

  char classBuf[HTML_BUFSIZE];
  char methodBuf[HTML_BUFSIZE];
  while (ptr->method1 != nullptr && ptr->method2 != nullptr) {
    if (gOptions.outputHtml) printf("<tr><td>\n");

    char* className = htmlEscape(ptr->method1->className, classBuf, HTML_BUFSIZE);
    char* methodName = htmlEscape(ptr->method1->methodName, methodBuf, HTML_BUFSIZE);

    printf("%s.%s ", className, methodName);
    if (gOptions.outputHtml) printf("</td><td>");

    printf("%" PRIu64 " ", ptr->method1->elapsedExclusive);
    if (gOptions.outputHtml) printf("</td><td>");

    printf("%" PRIu64 " ", ptr->method2->elapsedExclusive);
    if (gOptions.outputHtml) printf("</td><td>");

    printf("%" PRIu64 " ", ptr->differenceExclusive);
    if (gOptions.outputHtml) printf("</td><td>");

    printf("%.2f\n", ptr->differenceExclusivePercentage);
    if (gOptions.outputHtml) printf("</td><td>\n");

    printf("%d\n", ptr->method1->numCalls[0]);
    if (gOptions.outputHtml) printf("</td><td>\n");

    printf("%d\n", ptr->method2->numCalls[0]);
    if (gOptions.outputHtml) printf("</td></tr>\n");

    ptr++;
  }

  if (gOptions.outputHtml) printf("</table>\n");

  if (gOptions.outputHtml) {
    printf(htmlHeader, gOptions.sortableUrl);
    printf("Run 1: %s<br>\n", gOptions.diffFileName);
    printf("Run 2: %s<br>\n", gOptions.traceFileName);
    printf("<a name=\"inclusive\"></a><h3 id=\"inculisve\">Inclusive</h3>\n");
    printf(tableHeader, "inclusive_table");
  }

  qsort(diffs, matches, sizeof(DiffEntry), compareDiffEntriesInculsive);
  ptr = diffs;

  while (ptr->method1 != nullptr && ptr->method2 != nullptr) {
    if (gOptions.outputHtml) printf("<tr><td>\n");

    char* className = htmlEscape(ptr->method1->className, classBuf, HTML_BUFSIZE);
    char* methodName = htmlEscape(ptr->method1->methodName, methodBuf, HTML_BUFSIZE);

    printf("%s.%s ", className, methodName);
    if (gOptions.outputHtml) printf("</td><td>");

    printf("%" PRIu64 " ", ptr->method1->elapsedInclusive);
    if (gOptions.outputHtml) printf("</td><td>");

    printf("%" PRIu64 " ", ptr->method2->elapsedInclusive);
    if (gOptions.outputHtml) printf("</td><td>");

    printf("%" PRIu64 " ", ptr->differenceInclusive);
    if (gOptions.outputHtml) printf("</td><td>");

    printf("%.2f\n", ptr->differenceInclusivePercentage);
    if (gOptions.outputHtml) printf("</td><td>\n");

    printf("%d\n", ptr->method1->numCalls[0]);
    if (gOptions.outputHtml) printf("</td><td>\n");

    printf("%d\n", ptr->method2->numCalls[0]);
    if (gOptions.outputHtml) printf("</td></tr>\n");

    ptr++;
  }

  if (gOptions.outputHtml) {
    printf("</table>\n");
    printf("<h3>Run 1 methods not found in Run 2</h3>");
    printf(tableHeaderMissing, "?");
  }

  for (int32_t i = 0; i < d1->numMethods; ++i) {
    if (methods1[i] != nullptr) {
      printMissingMethod(methods1[i]);
    }
  }

  if (gOptions.outputHtml) {
    printf("</table>\n");
    printf("<h3>Run 2 methods not found in Run 1</h3>");
    printf(tableHeaderMissing, "?");
  }

  for (int32_t i = 0; i < d2->numMethods; ++i) {
    if (methods2[i] != nullptr) {
      printMissingMethod(methods2[i]);
    }
  }

  if (gOptions.outputHtml) printf("</body></html\n");
}

int32_t usage(const char* program) {
  fprintf(stderr, "Copyright (C) 2006 The Android Open Source Project\n\n");
  fprintf(stderr,
          "usage: %s [-ho] [-s sortable] [-d trace-file-name] [-g outfile] "
          "trace-file-name\n",
          program);
  fprintf(stderr, "  -d trace-file-name  - Diff with this trace\n");
  fprintf(stderr, "  -g outfile          - Write graph to 'outfile'\n");
  fprintf(stderr,
          "  -k                  - When writing a graph, keep the intermediate "
          "DOT file\n");
  fprintf(stderr, "  -h                  - Turn on HTML output\n");
  fprintf(
      stderr,
      "  -o                  - Dump the dmtrace file instead of profiling\n");
  fprintf(stderr,
          "  -s                  - URL base to where the sortable javascript "
          "file\n");
  fprintf(stderr,
          "  -t threshold        - Threshold percentage for including nodes in "
          "the graph\n");
  return 2;
}

// Returns true if there was an error
int32_t parseOptions(int32_t argc, char** argv) {
  while (1) {
    int32_t opt = getopt(argc, argv, "d:hg:kos:t:");
    if (opt == -1) break;
    switch (opt) {
      case 'd':
        gOptions.diffFileName = optarg;
        break;
      case 'g':
        gOptions.graphFileName = optarg;
        break;
      case 'k':
        gOptions.keepDotFile = 1;
        break;
      case 'h':
        gOptions.outputHtml = 1;
        break;
      case 'o':
        gOptions.dump = 1;
        break;
      case 's':
        gOptions.sortableUrl = optarg;
        break;
      case 't':
        gOptions.threshold = atoi(optarg);
        break;
      default:
        return 1;
    }
  }
  return 0;
}

/*
 * Parse args.
 */
int32_t main(int32_t argc, char** argv) {
  gOptions.threshold = -1;

  // Parse the options
  if (parseOptions(argc, argv) || argc - optind != 1) return usage(argv[0]);

  gOptions.traceFileName = argv[optind];

  if (gOptions.threshold < 0 || 100 <= gOptions.threshold) {
    gOptions.threshold = 20;
  }

  if (gOptions.dump) {
    dumpTrace();
    return 0;
  }

  uint64_t sumThreadTime = 0;

  TraceData data1;
  DataKeys* dataKeys = parseDataKeys(&data1, gOptions.traceFileName, &sumThreadTime);
  if (dataKeys == nullptr) {
    fprintf(stderr, "Cannot read \"%s\".\n", gOptions.traceFileName);
    exit(1);
  }

  if (gOptions.diffFileName != nullptr) {
    uint64_t sum2;
    TraceData data2;
    DataKeys* d2 = parseDataKeys(&data2, gOptions.diffFileName, &sum2);
    if (d2 == nullptr) {
      fprintf(stderr, "Cannot read \"%s\".\n", gOptions.diffFileName);
      exit(1);
    }

    createDiff(d2, dataKeys);

    freeDataKeys(d2);
  } else {
    MethodEntry** methods = parseMethodEntries(dataKeys);
    profileTrace(&data1, methods, dataKeys->numMethods, sumThreadTime);
    if (gOptions.graphFileName != nullptr) {
      createInclusiveProfileGraphNew(dataKeys);
    }
    delete[] methods;
  }

  freeDataKeys(dataKeys);

  return 0;
}
