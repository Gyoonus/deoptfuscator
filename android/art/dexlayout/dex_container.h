/*
 * Copyright (C) 2018 The Android Open Source Project
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
 * Header file of an in-memory representation of DEX files.
 */

#ifndef ART_DEXLAYOUT_DEX_CONTAINER_H_
#define ART_DEXLAYOUT_DEX_CONTAINER_H_

#include <vector>

namespace art {

// Dex container holds the artifacts produced by dexlayout and contains up to two sections: a main
// section and a data section.
// This container may also hold metadata used for multi dex deduplication in the future.
class DexContainer {
 public:
  virtual ~DexContainer() {}

  class Section {
   public:
    virtual ~Section() {}

    // Returns the start of the memory region.
    virtual uint8_t* Begin() = 0;

    // Size in bytes.
    virtual size_t Size() const = 0;

    // Resize the backing storage.
    virtual void Resize(size_t size) = 0;

    // Clear the container.
    virtual void Clear() = 0;

    // Returns the end of the memory region.
    uint8_t* End() {
      return Begin() + Size();
    }
  };

  // Vector backed section.
  class VectorSection : public Section {
   public:
    virtual ~VectorSection() {}

    uint8_t* Begin() OVERRIDE {
      return &data_[0];
    }

    size_t Size() const OVERRIDE {
      return data_.size();
    }

    void Resize(size_t size) OVERRIDE {
      data_.resize(size, 0u);
    }

    void Clear() OVERRIDE {
      data_.clear();
    }

   private:
    std::vector<uint8_t> data_;
  };

  virtual Section* GetMainSection() = 0;
  virtual Section* GetDataSection() = 0;
  virtual bool IsCompactDexContainer() const = 0;
};

}  // namespace art

#endif  // ART_DEXLAYOUT_DEX_CONTAINER_H_
