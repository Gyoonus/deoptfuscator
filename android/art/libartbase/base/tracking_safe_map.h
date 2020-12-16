/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_TRACKING_SAFE_MAP_H_
#define ART_LIBARTBASE_BASE_TRACKING_SAFE_MAP_H_

#include "base/allocator.h"
#include "base/safe_map.h"

namespace art {

template<class Key, class T, AllocatorTag kTag, class Compare = std::less<Key>>
class AllocationTrackingSafeMap : public SafeMap<
    Key, T, Compare, TrackingAllocator<std::pair<const Key, T>, kTag>> {
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_TRACKING_SAFE_MAP_H_
