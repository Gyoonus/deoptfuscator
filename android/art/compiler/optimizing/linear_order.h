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
 */

#ifndef ART_COMPILER_OPTIMIZING_LINEAR_ORDER_H_
#define ART_COMPILER_OPTIMIZING_LINEAR_ORDER_H_

#include <type_traits>

#include "nodes.h"

namespace art {

void LinearizeGraphInternal(const HGraph* graph, ArrayRef<HBasicBlock*> linear_order);

// Linearizes the 'graph' such that:
// (1): a block is always after its dominator,
// (2): blocks of loops are contiguous.
//
// Storage is obtained through 'allocator' and the linear order it computed
// into 'linear_order'. Once computed, iteration can be expressed as:
//
// for (HBasicBlock* block : linear_order)                   // linear order
//
// for (HBasicBlock* block : ReverseRange(linear_order))     // linear post order
//
template <typename Vector>
void LinearizeGraph(const HGraph* graph, Vector* linear_order) {
  static_assert(std::is_same<HBasicBlock*, typename Vector::value_type>::value,
                "Vector::value_type must be HBasicBlock*.");
  // Resize the vector and pass an ArrayRef<> to internal implementation which is shared
  // for all kinds of vectors, i.e. ArenaVector<> or ScopedArenaVector<>.
  linear_order->resize(graph->GetReversePostOrder().size());
  LinearizeGraphInternal(graph, ArrayRef<HBasicBlock*>(*linear_order));
}

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_LINEAR_ORDER_H_
