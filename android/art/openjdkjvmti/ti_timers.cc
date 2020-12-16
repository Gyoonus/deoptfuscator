/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "ti_timers.h"

#include <limits>

#ifndef __APPLE__
#include <time.h>
#else
#include <sys/time.h>
#endif
#include <unistd.h>

#include "art_jvmti.h"
#include "base/macros.h"

namespace openjdkjvmti {

jvmtiError TimerUtil::GetAvailableProcessors(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                             jint* processor_count_ptr) {
  if (processor_count_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  *processor_count_ptr = static_cast<jint>(sysconf(_SC_NPROCESSORS_CONF));

  return ERR(NONE);
}

jvmtiError TimerUtil::GetTimerInfo(jvmtiEnv* env ATTRIBUTE_UNUSED, jvmtiTimerInfo* info_ptr) {
  if (info_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  info_ptr->max_value = static_cast<jlong>(std::numeric_limits<uint64_t>::max());
  info_ptr->may_skip_forward = JNI_TRUE;
  info_ptr->may_skip_backward = JNI_TRUE;
  info_ptr->kind = jvmtiTimerKind::JVMTI_TIMER_ELAPSED;

  return ERR(NONE);
}

jvmtiError TimerUtil::GetTime(jvmtiEnv* env ATTRIBUTE_UNUSED, jlong* nanos_ptr) {
  if (nanos_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

#ifndef __APPLE__
  // Use the same implementation as System.nanoTime.
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  *nanos_ptr = now.tv_sec * 1000000000LL + now.tv_nsec;
#else
  // No CLOCK_MONOTONIC support on older Mac OS.
  struct timeval t;
  t.tv_sec = t.tv_usec = 0;
  gettimeofday(&t, NULL);
  *nanos_ptr = static_cast<jlong>(t.tv_sec)*1000000000LL + static_cast<jlong>(t.tv_usec)*1000LL;
#endif

  return ERR(NONE);
}

}  // namespace openjdkjvmti
