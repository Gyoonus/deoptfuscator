/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * Implementation file of dex ir verifier.
 *
 * Compares two dex files at the IR level, allowing differences in layout, but not in data.
 */

#include "dex_verify.h"

#include <inttypes.h>

#include "android-base/stringprintf.h"

namespace art {

using android::base::StringPrintf;

bool VerifyOutputDexFile(dex_ir::Header* orig_header,
                         dex_ir::Header* output_header,
                         std::string* error_msg) {
  dex_ir::Collections& orig = orig_header->GetCollections();
  dex_ir::Collections& output = output_header->GetCollections();

  // Compare all id sections. They have a defined order that can't be changed by dexlayout.
  if (!VerifyIds(orig.StringIds(), output.StringIds(), "string ids", error_msg) ||
      !VerifyIds(orig.TypeIds(), output.TypeIds(), "type ids", error_msg) ||
      !VerifyIds(orig.ProtoIds(), output.ProtoIds(), "proto ids", error_msg) ||
      !VerifyIds(orig.FieldIds(), output.FieldIds(), "field ids", error_msg) ||
      !VerifyIds(orig.MethodIds(), output.MethodIds(), "method ids", error_msg)) {
    return false;
  }
  // Compare class defs. The order may have been changed by dexlayout.
  if (!VerifyClassDefs(orig.ClassDefs(), output.ClassDefs(), error_msg)) {
    return false;
  }
  return true;
}

template<class T> bool VerifyIds(std::vector<std::unique_ptr<T>>& orig,
                                 std::vector<std::unique_ptr<T>>& output,
                                 const char* section_name,
                                 std::string* error_msg) {
  if (orig.size() != output.size()) {
    *error_msg = StringPrintf(
        "Mismatched size for %s section: %zu vs %zu.", section_name, orig.size(), output.size());
    return false;
  }
  for (size_t i = 0; i < orig.size(); ++i) {
    if (!VerifyId(orig[i].get(), output[i].get(), error_msg)) {
      return false;
    }
  }
  return true;
}

bool VerifyId(dex_ir::StringId* orig, dex_ir::StringId* output, std::string* error_msg) {
  if (strcmp(orig->Data(), output->Data()) != 0) {
    *error_msg = StringPrintf("Mismatched string data for string id %u at offset %x: %s vs %s.",
                              orig->GetIndex(),
                              orig->GetOffset(),
                              orig->Data(),
                              output->Data());
    return false;
  }
  return true;
}

bool VerifyId(dex_ir::TypeId* orig, dex_ir::TypeId* output, std::string* error_msg) {
  if (orig->GetStringId()->GetIndex() != output->GetStringId()->GetIndex()) {
    *error_msg = StringPrintf("Mismatched string index for type id %u at offset %x: %u vs %u.",
                              orig->GetIndex(),
                              orig->GetOffset(),
                              orig->GetStringId()->GetIndex(),
                              output->GetStringId()->GetIndex());
    return false;
  }
  return true;
}

bool VerifyId(dex_ir::ProtoId* orig, dex_ir::ProtoId* output, std::string* error_msg) {
  if (orig->Shorty()->GetIndex() != output->Shorty()->GetIndex()) {
    *error_msg = StringPrintf("Mismatched string index for proto id %u at offset %x: %u vs %u.",
                              orig->GetIndex(),
                              orig->GetOffset(),
                              orig->Shorty()->GetIndex(),
                              output->Shorty()->GetIndex());
    return false;
  }
  if (orig->ReturnType()->GetIndex() != output->ReturnType()->GetIndex()) {
    *error_msg = StringPrintf("Mismatched type index for proto id %u at offset %x: %u vs %u.",
                              orig->GetIndex(),
                              orig->GetOffset(),
                              orig->ReturnType()->GetIndex(),
                              output->ReturnType()->GetIndex());
    return false;
  }
  if (!VerifyTypeList(orig->Parameters(), output->Parameters())) {
    *error_msg = StringPrintf("Mismatched type list for proto id %u at offset %x.",
                              orig->GetIndex(),
                              orig->GetOffset());
  }
  return true;
}

bool VerifyId(dex_ir::FieldId* orig, dex_ir::FieldId* output, std::string* error_msg) {
  if (orig->Class()->GetIndex() != output->Class()->GetIndex()) {
    *error_msg =
        StringPrintf("Mismatched class type index for field id %u at offset %x: %u vs %u.",
                     orig->GetIndex(),
                     orig->GetOffset(),
                     orig->Class()->GetIndex(),
                     output->Class()->GetIndex());
    return false;
  }
  if (orig->Type()->GetIndex() != output->Type()->GetIndex()) {
    *error_msg = StringPrintf("Mismatched type index for field id %u at offset %x: %u vs %u.",
                              orig->GetIndex(),
                              orig->GetOffset(),
                              orig->Class()->GetIndex(),
                              output->Class()->GetIndex());
    return false;
  }
  if (orig->Name()->GetIndex() != output->Name()->GetIndex()) {
    *error_msg = StringPrintf("Mismatched string index for field id %u at offset %x: %u vs %u.",
                              orig->GetIndex(),
                              orig->GetOffset(),
                              orig->Name()->GetIndex(),
                              output->Name()->GetIndex());
    return false;
  }
  return true;
}

bool VerifyId(dex_ir::MethodId* orig, dex_ir::MethodId* output, std::string* error_msg) {
  if (orig->Class()->GetIndex() != output->Class()->GetIndex()) {
    *error_msg = StringPrintf("Mismatched type index for method id %u at offset %x: %u vs %u.",
                              orig->GetIndex(),
                              orig->GetOffset(),
                              orig->Class()->GetIndex(),
                              output->Class()->GetIndex());
    return false;
  }
  if (orig->Proto()->GetIndex() != output->Proto()->GetIndex()) {
    *error_msg = StringPrintf("Mismatched proto index for method id %u at offset %x: %u vs %u.",
                              orig->GetIndex(),
                              orig->GetOffset(),
                              orig->Class()->GetIndex(),
                              output->Class()->GetIndex());
    return false;
  }
  if (orig->Name()->GetIndex() != output->Name()->GetIndex()) {
    *error_msg =
        StringPrintf("Mismatched string index for method id %u at offset %x: %u vs %u.",
                     orig->GetIndex(),
                     orig->GetOffset(),
                     orig->Name()->GetIndex(),
                     output->Name()->GetIndex());
    return false;
  }
  return true;
}

struct ClassDefCompare {
  bool operator()(dex_ir::ClassDef* lhs, dex_ir::ClassDef* rhs) const {
    return lhs->ClassType()->GetIndex() < rhs->ClassType()->GetIndex();
  }
};

// The class defs may have a new order due to dexlayout. Use the class's class_idx to uniquely
// identify them and sort them for comparison.
bool VerifyClassDefs(std::vector<std::unique_ptr<dex_ir::ClassDef>>& orig,
                     std::vector<std::unique_ptr<dex_ir::ClassDef>>& output,
                     std::string* error_msg) {
  if (orig.size() != output.size()) {
    *error_msg = StringPrintf(
        "Mismatched size for class defs section: %zu vs %zu.", orig.size(), output.size());
    return false;
  }
  // Store the class defs into sets sorted by the class's type index.
  std::set<dex_ir::ClassDef*, ClassDefCompare> orig_set;
  std::set<dex_ir::ClassDef*, ClassDefCompare> output_set;
  for (size_t i = 0; i < orig.size(); ++i) {
    orig_set.insert(orig[i].get());
    output_set.insert(output[i].get());
  }
  auto orig_iter = orig_set.begin();
  auto output_iter = output_set.begin();
  while (orig_iter != orig_set.end() && output_iter != output_set.end()) {
    if (!VerifyClassDef(*orig_iter, *output_iter, error_msg)) {
      return false;
    }
    orig_iter++;
    output_iter++;
  }
  return true;
}

bool VerifyClassDef(dex_ir::ClassDef* orig, dex_ir::ClassDef* output, std::string* error_msg) {
  if (orig->ClassType()->GetIndex() != output->ClassType()->GetIndex()) {
    *error_msg =
        StringPrintf("Mismatched class type index for class def %u at offset %x: %u vs %u.",
                     orig->GetIndex(),
                     orig->GetOffset(),
                     orig->ClassType()->GetIndex(),
                     output->ClassType()->GetIndex());
    return false;
  }
  if (orig->GetAccessFlags() != output->GetAccessFlags()) {
    *error_msg =
        StringPrintf("Mismatched access flags for class def %u at offset %x: %x vs %x.",
                     orig->GetIndex(),
                     orig->GetOffset(),
                     orig->GetAccessFlags(),
                     output->GetAccessFlags());
    return false;
  }
  uint32_t orig_super = orig->Superclass() == nullptr ? 0 : orig->Superclass()->GetIndex();
  uint32_t output_super = output->Superclass() == nullptr ? 0 : output->Superclass()->GetIndex();
  if (orig_super != output_super) {
    *error_msg =
        StringPrintf("Mismatched super class for class def %u at offset %x: %u vs %u.",
                     orig->GetIndex(),
                     orig->GetOffset(),
                     orig_super,
                     output_super);
    return false;
  }
  if (!VerifyTypeList(orig->Interfaces(), output->Interfaces())) {
    *error_msg = StringPrintf("Mismatched type list for class def %u at offset %x.",
                              orig->GetIndex(),
                              orig->GetOffset());
    return false;
  }
  const char* orig_source = orig->SourceFile() == nullptr ? "" : orig->SourceFile()->Data();
  const char* output_source = output->SourceFile() == nullptr ? "" : output->SourceFile()->Data();
  if (strcmp(orig_source, output_source) != 0) {
    *error_msg = StringPrintf("Mismatched source file for class def %u at offset %x: %s vs %s.",
                              orig->GetIndex(),
                              orig->GetOffset(),
                              orig_source,
                              output_source);
    return false;
  }
  if (!VerifyAnnotationsDirectory(orig->Annotations(), output->Annotations(), error_msg)) {
    return false;
  }
  if (!VerifyClassData(orig->GetClassData(), output->GetClassData(), error_msg)) {
    return false;
  }
  return VerifyEncodedArray(orig->StaticValues(), output->StaticValues(), error_msg);
}

bool VerifyTypeList(const dex_ir::TypeList* orig, const dex_ir::TypeList* output) {
  if (orig == nullptr || output == nullptr) {
    return orig == output;
  }
  const dex_ir::TypeIdVector* orig_list = orig->GetTypeList();
  const dex_ir::TypeIdVector* output_list = output->GetTypeList();
  if (orig_list->size() != output_list->size()) {
    return false;
  }
  for (size_t i = 0; i < orig_list->size(); ++i) {
    if ((*orig_list)[i]->GetIndex() != (*output_list)[i]->GetIndex()) {
      return false;
    }
  }
  return true;
}

bool VerifyAnnotationsDirectory(dex_ir::AnnotationsDirectoryItem* orig,
                                dex_ir::AnnotationsDirectoryItem* output,
                                std::string* error_msg) {
  if (orig == nullptr || output == nullptr) {
    if (orig != output) {
      *error_msg = "Found unexpected empty annotations directory.";
      return false;
    }
    return true;
  }
  if (!VerifyAnnotationSet(orig->GetClassAnnotation(), output->GetClassAnnotation(), error_msg)) {
    return false;
  }
  if (!VerifyFieldAnnotations(orig->GetFieldAnnotations(),
                              output->GetFieldAnnotations(),
                              orig->GetOffset(),
                              error_msg)) {
    return false;
  }
  if (!VerifyMethodAnnotations(orig->GetMethodAnnotations(),
                               output->GetMethodAnnotations(),
                               orig->GetOffset(),
                               error_msg)) {
    return false;
  }
  return VerifyParameterAnnotations(orig->GetParameterAnnotations(),
                                    output->GetParameterAnnotations(),
                                    orig->GetOffset(),
                                    error_msg);
}

bool VerifyFieldAnnotations(dex_ir::FieldAnnotationVector* orig,
                            dex_ir::FieldAnnotationVector* output,
                            uint32_t orig_offset,
                            std::string* error_msg) {
  if (orig == nullptr || output == nullptr) {
    if (orig != output) {
      *error_msg = StringPrintf(
          "Found unexpected empty field annotations for annotations directory at offset %x.",
          orig_offset);
      return false;
    }
    return true;
  }
  if (orig->size() != output->size()) {
    *error_msg = StringPrintf(
        "Mismatched field annotations size for annotations directory at offset %x: %zu vs %zu.",
        orig_offset,
        orig->size(),
        output->size());
    return false;
  }
  for (size_t i = 0; i < orig->size(); ++i) {
    dex_ir::FieldAnnotation* orig_field = (*orig)[i].get();
    dex_ir::FieldAnnotation* output_field = (*output)[i].get();
    if (orig_field->GetFieldId()->GetIndex() != output_field->GetFieldId()->GetIndex()) {
      *error_msg = StringPrintf(
          "Mismatched field annotation index for annotations directory at offset %x: %u vs %u.",
          orig_offset,
          orig_field->GetFieldId()->GetIndex(),
          output_field->GetFieldId()->GetIndex());
      return false;
    }
    if (!VerifyAnnotationSet(orig_field->GetAnnotationSetItem(),
                             output_field->GetAnnotationSetItem(),
                             error_msg)) {
      return false;
    }
  }
  return true;
}

bool VerifyMethodAnnotations(dex_ir::MethodAnnotationVector* orig,
                             dex_ir::MethodAnnotationVector* output,
                             uint32_t orig_offset,
                             std::string* error_msg) {
  if (orig == nullptr || output == nullptr) {
    if (orig != output) {
      *error_msg = StringPrintf(
          "Found unexpected empty method annotations for annotations directory at offset %x.",
          orig_offset);
      return false;
    }
    return true;
  }
  if (orig->size() != output->size()) {
    *error_msg = StringPrintf(
        "Mismatched method annotations size for annotations directory at offset %x: %zu vs %zu.",
        orig_offset,
        orig->size(),
        output->size());
    return false;
  }
  for (size_t i = 0; i < orig->size(); ++i) {
    dex_ir::MethodAnnotation* orig_method = (*orig)[i].get();
    dex_ir::MethodAnnotation* output_method = (*output)[i].get();
    if (orig_method->GetMethodId()->GetIndex() != output_method->GetMethodId()->GetIndex()) {
      *error_msg = StringPrintf(
          "Mismatched method annotation index for annotations directory at offset %x: %u vs %u.",
          orig_offset,
          orig_method->GetMethodId()->GetIndex(),
          output_method->GetMethodId()->GetIndex());
      return false;
    }
    if (!VerifyAnnotationSet(orig_method->GetAnnotationSetItem(),
                             output_method->GetAnnotationSetItem(),
                             error_msg)) {
      return false;
    }
  }
  return true;
}

bool VerifyParameterAnnotations(dex_ir::ParameterAnnotationVector* orig,
                                dex_ir::ParameterAnnotationVector* output,
                                uint32_t orig_offset,
                                std::string* error_msg) {
  if (orig == nullptr || output == nullptr) {
    if (orig != output) {
      *error_msg = StringPrintf(
          "Found unexpected empty parameter annotations for annotations directory at offset %x.",
          orig_offset);
      return false;
    }
    return true;
  }
  if (orig->size() != output->size()) {
    *error_msg = StringPrintf(
        "Mismatched parameter annotations size for annotations directory at offset %x: %zu vs %zu.",
        orig_offset,
        orig->size(),
        output->size());
    return false;
  }
  for (size_t i = 0; i < orig->size(); ++i) {
    dex_ir::ParameterAnnotation* orig_param = (*orig)[i].get();
    dex_ir::ParameterAnnotation* output_param = (*output)[i].get();
    if (orig_param->GetMethodId()->GetIndex() != output_param->GetMethodId()->GetIndex()) {
      *error_msg = StringPrintf(
          "Mismatched parameter annotation index for annotations directory at offset %x: %u vs %u.",
          orig_offset,
          orig_param->GetMethodId()->GetIndex(),
          output_param->GetMethodId()->GetIndex());
      return false;
    }
    if (!VerifyAnnotationSetRefList(orig_param->GetAnnotations(),
                                    output_param->GetAnnotations(),
                                    error_msg)) {
      return false;
    }
  }
  return true;
}

bool VerifyAnnotationSetRefList(dex_ir::AnnotationSetRefList* orig,
                                dex_ir::AnnotationSetRefList* output,
                                std::string* error_msg) {
  std::vector<dex_ir::AnnotationSetItem*>* orig_items = orig->GetItems();
  std::vector<dex_ir::AnnotationSetItem*>* output_items = output->GetItems();
  if (orig_items->size() != output_items->size()) {
    *error_msg = StringPrintf(
        "Mismatched annotation set ref list size at offset %x: %zu vs %zu.",
        orig->GetOffset(),
        orig_items->size(),
        output_items->size());
    return false;
  }
  for (size_t i = 0; i < orig_items->size(); ++i) {
    if (!VerifyAnnotationSet((*orig_items)[i], (*output_items)[i], error_msg)) {
      return false;
    }
  }
  return true;
}

bool VerifyAnnotationSet(dex_ir::AnnotationSetItem* orig,
                         dex_ir::AnnotationSetItem* output,
                         std::string* error_msg) {
  if (orig == nullptr || output == nullptr) {
    if (orig != output) {
      *error_msg = "Found unexpected empty annotation set.";
      return false;
    }
    return true;
  }
  std::vector<dex_ir::AnnotationItem*>* orig_items = orig->GetItems();
  std::vector<dex_ir::AnnotationItem*>* output_items = output->GetItems();
  if (orig_items->size() != output_items->size()) {
    *error_msg = StringPrintf("Mismatched size for annotation set at offset %x: %zu vs %zu.",
                              orig->GetOffset(),
                              orig_items->size(),
                              output_items->size());
    return false;
  }
  for (size_t i = 0; i < orig_items->size(); ++i) {
    if (!VerifyAnnotation((*orig_items)[i], (*output_items)[i], error_msg)) {
      return false;
    }
  }
  return true;
}

bool VerifyAnnotation(dex_ir::AnnotationItem* orig,
                      dex_ir::AnnotationItem* output,
                      std::string* error_msg) {
  if (orig->GetVisibility() != output->GetVisibility()) {
    *error_msg = StringPrintf("Mismatched visibility for annotation at offset %x: %u vs %u.",
                              orig->GetOffset(),
                              orig->GetVisibility(),
                              output->GetVisibility());
    return false;
  }
  return VerifyEncodedAnnotation(orig->GetAnnotation(),
                                 output->GetAnnotation(),
                                 orig->GetOffset(),
                                 error_msg);
}

bool VerifyEncodedAnnotation(dex_ir::EncodedAnnotation* orig,
                             dex_ir::EncodedAnnotation* output,
                             uint32_t orig_offset,
                             std::string* error_msg) {
  if (orig->GetType()->GetIndex() != output->GetType()->GetIndex()) {
    *error_msg = StringPrintf(
        "Mismatched encoded annotation type for annotation at offset %x: %u vs %u.",
        orig_offset,
        orig->GetType()->GetIndex(),
        output->GetType()->GetIndex());
    return false;
  }
  dex_ir::AnnotationElementVector* orig_elements = orig->GetAnnotationElements();
  dex_ir::AnnotationElementVector* output_elements = output->GetAnnotationElements();
  if (orig_elements->size() != output_elements->size()) {
    *error_msg = StringPrintf(
        "Mismatched encoded annotation size for annotation at offset %x: %zu vs %zu.",
        orig_offset,
        orig_elements->size(),
        output_elements->size());
    return false;
  }
  for (size_t i = 0; i < orig_elements->size(); ++i) {
    if (!VerifyAnnotationElement((*orig_elements)[i].get(),
                                 (*output_elements)[i].get(),
                                 orig_offset,
                                 error_msg)) {
      return false;
    }
  }
  return true;
}

bool VerifyAnnotationElement(dex_ir::AnnotationElement* orig,
                             dex_ir::AnnotationElement* output,
                             uint32_t orig_offset,
                             std::string* error_msg) {
  if (orig->GetName()->GetIndex() != output->GetName()->GetIndex()) {
    *error_msg = StringPrintf(
        "Mismatched annotation element name for annotation at offset %x: %u vs %u.",
        orig_offset,
        orig->GetName()->GetIndex(),
        output->GetName()->GetIndex());
    return false;
  }
  return VerifyEncodedValue(orig->GetValue(), output->GetValue(), orig_offset, error_msg);
}

bool VerifyEncodedValue(dex_ir::EncodedValue* orig,
                        dex_ir::EncodedValue* output,
                        uint32_t orig_offset,
                        std::string* error_msg) {
  if (orig->Type() != output->Type()) {
    *error_msg = StringPrintf(
        "Mismatched encoded value type for annotation or encoded array at offset %x: %d vs %d.",
        orig_offset,
        orig->Type(),
        output->Type());
    return false;
  }
  switch (orig->Type()) {
    case DexFile::kDexAnnotationByte:
      if (orig->GetByte() != output->GetByte()) {
        *error_msg = StringPrintf("Mismatched encoded byte for annotation at offset %x: %d vs %d.",
                                  orig_offset,
                                  orig->GetByte(),
                                  output->GetByte());
        return false;
      }
      break;
    case DexFile::kDexAnnotationShort:
      if (orig->GetShort() != output->GetShort()) {
        *error_msg = StringPrintf("Mismatched encoded short for annotation at offset %x: %d vs %d.",
                                  orig_offset,
                                  orig->GetShort(),
                                  output->GetShort());
        return false;
      }
      break;
    case DexFile::kDexAnnotationChar:
      if (orig->GetChar() != output->GetChar()) {
        *error_msg = StringPrintf("Mismatched encoded char for annotation at offset %x: %c vs %c.",
                                  orig_offset,
                                  orig->GetChar(),
                                  output->GetChar());
        return false;
      }
      break;
    case DexFile::kDexAnnotationInt:
      if (orig->GetInt() != output->GetInt()) {
        *error_msg = StringPrintf("Mismatched encoded int for annotation at offset %x: %d vs %d.",
                                  orig_offset,
                                  orig->GetInt(),
                                  output->GetInt());
        return false;
      }
      break;
    case DexFile::kDexAnnotationLong:
      if (orig->GetLong() != output->GetLong()) {
        *error_msg = StringPrintf(
            "Mismatched encoded long for annotation at offset %x: %" PRId64 " vs %" PRId64 ".",
            orig_offset,
            orig->GetLong(),
            output->GetLong());
        return false;
      }
      break;
    case DexFile::kDexAnnotationFloat:
      // The float value is encoded, so compare as if it's an int.
      if (orig->GetInt() != output->GetInt()) {
        *error_msg = StringPrintf(
            "Mismatched encoded float for annotation at offset %x: %x (encoded) vs %x (encoded).",
                                  orig_offset,
                                  orig->GetInt(),
                                  output->GetInt());
        return false;
      }
      break;
    case DexFile::kDexAnnotationDouble:
      // The double value is encoded, so compare as if it's a long.
      if (orig->GetLong() != output->GetLong()) {
        *error_msg = StringPrintf(
            "Mismatched encoded double for annotation at offset %x: %" PRIx64
            " (encoded) vs %" PRIx64 " (encoded).",
            orig_offset,
            orig->GetLong(),
            output->GetLong());
        return false;
      }
      break;
    case DexFile::kDexAnnotationString:
      if (orig->GetStringId()->GetIndex() != output->GetStringId()->GetIndex()) {
        *error_msg = StringPrintf(
            "Mismatched encoded string for annotation at offset %x: %s vs %s.",
            orig_offset,
            orig->GetStringId()->Data(),
            output->GetStringId()->Data());
        return false;
      }
      break;
    case DexFile::kDexAnnotationType:
      if (orig->GetTypeId()->GetIndex() != output->GetTypeId()->GetIndex()) {
        *error_msg = StringPrintf("Mismatched encoded type for annotation at offset %x: %u vs %u.",
                                  orig_offset,
                                  orig->GetTypeId()->GetIndex(),
                                  output->GetTypeId()->GetIndex());
        return false;
      }
      break;
    case DexFile::kDexAnnotationField:
    case DexFile::kDexAnnotationEnum:
      if (orig->GetFieldId()->GetIndex() != output->GetFieldId()->GetIndex()) {
        *error_msg = StringPrintf("Mismatched encoded field for annotation at offset %x: %u vs %u.",
                                  orig_offset,
                                  orig->GetFieldId()->GetIndex(),
                                  output->GetFieldId()->GetIndex());
        return false;
      }
      break;
    case DexFile::kDexAnnotationMethod:
      if (orig->GetMethodId()->GetIndex() != output->GetMethodId()->GetIndex()) {
        *error_msg = StringPrintf(
            "Mismatched encoded method for annotation at offset %x: %u vs %u.",
            orig_offset,
            orig->GetMethodId()->GetIndex(),
            output->GetMethodId()->GetIndex());
        return false;
      }
      break;
    case DexFile::kDexAnnotationArray:
      if (!VerifyEncodedArray(orig->GetEncodedArray(), output->GetEncodedArray(), error_msg)) {
        return false;
      }
      break;
    case DexFile::kDexAnnotationAnnotation:
      if (!VerifyEncodedAnnotation(orig->GetEncodedAnnotation(),
                                   output->GetEncodedAnnotation(),
                                   orig_offset,
                                   error_msg)) {
        return false;
      }
      break;
    case DexFile::kDexAnnotationNull:
      break;
    case DexFile::kDexAnnotationBoolean:
      if (orig->GetBoolean() != output->GetBoolean()) {
        *error_msg = StringPrintf(
            "Mismatched encoded boolean for annotation at offset %x: %d vs %d.",
            orig_offset,
            orig->GetBoolean(),
            output->GetBoolean());
        return false;
      }
      break;
    default:
      break;
  }
  return true;
}

bool VerifyEncodedArray(dex_ir::EncodedArrayItem* orig,
                        dex_ir::EncodedArrayItem* output,
                        std::string* error_msg) {
  if (orig == nullptr || output == nullptr) {
    if (orig != output) {
      *error_msg = "Found unexpected empty encoded array.";
      return false;
    }
    return true;
  }
  dex_ir::EncodedValueVector* orig_vector = orig->GetEncodedValues();
  dex_ir::EncodedValueVector* output_vector = output->GetEncodedValues();
  if (orig_vector->size() != output_vector->size()) {
    *error_msg = StringPrintf("Mismatched size for encoded array at offset %x: %zu vs %zu.",
                              orig->GetOffset(),
                              orig_vector->size(),
                              output_vector->size());
    return false;
  }
  for (size_t i = 0; i < orig_vector->size(); ++i) {
    if (!VerifyEncodedValue((*orig_vector)[i].get(),
                            (*output_vector)[i].get(),
                            orig->GetOffset(),
                            error_msg)) {
      return false;
    }
  }
  return true;
}

bool VerifyClassData(dex_ir::ClassData* orig, dex_ir::ClassData* output, std::string* error_msg) {
  if (orig == nullptr || output == nullptr) {
    if (orig != output) {
      *error_msg = "Found unexpected empty class data.";
      return false;
    }
    return true;
  }
  if (!VerifyFields(orig->StaticFields(), output->StaticFields(), orig->GetOffset(), error_msg)) {
    return false;
  }
  if (!VerifyFields(orig->InstanceFields(),
                    output->InstanceFields(),
                    orig->GetOffset(),
                    error_msg)) {
    return false;
  }
  if (!VerifyMethods(orig->DirectMethods(),
                     output->DirectMethods(),
                     orig->GetOffset(),
                     error_msg)) {
    return false;
  }
  return VerifyMethods(orig->VirtualMethods(),
                       output->VirtualMethods(),
                       orig->GetOffset(),
                       error_msg);
}

bool VerifyFields(dex_ir::FieldItemVector* orig,
                  dex_ir::FieldItemVector* output,
                  uint32_t orig_offset,
                  std::string* error_msg) {
  if (orig->size() != output->size()) {
    *error_msg = StringPrintf("Mismatched fields size for class data at offset %x: %zu vs %zu.",
                              orig_offset,
                              orig->size(),
                              output->size());
    return false;
  }
  for (size_t i = 0; i < orig->size(); ++i) {
    dex_ir::FieldItem* orig_field = (*orig)[i].get();
    dex_ir::FieldItem* output_field = (*output)[i].get();
    if (orig_field->GetFieldId()->GetIndex() != output_field->GetFieldId()->GetIndex()) {
      *error_msg = StringPrintf("Mismatched field index for class data at offset %x: %u vs %u.",
                                orig_offset,
                                orig_field->GetFieldId()->GetIndex(),
                                output_field->GetFieldId()->GetIndex());
      return false;
    }
    if (orig_field->GetAccessFlags() != output_field->GetAccessFlags()) {
      *error_msg = StringPrintf(
          "Mismatched field access flags for class data at offset %x: %u vs %u.",
          orig_offset,
          orig_field->GetAccessFlags(),
          output_field->GetAccessFlags());
      return false;
    }
  }
  return true;
}

bool VerifyMethods(dex_ir::MethodItemVector* orig,
                   dex_ir::MethodItemVector* output,
                   uint32_t orig_offset,
                   std::string* error_msg) {
  if (orig->size() != output->size()) {
    *error_msg = StringPrintf("Mismatched methods size for class data at offset %x: %zu vs %zu.",
                              orig_offset,
                              orig->size(),
                              output->size());
    return false;
  }
  for (size_t i = 0; i < orig->size(); ++i) {
    dex_ir::MethodItem* orig_method = (*orig)[i].get();
    dex_ir::MethodItem* output_method = (*output)[i].get();
    if (orig_method->GetMethodId()->GetIndex() != output_method->GetMethodId()->GetIndex()) {
      *error_msg = StringPrintf("Mismatched method index for class data at offset %x: %u vs %u.",
                                orig_offset,
                                orig_method->GetMethodId()->GetIndex(),
                                output_method->GetMethodId()->GetIndex());
      return false;
    }
    if (orig_method->GetAccessFlags() != output_method->GetAccessFlags()) {
      *error_msg = StringPrintf(
          "Mismatched method access flags for class data at offset %x: %u vs %u.",
          orig_offset,
          orig_method->GetAccessFlags(),
          output_method->GetAccessFlags());
      return false;
    }
    if (!VerifyCode(orig_method->GetCodeItem(), output_method->GetCodeItem(), error_msg)) {
      return false;
    }
  }
  return true;
}

bool VerifyCode(dex_ir::CodeItem* orig, dex_ir::CodeItem* output, std::string* error_msg) {
  if (orig == nullptr || output == nullptr) {
    if (orig != output) {
      *error_msg = "Found unexpected empty code item.";
      return false;
    }
    return true;
  }
  if (orig->RegistersSize() != output->RegistersSize()) {
    *error_msg = StringPrintf("Mismatched registers size for code item at offset %x: %u vs %u.",
                              orig->GetOffset(),
                              orig->RegistersSize(),
                              output->RegistersSize());
    return false;
  }
  if (orig->InsSize() != output->InsSize()) {
    *error_msg = StringPrintf("Mismatched ins size for code item at offset %x: %u vs %u.",
                              orig->GetOffset(),
                              orig->InsSize(),
                              output->InsSize());
    return false;
  }
  if (orig->OutsSize() != output->OutsSize()) {
    *error_msg = StringPrintf("Mismatched outs size for code item at offset %x: %u vs %u.",
                              orig->GetOffset(),
                              orig->OutsSize(),
                              output->OutsSize());
    return false;
  }
  if (orig->TriesSize() != output->TriesSize()) {
    *error_msg = StringPrintf("Mismatched tries size for code item at offset %x: %u vs %u.",
                              orig->GetOffset(),
                              orig->TriesSize(),
                              output->TriesSize());
    return false;
  }
  if (!VerifyDebugInfo(orig->DebugInfo(), output->DebugInfo(), error_msg)) {
    return false;
  }
  if (orig->InsnsSize() != output->InsnsSize()) {
    *error_msg = StringPrintf("Mismatched insns size for code item at offset %x: %u vs %u.",
                              orig->GetOffset(),
                              orig->InsnsSize(),
                              output->InsnsSize());
    return false;
  }
  if (memcmp(orig->Insns(), output->Insns(), orig->InsnsSize()) != 0) {
    *error_msg = StringPrintf("Mismatched insns for code item at offset %x.",
                              orig->GetOffset());
    return false;
  }
  if (!VerifyTries(orig->Tries(), output->Tries(), orig->GetOffset(), error_msg)) {
    return false;
  }
  return VerifyHandlers(orig->Handlers(), output->Handlers(), orig->GetOffset(), error_msg);
}

bool VerifyDebugInfo(dex_ir::DebugInfoItem* orig,
                     dex_ir::DebugInfoItem* output,
                     std::string* error_msg) {
  if (orig == nullptr || output == nullptr) {
    if (orig != output) {
      *error_msg = "Found unexpected empty debug info.";
      return false;
    }
    return true;
  }
  // TODO: Test for debug equivalence rather than byte array equality.
  uint32_t orig_size = orig->GetDebugInfoSize();
  uint32_t output_size = output->GetDebugInfoSize();
  if (orig_size != output_size) {
    *error_msg = "DebugInfoSize disagreed.";
    return false;
  }
  uint8_t* orig_data = orig->GetDebugInfo();
  uint8_t* output_data = output->GetDebugInfo();
  if ((orig_data == nullptr && output_data != nullptr) ||
      (orig_data != nullptr && output_data == nullptr)) {
    *error_msg = "DebugInfo null/non-null mismatch.";
    return false;
  }
  if (memcmp(orig_data, output_data, orig_size) != 0) {
    *error_msg = "DebugInfo bytes mismatch.";
    return false;
  }
  return true;
}

bool VerifyTries(dex_ir::TryItemVector* orig,
                 dex_ir::TryItemVector* output,
                 uint32_t orig_offset,
                 std::string* error_msg) {
  if (orig == nullptr || output == nullptr) {
    if (orig != output) {
      *error_msg = "Found unexpected empty try items.";
      return false;
    }
    return true;
  }
  if (orig->size() != output->size()) {
    *error_msg = StringPrintf("Mismatched tries size for code item at offset %x: %zu vs %zu.",
                              orig_offset,
                              orig->size(),
                              output->size());
    return false;
  }
  for (size_t i = 0; i < orig->size(); ++i) {
    const dex_ir::TryItem* orig_try = (*orig)[i].get();
    const dex_ir::TryItem* output_try = (*output)[i].get();
    if (orig_try->StartAddr() != output_try->StartAddr()) {
      *error_msg = StringPrintf(
          "Mismatched try item start addr for code item at offset %x: %u vs %u.",
          orig_offset,
          orig_try->StartAddr(),
          output_try->StartAddr());
      return false;
    }
    if (orig_try->InsnCount() != output_try->InsnCount()) {
      *error_msg = StringPrintf(
          "Mismatched try item insn count for code item at offset %x: %u vs %u.",
          orig_offset,
          orig_try->InsnCount(),
                                output_try->InsnCount());
      return false;
    }
    if (!VerifyHandler(orig_try->GetHandlers(),
                       output_try->GetHandlers(),
                       orig_offset,
                       error_msg)) {
      return false;
    }
  }
  return true;
}

bool VerifyHandlers(dex_ir::CatchHandlerVector* orig,
                    dex_ir::CatchHandlerVector* output,
                    uint32_t orig_offset,
                    std::string* error_msg) {
  if (orig == nullptr || output == nullptr) {
    if (orig != output) {
      *error_msg = "Found unexpected empty catch handlers.";
      return false;
    }
    return true;
  }
  if (orig->size() != output->size()) {
    *error_msg = StringPrintf(
        "Mismatched catch handlers size for code item at offset %x: %zu vs %zu.",
        orig_offset,
        orig->size(),
        output->size());
    return false;
  }
  for (size_t i = 0; i < orig->size(); ++i) {
    if (!VerifyHandler((*orig)[i].get(), (*output)[i].get(), orig_offset, error_msg)) {
      return false;
    }
  }
  return true;
}

bool VerifyHandler(const dex_ir::CatchHandler* orig,
                   const dex_ir::CatchHandler* output,
                   uint32_t orig_offset,
                   std::string* error_msg) {
  dex_ir::TypeAddrPairVector* orig_handlers = orig->GetHandlers();
  dex_ir::TypeAddrPairVector* output_handlers = output->GetHandlers();
  if (orig_handlers->size() != output_handlers->size()) {
    *error_msg = StringPrintf(
        "Mismatched number of catch handlers for code item at offset %x: %zu vs %zu.",
        orig_offset,
        orig_handlers->size(),
        output_handlers->size());
    return false;
  }
  for (size_t i = 0; i < orig_handlers->size(); ++i) {
    const dex_ir::TypeAddrPair* orig_handler = (*orig_handlers)[i].get();
    const dex_ir::TypeAddrPair* output_handler = (*output_handlers)[i].get();
    if (orig_handler->GetTypeId() == nullptr || output_handler->GetTypeId() == nullptr) {
      if (orig_handler->GetTypeId() != output_handler->GetTypeId()) {
        *error_msg = StringPrintf(
            "Found unexpected catch all catch handler for code item at offset %x.",
            orig_offset);
        return false;
      }
    } else if (orig_handler->GetTypeId()->GetIndex() != output_handler->GetTypeId()->GetIndex()) {
      *error_msg = StringPrintf(
          "Mismatched catch handler type for code item at offset %x: %u vs %u.",
          orig_offset,
          orig_handler->GetTypeId()->GetIndex(),
          output_handler->GetTypeId()->GetIndex());
      return false;
    }
    if (orig_handler->GetAddress() != output_handler->GetAddress()) {
      *error_msg = StringPrintf(
          "Mismatched catch handler address for code item at offset %x: %u vs %u.",
          orig_offset,
          orig_handler->GetAddress(),
          output_handler->GetAddress());
      return false;
    }
  }
  return true;
}

}  // namespace art
