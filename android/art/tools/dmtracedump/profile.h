/*
 * Copyright (C) 2008 The Android Open Source Project
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
 * Android's method call profiling goodies.
 */
#ifndef ART_TOOLS_DMTRACEDUMP_PROFILE_H_
#define ART_TOOLS_DMTRACEDUMP_PROFILE_H_

/*
 * Enumeration for the two "action" bits.
 */
enum {
  METHOD_TRACE_ENTER = 0x00,   // method entry
  METHOD_TRACE_EXIT = 0x01,    // method exit
  METHOD_TRACE_UNROLL = 0x02,  // method exited by exception unrolling
  // 0x03 currently unused
};

#define TOKEN_CHAR '*'

/*
 * Common definitions, shared with the dump tool.
 */
#define METHOD_ACTION_MASK 0x03 /* two bits */
#define METHOD_ID(_method) ((_method) & (~METHOD_ACTION_MASK))
#define METHOD_ACTION(_method) (((unsigned int)(_method)) & METHOD_ACTION_MASK)
#define METHOD_COMBINE(_method, _action) ((_method) | (_action))

#endif  // ART_TOOLS_DMTRACEDUMP_PROFILE_H_
