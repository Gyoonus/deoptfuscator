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
 * Implementation file of the dexlayout utility.
 *
 * This is a tool to read dex files into an internal representation,
 * reorganize the representation, and emit dex files with a better
 * file layout.
 */

#include "dexlayout.h"

#include <inttypes.h>
#include <stdio.h>
#include <sys/mman.h>  // For the PROT_* and MAP_* constants.

#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "android-base/stringprintf.h"

#include "base/logging.h"  // For VLOG_IS_ON.
#include "base/os.h"
#include "base/utils.h"
#include "dex/art_dex_file_loader.h"
#include "dex/descriptors_names.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_layout.h"
#include "dex/dex_file_loader.h"
#include "dex/dex_file_types.h"
#include "dex/dex_file_verifier.h"
#include "dex/dex_instruction-inl.h"
#include "dex_ir_builder.h"
#include "dex_verify.h"
#include "dex_visualize.h"
#include "dex_writer.h"
#include "jit/profile_compilation_info.h"
#include "mem_map.h"

namespace art {

using android::base::StringPrintf;

/*
 * Flags for use with createAccessFlagStr().
 */
enum AccessFor {
  kAccessForClass = 0, kAccessForMethod = 1, kAccessForField = 2, kAccessForMAX
};
const int kNumFlags = 18;

/*
 * Gets 2 little-endian bytes.
 */
static inline uint16_t Get2LE(unsigned char const* src) {
  return src[0] | (src[1] << 8);
}

/*
 * Converts a type descriptor to human-readable "dotted" form.  For
 * example, "Ljava/lang/String;" becomes "java.lang.String", and
 * "[I" becomes "int[]".  Also converts '$' to '.', which means this
 * form can't be converted back to a descriptor.
 */
static std::string DescriptorToDotWrapper(const char* descriptor) {
  std::string result = DescriptorToDot(descriptor);
  size_t found = result.find('$');
  while (found != std::string::npos) {
    result[found] = '.';
    found = result.find('$', found);
  }
  return result;
}

/*
 * Converts the class name portion of a type descriptor to human-readable
 * "dotted" form. For example, "Ljava/lang/String;" becomes "String".
 */
static std::string DescriptorClassToDot(const char* str) {
  std::string descriptor(str);
  // Reduce to just the class name prefix.
  size_t last_slash = descriptor.rfind('/');
  if (last_slash == std::string::npos) {
    last_slash = 0;
  }
  // Start past the '/' or 'L'.
  last_slash++;

  // Copy class name over, trimming trailing ';'.
  size_t size = descriptor.size() - 1 - last_slash;
  std::string result(descriptor.substr(last_slash, size));

  // Replace '$' with '.'.
  size_t dollar_sign = result.find('$');
  while (dollar_sign != std::string::npos) {
    result[dollar_sign] = '.';
    dollar_sign = result.find('$', dollar_sign);
  }

  return result;
}

/*
 * Returns string representing the boolean value.
 */
static const char* StrBool(bool val) {
  return val ? "true" : "false";
}

/*
 * Returns a quoted string representing the boolean value.
 */
static const char* QuotedBool(bool val) {
  return val ? "\"true\"" : "\"false\"";
}

/*
 * Returns a quoted string representing the access flags.
 */
static const char* QuotedVisibility(uint32_t access_flags) {
  if (access_flags & kAccPublic) {
    return "\"public\"";
  } else if (access_flags & kAccProtected) {
    return "\"protected\"";
  } else if (access_flags & kAccPrivate) {
    return "\"private\"";
  } else {
    return "\"package\"";
  }
}

/*
 * Counts the number of '1' bits in a word.
 */
static int CountOnes(uint32_t val) {
  val = val - ((val >> 1) & 0x55555555);
  val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
  return (((val + (val >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/*
 * Creates a new string with human-readable access flags.
 *
 * In the base language the access_flags fields are type uint16_t; in Dalvik they're uint32_t.
 */
static char* CreateAccessFlagStr(uint32_t flags, AccessFor for_what) {
  static const char* kAccessStrings[kAccessForMAX][kNumFlags] = {
    {
      "PUBLIC",                /* 0x00001 */
      "PRIVATE",               /* 0x00002 */
      "PROTECTED",             /* 0x00004 */
      "STATIC",                /* 0x00008 */
      "FINAL",                 /* 0x00010 */
      "?",                     /* 0x00020 */
      "?",                     /* 0x00040 */
      "?",                     /* 0x00080 */
      "?",                     /* 0x00100 */
      "INTERFACE",             /* 0x00200 */
      "ABSTRACT",              /* 0x00400 */
      "?",                     /* 0x00800 */
      "SYNTHETIC",             /* 0x01000 */
      "ANNOTATION",            /* 0x02000 */
      "ENUM",                  /* 0x04000 */
      "?",                     /* 0x08000 */
      "VERIFIED",              /* 0x10000 */
      "OPTIMIZED",             /* 0x20000 */
    }, {
      "PUBLIC",                /* 0x00001 */
      "PRIVATE",               /* 0x00002 */
      "PROTECTED",             /* 0x00004 */
      "STATIC",                /* 0x00008 */
      "FINAL",                 /* 0x00010 */
      "SYNCHRONIZED",          /* 0x00020 */
      "BRIDGE",                /* 0x00040 */
      "VARARGS",               /* 0x00080 */
      "NATIVE",                /* 0x00100 */
      "?",                     /* 0x00200 */
      "ABSTRACT",              /* 0x00400 */
      "STRICT",                /* 0x00800 */
      "SYNTHETIC",             /* 0x01000 */
      "?",                     /* 0x02000 */
      "?",                     /* 0x04000 */
      "MIRANDA",               /* 0x08000 */
      "CONSTRUCTOR",           /* 0x10000 */
      "DECLARED_SYNCHRONIZED", /* 0x20000 */
    }, {
      "PUBLIC",                /* 0x00001 */
      "PRIVATE",               /* 0x00002 */
      "PROTECTED",             /* 0x00004 */
      "STATIC",                /* 0x00008 */
      "FINAL",                 /* 0x00010 */
      "?",                     /* 0x00020 */
      "VOLATILE",              /* 0x00040 */
      "TRANSIENT",             /* 0x00080 */
      "?",                     /* 0x00100 */
      "?",                     /* 0x00200 */
      "?",                     /* 0x00400 */
      "?",                     /* 0x00800 */
      "SYNTHETIC",             /* 0x01000 */
      "?",                     /* 0x02000 */
      "ENUM",                  /* 0x04000 */
      "?",                     /* 0x08000 */
      "?",                     /* 0x10000 */
      "?",                     /* 0x20000 */
    },
  };

  // Allocate enough storage to hold the expected number of strings,
  // plus a space between each.  We over-allocate, using the longest
  // string above as the base metric.
  const int kLongest = 21;  // The strlen of longest string above.
  const int count = CountOnes(flags);
  char* str;
  char* cp;
  cp = str = reinterpret_cast<char*>(malloc(count * (kLongest + 1) + 1));

  for (int i = 0; i < kNumFlags; i++) {
    if (flags & 0x01) {
      const char* accessStr = kAccessStrings[for_what][i];
      const int len = strlen(accessStr);
      if (cp != str) {
        *cp++ = ' ';
      }
      memcpy(cp, accessStr, len);
      cp += len;
    }
    flags >>= 1;
  }  // for

  *cp = '\0';
  return str;
}

static std::string GetSignatureForProtoId(const dex_ir::ProtoId* proto) {
  if (proto == nullptr) {
    return "<no signature>";
  }

  std::string result("(");
  const dex_ir::TypeList* type_list = proto->Parameters();
  if (type_list != nullptr) {
    for (const dex_ir::TypeId* type_id : *type_list->GetTypeList()) {
      result += type_id->GetStringId()->Data();
    }
  }
  result += ")";
  result += proto->ReturnType()->GetStringId()->Data();
  return result;
}

/*
 * Copies character data from "data" to "out", converting non-ASCII values
 * to fprintf format chars or an ASCII filler ('.' or '?').
 *
 * The output buffer must be able to hold (2*len)+1 bytes.  The result is
 * NULL-terminated.
 */
static void Asciify(char* out, const unsigned char* data, size_t len) {
  while (len--) {
    if (*data < 0x20) {
      // Could do more here, but we don't need them yet.
      switch (*data) {
        case '\0':
          *out++ = '\\';
          *out++ = '0';
          break;
        case '\n':
          *out++ = '\\';
          *out++ = 'n';
          break;
        default:
          *out++ = '.';
          break;
      }  // switch
    } else if (*data >= 0x80) {
      *out++ = '?';
    } else {
      *out++ = *data;
    }
    data++;
  }  // while
  *out = '\0';
}

/*
 * Dumps a string value with some escape characters.
 */
static void DumpEscapedString(const char* p, FILE* out_file) {
  fputs("\"", out_file);
  for (; *p; p++) {
    switch (*p) {
      case '\\':
        fputs("\\\\", out_file);
        break;
      case '\"':
        fputs("\\\"", out_file);
        break;
      case '\t':
        fputs("\\t", out_file);
        break;
      case '\n':
        fputs("\\n", out_file);
        break;
      case '\r':
        fputs("\\r", out_file);
        break;
      default:
        putc(*p, out_file);
    }  // switch
  }  // for
  fputs("\"", out_file);
}

/*
 * Dumps a string as an XML attribute value.
 */
static void DumpXmlAttribute(const char* p, FILE* out_file) {
  for (; *p; p++) {
    switch (*p) {
      case '&':
        fputs("&amp;", out_file);
        break;
      case '<':
        fputs("&lt;", out_file);
        break;
      case '>':
        fputs("&gt;", out_file);
        break;
      case '"':
        fputs("&quot;", out_file);
        break;
      case '\t':
        fputs("&#x9;", out_file);
        break;
      case '\n':
        fputs("&#xA;", out_file);
        break;
      case '\r':
        fputs("&#xD;", out_file);
        break;
      default:
        putc(*p, out_file);
    }  // switch
  }  // for
}

/*
 * Helper for dumpInstruction(), which builds the string
 * representation for the index in the given instruction.
 * Returns a pointer to a buffer of sufficient size.
 */
static std::unique_ptr<char[]> IndexString(dex_ir::Header* header,
                                           const Instruction* dec_insn,
                                           size_t buf_size) {
  std::unique_ptr<char[]> buf(new char[buf_size]);
  // Determine index and width of the string.
  uint32_t index = 0;
  uint32_t secondary_index = dex::kDexNoIndex;
  uint32_t width = 4;
  switch (Instruction::FormatOf(dec_insn->Opcode())) {
    // SOME NOT SUPPORTED:
    // case Instruction::k20bc:
    case Instruction::k21c:
    case Instruction::k35c:
    // case Instruction::k35ms:
    case Instruction::k3rc:
    // case Instruction::k3rms:
    // case Instruction::k35mi:
    // case Instruction::k3rmi:
      index = dec_insn->VRegB();
      width = 4;
      break;
    case Instruction::k31c:
      index = dec_insn->VRegB();
      width = 8;
      break;
    case Instruction::k22c:
    // case Instruction::k22cs:
      index = dec_insn->VRegC();
      width = 4;
      break;
    case Instruction::k45cc:
    case Instruction::k4rcc:
      index = dec_insn->VRegB();
      secondary_index = dec_insn->VRegH();
      width = 4;
      break;
    default:
      break;
  }  // switch

  // Determine index type.
  size_t outSize = 0;
  switch (Instruction::IndexTypeOf(dec_insn->Opcode())) {
    case Instruction::kIndexUnknown:
      // This function should never get called for this type, but do
      // something sensible here, just to help with debugging.
      outSize = snprintf(buf.get(), buf_size, "<unknown-index>");
      break;
    case Instruction::kIndexNone:
      // This function should never get called for this type, but do
      // something sensible here, just to help with debugging.
      outSize = snprintf(buf.get(), buf_size, "<no-index>");
      break;
    case Instruction::kIndexTypeRef:
      if (index < header->GetCollections().TypeIdsSize()) {
        const char* tp = header->GetCollections().GetTypeId(index)->GetStringId()->Data();
        outSize = snprintf(buf.get(), buf_size, "%s // type@%0*x", tp, width, index);
      } else {
        outSize = snprintf(buf.get(), buf_size, "<type?> // type@%0*x", width, index);
      }
      break;
    case Instruction::kIndexStringRef:
      if (index < header->GetCollections().StringIdsSize()) {
        const char* st = header->GetCollections().GetStringId(index)->Data();
        outSize = snprintf(buf.get(), buf_size, "\"%s\" // string@%0*x", st, width, index);
      } else {
        outSize = snprintf(buf.get(), buf_size, "<string?> // string@%0*x", width, index);
      }
      break;
    case Instruction::kIndexMethodRef:
      if (index < header->GetCollections().MethodIdsSize()) {
        dex_ir::MethodId* method_id = header->GetCollections().GetMethodId(index);
        const char* name = method_id->Name()->Data();
        std::string type_descriptor = GetSignatureForProtoId(method_id->Proto());
        const char* back_descriptor = method_id->Class()->GetStringId()->Data();
        outSize = snprintf(buf.get(), buf_size, "%s.%s:%s // method@%0*x",
                           back_descriptor, name, type_descriptor.c_str(), width, index);
      } else {
        outSize = snprintf(buf.get(), buf_size, "<method?> // method@%0*x", width, index);
      }
      break;
    case Instruction::kIndexFieldRef:
      if (index < header->GetCollections().FieldIdsSize()) {
        dex_ir::FieldId* field_id = header->GetCollections().GetFieldId(index);
        const char* name = field_id->Name()->Data();
        const char* type_descriptor = field_id->Type()->GetStringId()->Data();
        const char* back_descriptor = field_id->Class()->GetStringId()->Data();
        outSize = snprintf(buf.get(), buf_size, "%s.%s:%s // field@%0*x",
                           back_descriptor, name, type_descriptor, width, index);
      } else {
        outSize = snprintf(buf.get(), buf_size, "<field?> // field@%0*x", width, index);
      }
      break;
    case Instruction::kIndexVtableOffset:
      outSize = snprintf(buf.get(), buf_size, "[%0*x] // vtable #%0*x",
                         width, index, width, index);
      break;
    case Instruction::kIndexFieldOffset:
      outSize = snprintf(buf.get(), buf_size, "[obj+%0*x]", width, index);
      break;
    case Instruction::kIndexMethodAndProtoRef: {
      std::string method("<method?>");
      std::string proto("<proto?>");
      if (index < header->GetCollections().MethodIdsSize()) {
        dex_ir::MethodId* method_id = header->GetCollections().GetMethodId(index);
        const char* name = method_id->Name()->Data();
        std::string type_descriptor = GetSignatureForProtoId(method_id->Proto());
        const char* back_descriptor = method_id->Class()->GetStringId()->Data();
        method = StringPrintf("%s.%s:%s", back_descriptor, name, type_descriptor.c_str());
      }
      if (secondary_index < header->GetCollections().ProtoIdsSize()) {
        dex_ir::ProtoId* proto_id = header->GetCollections().GetProtoId(secondary_index);
        proto = GetSignatureForProtoId(proto_id);
      }
      outSize = snprintf(buf.get(), buf_size, "%s, %s // method@%0*x, proto@%0*x",
                         method.c_str(), proto.c_str(), width, index, width, secondary_index);
    }
    break;
    // SOME NOT SUPPORTED:
    // case Instruction::kIndexVaries:
    // case Instruction::kIndexInlineMethod:
    default:
      outSize = snprintf(buf.get(), buf_size, "<?>");
      break;
  }  // switch

  // Determine success of string construction.
  if (outSize >= buf_size) {
    // The buffer wasn't big enough; retry with computed size. Note: snprintf()
    // doesn't count/ the '\0' as part of its returned size, so we add explicit
    // space for it here.
    return IndexString(header, dec_insn, outSize + 1);
  }
  return buf;
}

/*
 * Dumps encoded annotation.
 */
void DexLayout::DumpEncodedAnnotation(dex_ir::EncodedAnnotation* annotation) {
  fputs(annotation->GetType()->GetStringId()->Data(), out_file_);
  // Display all name=value pairs.
  for (auto& subannotation : *annotation->GetAnnotationElements()) {
    fputc(' ', out_file_);
    fputs(subannotation->GetName()->Data(), out_file_);
    fputc('=', out_file_);
    DumpEncodedValue(subannotation->GetValue());
  }
}
/*
 * Dumps encoded value.
 */
void DexLayout::DumpEncodedValue(const dex_ir::EncodedValue* data) {
  switch (data->Type()) {
    case DexFile::kDexAnnotationByte:
      fprintf(out_file_, "%" PRId8, data->GetByte());
      break;
    case DexFile::kDexAnnotationShort:
      fprintf(out_file_, "%" PRId16, data->GetShort());
      break;
    case DexFile::kDexAnnotationChar:
      fprintf(out_file_, "%" PRIu16, data->GetChar());
      break;
    case DexFile::kDexAnnotationInt:
      fprintf(out_file_, "%" PRId32, data->GetInt());
      break;
    case DexFile::kDexAnnotationLong:
      fprintf(out_file_, "%" PRId64, data->GetLong());
      break;
    case DexFile::kDexAnnotationFloat: {
      fprintf(out_file_, "%g", data->GetFloat());
      break;
    }
    case DexFile::kDexAnnotationDouble: {
      fprintf(out_file_, "%g", data->GetDouble());
      break;
    }
    case DexFile::kDexAnnotationString: {
      dex_ir::StringId* string_id = data->GetStringId();
      if (options_.output_format_ == kOutputPlain) {
        DumpEscapedString(string_id->Data(), out_file_);
      } else {
        DumpXmlAttribute(string_id->Data(), out_file_);
      }
      break;
    }
    case DexFile::kDexAnnotationType: {
      dex_ir::TypeId* type_id = data->GetTypeId();
      fputs(type_id->GetStringId()->Data(), out_file_);
      break;
    }
    case DexFile::kDexAnnotationField:
    case DexFile::kDexAnnotationEnum: {
      dex_ir::FieldId* field_id = data->GetFieldId();
      fputs(field_id->Name()->Data(), out_file_);
      break;
    }
    case DexFile::kDexAnnotationMethod: {
      dex_ir::MethodId* method_id = data->GetMethodId();
      fputs(method_id->Name()->Data(), out_file_);
      break;
    }
    case DexFile::kDexAnnotationArray: {
      fputc('{', out_file_);
      // Display all elements.
      for (auto& value : *data->GetEncodedArray()->GetEncodedValues()) {
        fputc(' ', out_file_);
        DumpEncodedValue(value.get());
      }
      fputs(" }", out_file_);
      break;
    }
    case DexFile::kDexAnnotationAnnotation: {
      DumpEncodedAnnotation(data->GetEncodedAnnotation());
      break;
    }
    case DexFile::kDexAnnotationNull:
      fputs("null", out_file_);
      break;
    case DexFile::kDexAnnotationBoolean:
      fputs(StrBool(data->GetBoolean()), out_file_);
      break;
    default:
      fputs("????", out_file_);
      break;
  }  // switch
}

/*
 * Dumps the file header.
 */
void DexLayout::DumpFileHeader() {
  char sanitized[8 * 2 + 1];
  dex_ir::Collections& collections = header_->GetCollections();
  fprintf(out_file_, "DEX file header:\n");
  Asciify(sanitized, header_->Magic(), 8);
  fprintf(out_file_, "magic               : '%s'\n", sanitized);
  fprintf(out_file_, "checksum            : %08x\n", header_->Checksum());
  fprintf(out_file_, "signature           : %02x%02x...%02x%02x\n",
          header_->Signature()[0], header_->Signature()[1],
          header_->Signature()[DexFile::kSha1DigestSize - 2],
          header_->Signature()[DexFile::kSha1DigestSize - 1]);
  fprintf(out_file_, "file_size           : %d\n", header_->FileSize());
  fprintf(out_file_, "header_size         : %d\n", header_->HeaderSize());
  fprintf(out_file_, "link_size           : %d\n", header_->LinkSize());
  fprintf(out_file_, "link_off            : %d (0x%06x)\n",
          header_->LinkOffset(), header_->LinkOffset());
  fprintf(out_file_, "string_ids_size     : %d\n", collections.StringIdsSize());
  fprintf(out_file_, "string_ids_off      : %d (0x%06x)\n",
          collections.StringIdsOffset(), collections.StringIdsOffset());
  fprintf(out_file_, "type_ids_size       : %d\n", collections.TypeIdsSize());
  fprintf(out_file_, "type_ids_off        : %d (0x%06x)\n",
          collections.TypeIdsOffset(), collections.TypeIdsOffset());
  fprintf(out_file_, "proto_ids_size      : %d\n", collections.ProtoIdsSize());
  fprintf(out_file_, "proto_ids_off       : %d (0x%06x)\n",
          collections.ProtoIdsOffset(), collections.ProtoIdsOffset());
  fprintf(out_file_, "field_ids_size      : %d\n", collections.FieldIdsSize());
  fprintf(out_file_, "field_ids_off       : %d (0x%06x)\n",
          collections.FieldIdsOffset(), collections.FieldIdsOffset());
  fprintf(out_file_, "method_ids_size     : %d\n", collections.MethodIdsSize());
  fprintf(out_file_, "method_ids_off      : %d (0x%06x)\n",
          collections.MethodIdsOffset(), collections.MethodIdsOffset());
  fprintf(out_file_, "class_defs_size     : %d\n", collections.ClassDefsSize());
  fprintf(out_file_, "class_defs_off      : %d (0x%06x)\n",
          collections.ClassDefsOffset(), collections.ClassDefsOffset());
  fprintf(out_file_, "data_size           : %d\n", header_->DataSize());
  fprintf(out_file_, "data_off            : %d (0x%06x)\n\n",
          header_->DataOffset(), header_->DataOffset());
}

/*
 * Dumps a class_def_item.
 */
void DexLayout::DumpClassDef(int idx) {
  // General class information.
  dex_ir::ClassDef* class_def = header_->GetCollections().GetClassDef(idx);
  fprintf(out_file_, "Class #%d header:\n", idx);
  fprintf(out_file_, "class_idx           : %d\n", class_def->ClassType()->GetIndex());
  fprintf(out_file_, "access_flags        : %d (0x%04x)\n",
          class_def->GetAccessFlags(), class_def->GetAccessFlags());
  uint32_t superclass_idx =  class_def->Superclass() == nullptr ?
      DexFile::kDexNoIndex16 : class_def->Superclass()->GetIndex();
  fprintf(out_file_, "superclass_idx      : %d\n", superclass_idx);
  fprintf(out_file_, "interfaces_off      : %d (0x%06x)\n",
          class_def->InterfacesOffset(), class_def->InterfacesOffset());
  uint32_t source_file_offset = 0xffffffffU;
  if (class_def->SourceFile() != nullptr) {
    source_file_offset = class_def->SourceFile()->GetIndex();
  }
  fprintf(out_file_, "source_file_idx     : %d\n", source_file_offset);
  uint32_t annotations_offset = 0;
  if (class_def->Annotations() != nullptr) {
    annotations_offset = class_def->Annotations()->GetOffset();
  }
  fprintf(out_file_, "annotations_off     : %d (0x%06x)\n",
          annotations_offset, annotations_offset);
  if (class_def->GetClassData() == nullptr) {
    fprintf(out_file_, "class_data_off      : %d (0x%06x)\n", 0, 0);
  } else {
    fprintf(out_file_, "class_data_off      : %d (0x%06x)\n",
            class_def->GetClassData()->GetOffset(), class_def->GetClassData()->GetOffset());
  }

  // Fields and methods.
  dex_ir::ClassData* class_data = class_def->GetClassData();
  if (class_data != nullptr && class_data->StaticFields() != nullptr) {
    fprintf(out_file_, "static_fields_size  : %zu\n", class_data->StaticFields()->size());
  } else {
    fprintf(out_file_, "static_fields_size  : 0\n");
  }
  if (class_data != nullptr && class_data->InstanceFields() != nullptr) {
    fprintf(out_file_, "instance_fields_size: %zu\n", class_data->InstanceFields()->size());
  } else {
    fprintf(out_file_, "instance_fields_size: 0\n");
  }
  if (class_data != nullptr && class_data->DirectMethods() != nullptr) {
    fprintf(out_file_, "direct_methods_size : %zu\n", class_data->DirectMethods()->size());
  } else {
    fprintf(out_file_, "direct_methods_size : 0\n");
  }
  if (class_data != nullptr && class_data->VirtualMethods() != nullptr) {
    fprintf(out_file_, "virtual_methods_size: %zu\n", class_data->VirtualMethods()->size());
  } else {
    fprintf(out_file_, "virtual_methods_size: 0\n");
  }
  fprintf(out_file_, "\n");
}

/**
 * Dumps an annotation set item.
 */
void DexLayout::DumpAnnotationSetItem(dex_ir::AnnotationSetItem* set_item) {
  if (set_item == nullptr || set_item->GetItems()->size() == 0) {
    fputs("  empty-annotation-set\n", out_file_);
    return;
  }
  for (dex_ir::AnnotationItem* annotation : *set_item->GetItems()) {
    if (annotation == nullptr) {
      continue;
    }
    fputs("  ", out_file_);
    switch (annotation->GetVisibility()) {
      case DexFile::kDexVisibilityBuild:   fputs("VISIBILITY_BUILD ",   out_file_); break;
      case DexFile::kDexVisibilityRuntime: fputs("VISIBILITY_RUNTIME ", out_file_); break;
      case DexFile::kDexVisibilitySystem:  fputs("VISIBILITY_SYSTEM ",  out_file_); break;
      default:                             fputs("VISIBILITY_UNKNOWN ", out_file_); break;
    }  // switch
    DumpEncodedAnnotation(annotation->GetAnnotation());
    fputc('\n', out_file_);
  }
}

/*
 * Dumps class annotations.
 */
void DexLayout::DumpClassAnnotations(int idx) {
  dex_ir::ClassDef* class_def = header_->GetCollections().GetClassDef(idx);
  dex_ir::AnnotationsDirectoryItem* annotations_directory = class_def->Annotations();
  if (annotations_directory == nullptr) {
    return;  // none
  }

  fprintf(out_file_, "Class #%d annotations:\n", idx);

  dex_ir::AnnotationSetItem* class_set_item = annotations_directory->GetClassAnnotation();
  dex_ir::FieldAnnotationVector* fields = annotations_directory->GetFieldAnnotations();
  dex_ir::MethodAnnotationVector* methods = annotations_directory->GetMethodAnnotations();
  dex_ir::ParameterAnnotationVector* parameters = annotations_directory->GetParameterAnnotations();

  // Annotations on the class itself.
  if (class_set_item != nullptr) {
    fprintf(out_file_, "Annotations on class\n");
    DumpAnnotationSetItem(class_set_item);
  }

  // Annotations on fields.
  if (fields != nullptr) {
    for (auto& field : *fields) {
      const dex_ir::FieldId* field_id = field->GetFieldId();
      const uint32_t field_idx = field_id->GetIndex();
      const char* field_name = field_id->Name()->Data();
      fprintf(out_file_, "Annotations on field #%u '%s'\n", field_idx, field_name);
      DumpAnnotationSetItem(field->GetAnnotationSetItem());
    }
  }

  // Annotations on methods.
  if (methods != nullptr) {
    for (auto& method : *methods) {
      const dex_ir::MethodId* method_id = method->GetMethodId();
      const uint32_t method_idx = method_id->GetIndex();
      const char* method_name = method_id->Name()->Data();
      fprintf(out_file_, "Annotations on method #%u '%s'\n", method_idx, method_name);
      DumpAnnotationSetItem(method->GetAnnotationSetItem());
    }
  }

  // Annotations on method parameters.
  if (parameters != nullptr) {
    for (auto& parameter : *parameters) {
      const dex_ir::MethodId* method_id = parameter->GetMethodId();
      const uint32_t method_idx = method_id->GetIndex();
      const char* method_name = method_id->Name()->Data();
      fprintf(out_file_, "Annotations on method #%u '%s' parameters\n", method_idx, method_name);
      uint32_t j = 0;
      for (dex_ir::AnnotationSetItem* annotation : *parameter->GetAnnotations()->GetItems()) {
        fprintf(out_file_, "#%u\n", j);
        DumpAnnotationSetItem(annotation);
        ++j;
      }
    }
  }

  fputc('\n', out_file_);
}

/*
 * Dumps an interface that a class declares to implement.
 */
void DexLayout::DumpInterface(const dex_ir::TypeId* type_item, int i) {
  const char* interface_name = type_item->GetStringId()->Data();
  if (options_.output_format_ == kOutputPlain) {
    fprintf(out_file_, "    #%d              : '%s'\n", i, interface_name);
  } else {
    std::string dot(DescriptorToDotWrapper(interface_name));
    fprintf(out_file_, "<implements name=\"%s\">\n</implements>\n", dot.c_str());
  }
}

/*
 * Dumps the catches table associated with the code.
 */
void DexLayout::DumpCatches(const dex_ir::CodeItem* code) {
  const uint16_t tries_size = code->TriesSize();

  // No catch table.
  if (tries_size == 0) {
    fprintf(out_file_, "      catches       : (none)\n");
    return;
  }

  // Dump all table entries.
  fprintf(out_file_, "      catches       : %d\n", tries_size);
  std::vector<std::unique_ptr<const dex_ir::TryItem>>* tries = code->Tries();
  for (uint32_t i = 0; i < tries_size; i++) {
    const dex_ir::TryItem* try_item = (*tries)[i].get();
    const uint32_t start = try_item->StartAddr();
    const uint32_t end = start + try_item->InsnCount();
    fprintf(out_file_, "        0x%04x - 0x%04x\n", start, end);
    for (auto& handler : *try_item->GetHandlers()->GetHandlers()) {
      const dex_ir::TypeId* type_id = handler->GetTypeId();
      const char* descriptor = (type_id == nullptr) ? "<any>" : type_id->GetStringId()->Data();
      fprintf(out_file_, "          %s -> 0x%04x\n", descriptor, handler->GetAddress());
    }  // for
  }  // for
}

/*
 * Dumps a single instruction.
 */
void DexLayout::DumpInstruction(const dex_ir::CodeItem* code,
                                uint32_t code_offset,
                                uint32_t insn_idx,
                                uint32_t insn_width,
                                const Instruction* dec_insn) {
  // Address of instruction (expressed as byte offset).
  fprintf(out_file_, "%06x:", code_offset + 0x10 + insn_idx * 2);

  // Dump (part of) raw bytes.
  const uint16_t* insns = code->Insns();
  for (uint32_t i = 0; i < 8; i++) {
    if (i < insn_width) {
      if (i == 7) {
        fprintf(out_file_, " ... ");
      } else {
        // Print 16-bit value in little-endian order.
        const uint8_t* bytePtr = (const uint8_t*) &insns[insn_idx + i];
        fprintf(out_file_, " %02x%02x", bytePtr[0], bytePtr[1]);
      }
    } else {
      fputs("     ", out_file_);
    }
  }  // for

  // Dump pseudo-instruction or opcode.
  if (dec_insn->Opcode() == Instruction::NOP) {
    const uint16_t instr = Get2LE((const uint8_t*) &insns[insn_idx]);
    if (instr == Instruction::kPackedSwitchSignature) {
      fprintf(out_file_, "|%04x: packed-switch-data (%d units)", insn_idx, insn_width);
    } else if (instr == Instruction::kSparseSwitchSignature) {
      fprintf(out_file_, "|%04x: sparse-switch-data (%d units)", insn_idx, insn_width);
    } else if (instr == Instruction::kArrayDataSignature) {
      fprintf(out_file_, "|%04x: array-data (%d units)", insn_idx, insn_width);
    } else {
      fprintf(out_file_, "|%04x: nop // spacer", insn_idx);
    }
  } else {
    fprintf(out_file_, "|%04x: %s", insn_idx, dec_insn->Name());
  }

  // Set up additional argument.
  std::unique_ptr<char[]> index_buf;
  if (Instruction::IndexTypeOf(dec_insn->Opcode()) != Instruction::kIndexNone) {
    index_buf = IndexString(header_, dec_insn, 200);
  }

  // Dump the instruction.
  //
  // NOTE: pDecInsn->DumpString(pDexFile) differs too much from original.
  //
  switch (Instruction::FormatOf(dec_insn->Opcode())) {
    case Instruction::k10x:        // op
      break;
    case Instruction::k12x:        // op vA, vB
      fprintf(out_file_, " v%d, v%d", dec_insn->VRegA(), dec_insn->VRegB());
      break;
    case Instruction::k11n:        // op vA, #+B
      fprintf(out_file_, " v%d, #int %d // #%x",
              dec_insn->VRegA(), (int32_t) dec_insn->VRegB(), (uint8_t)dec_insn->VRegB());
      break;
    case Instruction::k11x:        // op vAA
      fprintf(out_file_, " v%d", dec_insn->VRegA());
      break;
    case Instruction::k10t:        // op +AA
    case Instruction::k20t: {      // op +AAAA
      const int32_t targ = (int32_t) dec_insn->VRegA();
      fprintf(out_file_, " %04x // %c%04x",
              insn_idx + targ,
              (targ < 0) ? '-' : '+',
              (targ < 0) ? -targ : targ);
      break;
    }
    case Instruction::k22x:        // op vAA, vBBBB
      fprintf(out_file_, " v%d, v%d", dec_insn->VRegA(), dec_insn->VRegB());
      break;
    case Instruction::k21t: {     // op vAA, +BBBB
      const int32_t targ = (int32_t) dec_insn->VRegB();
      fprintf(out_file_, " v%d, %04x // %c%04x", dec_insn->VRegA(),
              insn_idx + targ,
              (targ < 0) ? '-' : '+',
              (targ < 0) ? -targ : targ);
      break;
    }
    case Instruction::k21s:        // op vAA, #+BBBB
      fprintf(out_file_, " v%d, #int %d // #%x",
              dec_insn->VRegA(), (int32_t) dec_insn->VRegB(), (uint16_t)dec_insn->VRegB());
      break;
    case Instruction::k21h:        // op vAA, #+BBBB0000[00000000]
      // The printed format varies a bit based on the actual opcode.
      if (dec_insn->Opcode() == Instruction::CONST_HIGH16) {
        const int32_t value = dec_insn->VRegB() << 16;
        fprintf(out_file_, " v%d, #int %d // #%x",
                dec_insn->VRegA(), value, (uint16_t) dec_insn->VRegB());
      } else {
        const int64_t value = ((int64_t) dec_insn->VRegB()) << 48;
        fprintf(out_file_, " v%d, #long %" PRId64 " // #%x",
                dec_insn->VRegA(), value, (uint16_t) dec_insn->VRegB());
      }
      break;
    case Instruction::k21c:        // op vAA, thing@BBBB
    case Instruction::k31c:        // op vAA, thing@BBBBBBBB
      fprintf(out_file_, " v%d, %s", dec_insn->VRegA(), index_buf.get());
      break;
    case Instruction::k23x:        // op vAA, vBB, vCC
      fprintf(out_file_, " v%d, v%d, v%d",
              dec_insn->VRegA(), dec_insn->VRegB(), dec_insn->VRegC());
      break;
    case Instruction::k22b:        // op vAA, vBB, #+CC
      fprintf(out_file_, " v%d, v%d, #int %d // #%02x",
              dec_insn->VRegA(), dec_insn->VRegB(),
              (int32_t) dec_insn->VRegC(), (uint8_t) dec_insn->VRegC());
      break;
    case Instruction::k22t: {      // op vA, vB, +CCCC
      const int32_t targ = (int32_t) dec_insn->VRegC();
      fprintf(out_file_, " v%d, v%d, %04x // %c%04x",
              dec_insn->VRegA(), dec_insn->VRegB(),
              insn_idx + targ,
              (targ < 0) ? '-' : '+',
              (targ < 0) ? -targ : targ);
      break;
    }
    case Instruction::k22s:        // op vA, vB, #+CCCC
      fprintf(out_file_, " v%d, v%d, #int %d // #%04x",
              dec_insn->VRegA(), dec_insn->VRegB(),
              (int32_t) dec_insn->VRegC(), (uint16_t) dec_insn->VRegC());
      break;
    case Instruction::k22c:        // op vA, vB, thing@CCCC
    // NOT SUPPORTED:
    // case Instruction::k22cs:    // [opt] op vA, vB, field offset CCCC
      fprintf(out_file_, " v%d, v%d, %s",
              dec_insn->VRegA(), dec_insn->VRegB(), index_buf.get());
      break;
    case Instruction::k30t:
      fprintf(out_file_, " #%08x", dec_insn->VRegA());
      break;
    case Instruction::k31i: {     // op vAA, #+BBBBBBBB
      // This is often, but not always, a float.
      union {
        float f;
        uint32_t i;
      } conv;
      conv.i = dec_insn->VRegB();
      fprintf(out_file_, " v%d, #float %g // #%08x",
              dec_insn->VRegA(), conv.f, dec_insn->VRegB());
      break;
    }
    case Instruction::k31t:       // op vAA, offset +BBBBBBBB
      fprintf(out_file_, " v%d, %08x // +%08x",
              dec_insn->VRegA(), insn_idx + dec_insn->VRegB(), dec_insn->VRegB());
      break;
    case Instruction::k32x:        // op vAAAA, vBBBB
      fprintf(out_file_, " v%d, v%d", dec_insn->VRegA(), dec_insn->VRegB());
      break;
    case Instruction::k35c:           // op {vC, vD, vE, vF, vG}, thing@BBBB
    case Instruction::k45cc: {        // op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH
    // NOT SUPPORTED:
    // case Instruction::k35ms:       // [opt] invoke-virtual+super
    // case Instruction::k35mi:       // [opt] inline invoke
      uint32_t arg[Instruction::kMaxVarArgRegs];
      dec_insn->GetVarArgs(arg);
      fputs(" {", out_file_);
      for (int i = 0, n = dec_insn->VRegA(); i < n; i++) {
        if (i == 0) {
          fprintf(out_file_, "v%d", arg[i]);
        } else {
          fprintf(out_file_, ", v%d", arg[i]);
        }
      }  // for
      fprintf(out_file_, "}, %s", index_buf.get());
      break;
    }
    case Instruction::k3rc:           // op {vCCCC .. v(CCCC+AA-1)}, thing@BBBB
    case Instruction::k4rcc:          // op {vCCCC .. v(CCCC+AA-1)}, meth@BBBB, proto@HHHH
    // NOT SUPPORTED:
    // case Instruction::k3rms:       // [opt] invoke-virtual+super/range
    // case Instruction::k3rmi:       // [opt] execute-inline/range
      {
        // This doesn't match the "dx" output when some of the args are
        // 64-bit values -- dx only shows the first register.
        fputs(" {", out_file_);
        for (int i = 0, n = dec_insn->VRegA(); i < n; i++) {
          if (i == 0) {
            fprintf(out_file_, "v%d", dec_insn->VRegC() + i);
          } else {
            fprintf(out_file_, ", v%d", dec_insn->VRegC() + i);
          }
        }  // for
        fprintf(out_file_, "}, %s", index_buf.get());
      }
      break;
    case Instruction::k51l: {      // op vAA, #+BBBBBBBBBBBBBBBB
      // This is often, but not always, a double.
      union {
        double d;
        uint64_t j;
      } conv;
      conv.j = dec_insn->WideVRegB();
      fprintf(out_file_, " v%d, #double %g // #%016" PRIx64,
              dec_insn->VRegA(), conv.d, dec_insn->WideVRegB());
      break;
    }
    // NOT SUPPORTED:
    // case Instruction::k00x:        // unknown op or breakpoint
    //    break;
    default:
      fprintf(out_file_, " ???");
      break;
  }  // switch

  fputc('\n', out_file_);
}

/*
 * Dumps a bytecode disassembly.
 */
void DexLayout::DumpBytecodes(uint32_t idx, const dex_ir::CodeItem* code, uint32_t code_offset) {
  dex_ir::MethodId* method_id = header_->GetCollections().GetMethodId(idx);
  const char* name = method_id->Name()->Data();
  std::string type_descriptor = GetSignatureForProtoId(method_id->Proto());
  const char* back_descriptor = method_id->Class()->GetStringId()->Data();

  // Generate header.
  std::string dot(DescriptorToDotWrapper(back_descriptor));
  fprintf(out_file_, "%06x:                                        |[%06x] %s.%s:%s\n",
          code_offset, code_offset, dot.c_str(), name, type_descriptor.c_str());

  // Iterate over all instructions.
  for (const DexInstructionPcPair& inst : code->Instructions()) {
    const uint32_t insn_width = inst->SizeInCodeUnits();
    if (insn_width == 0) {
      LOG(WARNING) << "GLITCH: zero-width instruction at idx=0x" << std::hex << inst.DexPc();
      break;
    }
    DumpInstruction(code, code_offset, inst.DexPc(), insn_width, &inst.Inst());
  }  // for
}

/*
 * Callback for dumping each positions table entry.
 */
static bool DumpPositionsCb(void* context, const DexFile::PositionInfo& entry) {
  FILE* out_file = reinterpret_cast<FILE*>(context);
  fprintf(out_file, "        0x%04x line=%d\n", entry.address_, entry.line_);
  return false;
}

/*
 * Callback for dumping locals table entry.
 */
static void DumpLocalsCb(void* context, const DexFile::LocalInfo& entry) {
  const char* signature = entry.signature_ != nullptr ? entry.signature_ : "";
  FILE* out_file = reinterpret_cast<FILE*>(context);
  fprintf(out_file, "        0x%04x - 0x%04x reg=%d %s %s %s\n",
          entry.start_address_, entry.end_address_, entry.reg_,
          entry.name_, entry.descriptor_, signature);
}

/*
 * Lookup functions.
 */
static const char* StringDataByIdx(uint32_t idx, dex_ir::Collections& collections) {
  dex_ir::StringId* string_id = collections.GetStringIdOrNullPtr(idx);
  if (string_id == nullptr) {
    return nullptr;
  }
  return string_id->Data();
}

static const char* StringDataByTypeIdx(uint16_t idx, dex_ir::Collections& collections) {
  dex_ir::TypeId* type_id = collections.GetTypeIdOrNullPtr(idx);
  if (type_id == nullptr) {
    return nullptr;
  }
  dex_ir::StringId* string_id = type_id->GetStringId();
  if (string_id == nullptr) {
    return nullptr;
  }
  return string_id->Data();
}


/*
 * Dumps code of a method.
 */
void DexLayout::DumpCode(uint32_t idx,
                         const dex_ir::CodeItem* code,
                         uint32_t code_offset,
                         const char* declaring_class_descriptor,
                         const char* method_name,
                         bool is_static,
                         const dex_ir::ProtoId* proto) {
  fprintf(out_file_, "      registers     : %d\n", code->RegistersSize());
  fprintf(out_file_, "      ins           : %d\n", code->InsSize());
  fprintf(out_file_, "      outs          : %d\n", code->OutsSize());
  fprintf(out_file_, "      insns size    : %d 16-bit code units\n",
          code->InsnsSize());

  // Bytecode disassembly, if requested.
  if (options_.disassemble_) {
    DumpBytecodes(idx, code, code_offset);
  }

  // Try-catch blocks.
  DumpCatches(code);

  // Positions and locals table in the debug info.
  dex_ir::DebugInfoItem* debug_info = code->DebugInfo();
  fprintf(out_file_, "      positions     : \n");
  if (debug_info != nullptr) {
    DexFile::DecodeDebugPositionInfo(debug_info->GetDebugInfo(),
                                     [this](uint32_t idx) {
                                       return StringDataByIdx(idx, this->header_->GetCollections());
                                     },
                                     DumpPositionsCb,
                                     out_file_);
  }
  fprintf(out_file_, "      locals        : \n");
  if (debug_info != nullptr) {
    std::vector<const char*> arg_descriptors;
    const dex_ir::TypeList* parameters = proto->Parameters();
    if (parameters != nullptr) {
      const dex_ir::TypeIdVector* parameter_type_vector = parameters->GetTypeList();
      if (parameter_type_vector != nullptr) {
        for (const dex_ir::TypeId* type_id : *parameter_type_vector) {
          arg_descriptors.push_back(type_id->GetStringId()->Data());
        }
      }
    }
    DexFile::DecodeDebugLocalInfo(debug_info->GetDebugInfo(),
                                  "DexLayout in-memory",
                                  declaring_class_descriptor,
                                  arg_descriptors,
                                  method_name,
                                  is_static,
                                  code->RegistersSize(),
                                  code->InsSize(),
                                  code->InsnsSize(),
                                  [this](uint32_t idx) {
                                    return StringDataByIdx(idx, this->header_->GetCollections());
                                  },
                                  [this](uint32_t idx) {
                                    return
                                        StringDataByTypeIdx(dchecked_integral_cast<uint16_t>(idx),
                                                            this->header_->GetCollections());
                                  },
                                  DumpLocalsCb,
                                  out_file_);
  }
}

/*
 * Dumps a method.
 */
void DexLayout::DumpMethod(uint32_t idx, uint32_t flags, const dex_ir::CodeItem* code, int i) {
  // Bail for anything private if export only requested.
  if (options_.exports_only_ && (flags & (kAccPublic | kAccProtected)) == 0) {
    return;
  }

  dex_ir::MethodId* method_id = header_->GetCollections().GetMethodId(idx);
  const char* name = method_id->Name()->Data();
  char* type_descriptor = strdup(GetSignatureForProtoId(method_id->Proto()).c_str());
  const char* back_descriptor = method_id->Class()->GetStringId()->Data();
  char* access_str = CreateAccessFlagStr(flags, kAccessForMethod);

  if (options_.output_format_ == kOutputPlain) {
    fprintf(out_file_, "    #%d              : (in %s)\n", i, back_descriptor);
    fprintf(out_file_, "      name          : '%s'\n", name);
    fprintf(out_file_, "      type          : '%s'\n", type_descriptor);
    fprintf(out_file_, "      access        : 0x%04x (%s)\n", flags, access_str);
    if (code == nullptr) {
      fprintf(out_file_, "      code          : (none)\n");
    } else {
      fprintf(out_file_, "      code          -\n");
      DumpCode(idx,
               code,
               code->GetOffset(),
               back_descriptor,
               name,
               (flags & kAccStatic) != 0,
               method_id->Proto());
    }
    if (options_.disassemble_) {
      fputc('\n', out_file_);
    }
  } else if (options_.output_format_ == kOutputXml) {
    const bool constructor = (name[0] == '<');

    // Method name and prototype.
    if (constructor) {
      std::string dot(DescriptorClassToDot(back_descriptor));
      fprintf(out_file_, "<constructor name=\"%s\"\n", dot.c_str());
      dot = DescriptorToDotWrapper(back_descriptor);
      fprintf(out_file_, " type=\"%s\"\n", dot.c_str());
    } else {
      fprintf(out_file_, "<method name=\"%s\"\n", name);
      const char* return_type = strrchr(type_descriptor, ')');
      if (return_type == nullptr) {
        LOG(ERROR) << "bad method type descriptor '" << type_descriptor << "'";
        goto bail;
      }
      std::string dot(DescriptorToDotWrapper(return_type + 1));
      fprintf(out_file_, " return=\"%s\"\n", dot.c_str());
      fprintf(out_file_, " abstract=%s\n", QuotedBool((flags & kAccAbstract) != 0));
      fprintf(out_file_, " native=%s\n", QuotedBool((flags & kAccNative) != 0));
      fprintf(out_file_, " synchronized=%s\n", QuotedBool(
          (flags & (kAccSynchronized | kAccDeclaredSynchronized)) != 0));
    }

    // Additional method flags.
    fprintf(out_file_, " static=%s\n", QuotedBool((flags & kAccStatic) != 0));
    fprintf(out_file_, " final=%s\n", QuotedBool((flags & kAccFinal) != 0));
    // The "deprecated=" not knowable w/o parsing annotations.
    fprintf(out_file_, " visibility=%s\n>\n", QuotedVisibility(flags));

    // Parameters.
    if (type_descriptor[0] != '(') {
      LOG(ERROR) << "ERROR: bad descriptor '" << type_descriptor << "'";
      goto bail;
    }
    char* tmp_buf = reinterpret_cast<char*>(malloc(strlen(type_descriptor) + 1));
    const char* base = type_descriptor + 1;
    int arg_num = 0;
    while (*base != ')') {
      char* cp = tmp_buf;
      while (*base == '[') {
        *cp++ = *base++;
      }
      if (*base == 'L') {
        // Copy through ';'.
        do {
          *cp = *base++;
        } while (*cp++ != ';');
      } else {
        // Primitive char, copy it.
        if (strchr("ZBCSIFJD", *base) == nullptr) {
          LOG(ERROR) << "ERROR: bad method signature '" << base << "'";
          break;  // while
        }
        *cp++ = *base++;
      }
      // Null terminate and display.
      *cp++ = '\0';
      std::string dot(DescriptorToDotWrapper(tmp_buf));
      fprintf(out_file_, "<parameter name=\"arg%d\" type=\"%s\">\n"
                        "</parameter>\n", arg_num++, dot.c_str());
    }  // while
    free(tmp_buf);
    if (constructor) {
      fprintf(out_file_, "</constructor>\n");
    } else {
      fprintf(out_file_, "</method>\n");
    }
  }

 bail:
  free(type_descriptor);
  free(access_str);
}

/*
 * Dumps a static (class) field.
 */
void DexLayout::DumpSField(uint32_t idx, uint32_t flags, int i, dex_ir::EncodedValue* init) {
  // Bail for anything private if export only requested.
  if (options_.exports_only_ && (flags & (kAccPublic | kAccProtected)) == 0) {
    return;
  }

  dex_ir::FieldId* field_id = header_->GetCollections().GetFieldId(idx);
  const char* name = field_id->Name()->Data();
  const char* type_descriptor = field_id->Type()->GetStringId()->Data();
  const char* back_descriptor = field_id->Class()->GetStringId()->Data();
  char* access_str = CreateAccessFlagStr(flags, kAccessForField);

  if (options_.output_format_ == kOutputPlain) {
    fprintf(out_file_, "    #%d              : (in %s)\n", i, back_descriptor);
    fprintf(out_file_, "      name          : '%s'\n", name);
    fprintf(out_file_, "      type          : '%s'\n", type_descriptor);
    fprintf(out_file_, "      access        : 0x%04x (%s)\n", flags, access_str);
    if (init != nullptr) {
      fputs("      value         : ", out_file_);
      DumpEncodedValue(init);
      fputs("\n", out_file_);
    }
  } else if (options_.output_format_ == kOutputXml) {
    fprintf(out_file_, "<field name=\"%s\"\n", name);
    std::string dot(DescriptorToDotWrapper(type_descriptor));
    fprintf(out_file_, " type=\"%s\"\n", dot.c_str());
    fprintf(out_file_, " transient=%s\n", QuotedBool((flags & kAccTransient) != 0));
    fprintf(out_file_, " volatile=%s\n", QuotedBool((flags & kAccVolatile) != 0));
    // The "value=" is not knowable w/o parsing annotations.
    fprintf(out_file_, " static=%s\n", QuotedBool((flags & kAccStatic) != 0));
    fprintf(out_file_, " final=%s\n", QuotedBool((flags & kAccFinal) != 0));
    // The "deprecated=" is not knowable w/o parsing annotations.
    fprintf(out_file_, " visibility=%s\n", QuotedVisibility(flags));
    if (init != nullptr) {
      fputs(" value=\"", out_file_);
      DumpEncodedValue(init);
      fputs("\"\n", out_file_);
    }
    fputs(">\n</field>\n", out_file_);
  }

  free(access_str);
}

/*
 * Dumps an instance field.
 */
void DexLayout::DumpIField(uint32_t idx, uint32_t flags, int i) {
  DumpSField(idx, flags, i, nullptr);
}

/*
 * Dumps the class.
 *
 * Note "idx" is a DexClassDef index, not a DexTypeId index.
 *
 * If "*last_package" is nullptr or does not match the current class' package,
 * the value will be replaced with a newly-allocated string.
 */
void DexLayout::DumpClass(int idx, char** last_package) {
  dex_ir::ClassDef* class_def = header_->GetCollections().GetClassDef(idx);
  // Omitting non-public class.
  if (options_.exports_only_ && (class_def->GetAccessFlags() & kAccPublic) == 0) {
    return;
  }

  if (options_.show_section_headers_) {
    DumpClassDef(idx);
  }

  if (options_.show_annotations_) {
    DumpClassAnnotations(idx);
  }

  // For the XML output, show the package name.  Ideally we'd gather
  // up the classes, sort them, and dump them alphabetically so the
  // package name wouldn't jump around, but that's not a great plan
  // for something that needs to run on the device.
  const char* class_descriptor =
      header_->GetCollections().GetClassDef(idx)->ClassType()->GetStringId()->Data();
  if (!(class_descriptor[0] == 'L' &&
        class_descriptor[strlen(class_descriptor)-1] == ';')) {
    // Arrays and primitives should not be defined explicitly. Keep going?
    LOG(ERROR) << "Malformed class name '" << class_descriptor << "'";
  } else if (options_.output_format_ == kOutputXml) {
    char* mangle = strdup(class_descriptor + 1);
    mangle[strlen(mangle)-1] = '\0';

    // Reduce to just the package name.
    char* last_slash = strrchr(mangle, '/');
    if (last_slash != nullptr) {
      *last_slash = '\0';
    } else {
      *mangle = '\0';
    }

    for (char* cp = mangle; *cp != '\0'; cp++) {
      if (*cp == '/') {
        *cp = '.';
      }
    }  // for

    if (*last_package == nullptr || strcmp(mangle, *last_package) != 0) {
      // Start of a new package.
      if (*last_package != nullptr) {
        fprintf(out_file_, "</package>\n");
      }
      fprintf(out_file_, "<package name=\"%s\"\n>\n", mangle);
      free(*last_package);
      *last_package = mangle;
    } else {
      free(mangle);
    }
  }

  // General class information.
  char* access_str = CreateAccessFlagStr(class_def->GetAccessFlags(), kAccessForClass);
  const char* superclass_descriptor = nullptr;
  if (class_def->Superclass() != nullptr) {
    superclass_descriptor = class_def->Superclass()->GetStringId()->Data();
  }
  if (options_.output_format_ == kOutputPlain) {
    fprintf(out_file_, "Class #%d            -\n", idx);
    fprintf(out_file_, "  Class descriptor  : '%s'\n", class_descriptor);
    fprintf(out_file_, "  Access flags      : 0x%04x (%s)\n",
            class_def->GetAccessFlags(), access_str);
    if (superclass_descriptor != nullptr) {
      fprintf(out_file_, "  Superclass        : '%s'\n", superclass_descriptor);
    }
    fprintf(out_file_, "  Interfaces        -\n");
  } else {
    std::string dot(DescriptorClassToDot(class_descriptor));
    fprintf(out_file_, "<class name=\"%s\"\n", dot.c_str());
    if (superclass_descriptor != nullptr) {
      dot = DescriptorToDotWrapper(superclass_descriptor);
      fprintf(out_file_, " extends=\"%s\"\n", dot.c_str());
    }
    fprintf(out_file_, " interface=%s\n",
            QuotedBool((class_def->GetAccessFlags() & kAccInterface) != 0));
    fprintf(out_file_, " abstract=%s\n",
            QuotedBool((class_def->GetAccessFlags() & kAccAbstract) != 0));
    fprintf(out_file_, " static=%s\n", QuotedBool((class_def->GetAccessFlags() & kAccStatic) != 0));
    fprintf(out_file_, " final=%s\n", QuotedBool((class_def->GetAccessFlags() & kAccFinal) != 0));
    // The "deprecated=" not knowable w/o parsing annotations.
    fprintf(out_file_, " visibility=%s\n", QuotedVisibility(class_def->GetAccessFlags()));
    fprintf(out_file_, ">\n");
  }

  // Interfaces.
  const dex_ir::TypeList* interfaces = class_def->Interfaces();
  if (interfaces != nullptr) {
    const dex_ir::TypeIdVector* interfaces_vector = interfaces->GetTypeList();
    for (uint32_t i = 0; i < interfaces_vector->size(); i++) {
      DumpInterface((*interfaces_vector)[i], i);
    }  // for
  }

  // Fields and methods.
  dex_ir::ClassData* class_data = class_def->GetClassData();
  // Prepare data for static fields.
  dex_ir::EncodedArrayItem* static_values = class_def->StaticValues();
  dex_ir::EncodedValueVector* encoded_values =
      static_values == nullptr ? nullptr : static_values->GetEncodedValues();
  const uint32_t encoded_values_size = (encoded_values == nullptr) ? 0 : encoded_values->size();

  // Static fields.
  if (options_.output_format_ == kOutputPlain) {
    fprintf(out_file_, "  Static fields     -\n");
  }
  if (class_data != nullptr) {
    dex_ir::FieldItemVector* static_fields = class_data->StaticFields();
    if (static_fields != nullptr) {
      for (uint32_t i = 0; i < static_fields->size(); i++) {
        DumpSField((*static_fields)[i]->GetFieldId()->GetIndex(),
                   (*static_fields)[i]->GetAccessFlags(),
                   i,
                   i < encoded_values_size ? (*encoded_values)[i].get() : nullptr);
      }  // for
    }
  }

  // Instance fields.
  if (options_.output_format_ == kOutputPlain) {
    fprintf(out_file_, "  Instance fields   -\n");
  }
  if (class_data != nullptr) {
    dex_ir::FieldItemVector* instance_fields = class_data->InstanceFields();
    if (instance_fields != nullptr) {
      for (uint32_t i = 0; i < instance_fields->size(); i++) {
        DumpIField((*instance_fields)[i]->GetFieldId()->GetIndex(),
                   (*instance_fields)[i]->GetAccessFlags(),
                   i);
      }  // for
    }
  }

  // Direct methods.
  if (options_.output_format_ == kOutputPlain) {
    fprintf(out_file_, "  Direct methods    -\n");
  }
  if (class_data != nullptr) {
    dex_ir::MethodItemVector* direct_methods = class_data->DirectMethods();
    if (direct_methods != nullptr) {
      for (uint32_t i = 0; i < direct_methods->size(); i++) {
        DumpMethod((*direct_methods)[i]->GetMethodId()->GetIndex(),
                   (*direct_methods)[i]->GetAccessFlags(),
                   (*direct_methods)[i]->GetCodeItem(),
                 i);
      }  // for
    }
  }

  // Virtual methods.
  if (options_.output_format_ == kOutputPlain) {
    fprintf(out_file_, "  Virtual methods   -\n");
  }
  if (class_data != nullptr) {
    dex_ir::MethodItemVector* virtual_methods = class_data->VirtualMethods();
    if (virtual_methods != nullptr) {
      for (uint32_t i = 0; i < virtual_methods->size(); i++) {
        DumpMethod((*virtual_methods)[i]->GetMethodId()->GetIndex(),
                   (*virtual_methods)[i]->GetAccessFlags(),
                   (*virtual_methods)[i]->GetCodeItem(),
                   i);
      }  // for
    }
  }

  // End of class.
  if (options_.output_format_ == kOutputPlain) {
    const char* file_name = "unknown";
    if (class_def->SourceFile() != nullptr) {
      file_name = class_def->SourceFile()->Data();
    }
    const dex_ir::StringId* source_file = class_def->SourceFile();
    fprintf(out_file_, "  source_file_idx   : %d (%s)\n\n",
            source_file == nullptr ? 0xffffffffU : source_file->GetIndex(), file_name);
  } else if (options_.output_format_ == kOutputXml) {
    fprintf(out_file_, "</class>\n");
  }

  free(access_str);
}

void DexLayout::DumpDexFile() {
  // Headers.
  if (options_.show_file_headers_) {
    DumpFileHeader();
  }

  // Open XML context.
  if (options_.output_format_ == kOutputXml) {
    fprintf(out_file_, "<api>\n");
  }

  // Iterate over all classes.
  char* package = nullptr;
  const uint32_t class_defs_size = header_->GetCollections().ClassDefsSize();
  for (uint32_t i = 0; i < class_defs_size; i++) {
    DumpClass(i, &package);
  }  // for

  // Free the last package allocated.
  if (package != nullptr) {
    fprintf(out_file_, "</package>\n");
    free(package);
  }

  // Close XML context.
  if (options_.output_format_ == kOutputXml) {
    fprintf(out_file_, "</api>\n");
  }
}

void DexLayout::LayoutClassDefsAndClassData(const DexFile* dex_file) {
  std::vector<dex_ir::ClassDef*> new_class_def_order;
  for (std::unique_ptr<dex_ir::ClassDef>& class_def : header_->GetCollections().ClassDefs()) {
    dex::TypeIndex type_idx(class_def->ClassType()->GetIndex());
    if (info_->ContainsClass(*dex_file, type_idx)) {
      new_class_def_order.push_back(class_def.get());
    }
  }
  for (std::unique_ptr<dex_ir::ClassDef>& class_def : header_->GetCollections().ClassDefs()) {
    dex::TypeIndex type_idx(class_def->ClassType()->GetIndex());
    if (!info_->ContainsClass(*dex_file, type_idx)) {
      new_class_def_order.push_back(class_def.get());
    }
  }
  std::unordered_set<dex_ir::ClassData*> visited_class_data;
  size_t class_data_index = 0;
  dex_ir::CollectionVector<dex_ir::ClassData>::Vector& class_datas =
      header_->GetCollections().ClassDatas();
  for (dex_ir::ClassDef* class_def : new_class_def_order) {
    dex_ir::ClassData* class_data = class_def->GetClassData();
    if (class_data != nullptr && visited_class_data.find(class_data) == visited_class_data.end()) {
      visited_class_data.insert(class_data);
      // Overwrite the existing vector with the new ordering, note that the sets of objects are
      // equivalent, but the order changes. This is why this is not a memory leak.
      // TODO: Consider cleaning this up with a shared_ptr.
      class_datas[class_data_index].release();
      class_datas[class_data_index].reset(class_data);
      ++class_data_index;
    }
  }
  CHECK_EQ(class_data_index, class_datas.size());

  if (DexLayout::kChangeClassDefOrder) {
    // This currently produces dex files that violate the spec since the super class class_def is
    // supposed to occur before any subclasses.
    dex_ir::CollectionVector<dex_ir::ClassDef>::Vector& class_defs =
        header_->GetCollections().ClassDefs();
    CHECK_EQ(new_class_def_order.size(), class_defs.size());
    for (size_t i = 0; i < class_defs.size(); ++i) {
      // Overwrite the existing vector with the new ordering, note that the sets of objects are
      // equivalent, but the order changes. This is why this is not a memory leak.
      // TODO: Consider cleaning this up with a shared_ptr.
      class_defs[i].release();
      class_defs[i].reset(new_class_def_order[i]);
    }
  }
}

void DexLayout::LayoutStringData(const DexFile* dex_file) {
  const size_t num_strings = header_->GetCollections().StringIds().size();
  std::vector<bool> is_shorty(num_strings, false);
  std::vector<bool> from_hot_method(num_strings, false);
  for (std::unique_ptr<dex_ir::ClassDef>& class_def : header_->GetCollections().ClassDefs()) {
    // A name of a profile class is probably going to get looked up by ClassTable::Lookup, mark it
    // as hot. Add its super class and interfaces as well, which can be used during initialization.
    const bool is_profile_class =
        info_->ContainsClass(*dex_file, dex::TypeIndex(class_def->ClassType()->GetIndex()));
    if (is_profile_class) {
      from_hot_method[class_def->ClassType()->GetStringId()->GetIndex()] = true;
      const dex_ir::TypeId* superclass = class_def->Superclass();
      if (superclass != nullptr) {
        from_hot_method[superclass->GetStringId()->GetIndex()] = true;
      }
      const dex_ir::TypeList* interfaces = class_def->Interfaces();
      if (interfaces != nullptr) {
        for (const dex_ir::TypeId* interface_type : *interfaces->GetTypeList()) {
          from_hot_method[interface_type->GetStringId()->GetIndex()] = true;
        }
      }
    }
    dex_ir::ClassData* data = class_def->GetClassData();
    if (data == nullptr) {
      continue;
    }
    for (size_t i = 0; i < 2; ++i) {
      for (auto& method : *(i == 0 ? data->DirectMethods() : data->VirtualMethods())) {
        const dex_ir::MethodId* method_id = method->GetMethodId();
        dex_ir::CodeItem* code_item = method->GetCodeItem();
        if (code_item == nullptr) {
          continue;
        }
        const bool is_clinit = is_profile_class &&
            (method->GetAccessFlags() & kAccConstructor) != 0 &&
            (method->GetAccessFlags() & kAccStatic) != 0;
        const bool method_executed = is_clinit ||
            info_->GetMethodHotness(MethodReference(dex_file, method_id->GetIndex())).IsInProfile();
        if (!method_executed) {
          continue;
        }
        is_shorty[method_id->Proto()->Shorty()->GetIndex()] = true;
        dex_ir::CodeFixups* fixups = code_item->GetCodeFixups();
        if (fixups == nullptr) {
          continue;
        }
        // Add const-strings.
        for (dex_ir::StringId* id : fixups->StringIds()) {
          from_hot_method[id->GetIndex()] = true;
        }
        // Add field classes, names, and types.
        for (dex_ir::FieldId* id : fixups->FieldIds()) {
          // TODO: Only visit field ids from static getters and setters.
          from_hot_method[id->Class()->GetStringId()->GetIndex()] = true;
          from_hot_method[id->Name()->GetIndex()] = true;
          from_hot_method[id->Type()->GetStringId()->GetIndex()] = true;
        }
        // For clinits, add referenced method classes, names, and protos.
        if (is_clinit) {
          for (dex_ir::MethodId* id : fixups->MethodIds()) {
            from_hot_method[id->Class()->GetStringId()->GetIndex()] = true;
            from_hot_method[id->Name()->GetIndex()] = true;
            is_shorty[id->Proto()->Shorty()->GetIndex()] = true;
          }
        }
      }
    }
  }
  // Sort string data by specified order.
  std::vector<dex_ir::StringId*> string_ids;
  for (auto& string_id : header_->GetCollections().StringIds()) {
    string_ids.push_back(string_id.get());
  }
  std::sort(string_ids.begin(),
            string_ids.end(),
            [&is_shorty, &from_hot_method](const dex_ir::StringId* a,
                                           const dex_ir::StringId* b) {
    const bool a_is_hot = from_hot_method[a->GetIndex()];
    const bool b_is_hot = from_hot_method[b->GetIndex()];
    if (a_is_hot != b_is_hot) {
      return a_is_hot < b_is_hot;
    }
    // After hot methods are partitioned, subpartition shorties.
    const bool a_is_shorty = is_shorty[a->GetIndex()];
    const bool b_is_shorty = is_shorty[b->GetIndex()];
    if (a_is_shorty != b_is_shorty) {
      return a_is_shorty < b_is_shorty;
    }
    // Order by index by default.
    return a->GetIndex() < b->GetIndex();
  });
  dex_ir::CollectionVector<dex_ir::StringData>::Vector& string_datas =
      header_->GetCollections().StringDatas();
  // Now we know what order we want the string data, reorder them.
  size_t data_index = 0;
  for (dex_ir::StringId* string_id : string_ids) {
    string_datas[data_index].release();
    string_datas[data_index].reset(string_id->DataItem());
    ++data_index;
  }
  if (kIsDebugBuild) {
    std::unordered_set<dex_ir::StringData*> visited;
    for (const std::unique_ptr<dex_ir::StringData>& data : string_datas) {
      visited.insert(data.get());
    }
    for (auto& string_id : header_->GetCollections().StringIds()) {
      CHECK(visited.find(string_id->DataItem()) != visited.end());
    }
  }
  CHECK_EQ(data_index, string_datas.size());
}

// Orders code items according to specified class data ordering.
void DexLayout::LayoutCodeItems(const DexFile* dex_file) {
  static constexpr InvokeType invoke_types[] = {
    kDirect,
    kVirtual
  };

  std::unordered_map<dex_ir::CodeItem*, LayoutType>& code_item_layout =
      layout_hotness_info_.code_item_layout_;

  // Assign hotness flags to all code items.
  for (InvokeType invoke_type : invoke_types) {
    for (std::unique_ptr<dex_ir::ClassDef>& class_def : header_->GetCollections().ClassDefs()) {
      const bool is_profile_class =
          info_->ContainsClass(*dex_file, dex::TypeIndex(class_def->ClassType()->GetIndex()));

      // Skip classes that are not defined in this dex file.
      dex_ir::ClassData* class_data = class_def->GetClassData();
      if (class_data == nullptr) {
        continue;
      }
      for (auto& method : *(invoke_type == InvokeType::kDirect
                                ? class_data->DirectMethods()
                                : class_data->VirtualMethods())) {
        const dex_ir::MethodId *method_id = method->GetMethodId();
        dex_ir::CodeItem *code_item = method->GetCodeItem();
        if (code_item == nullptr) {
          continue;
        }
        // Separate executed methods (clinits and profiled methods) from unexecuted methods.
        const bool is_clinit = (method->GetAccessFlags() & kAccConstructor) != 0 &&
            (method->GetAccessFlags() & kAccStatic) != 0;
        const bool is_startup_clinit = is_profile_class && is_clinit;
        using Hotness = ProfileCompilationInfo::MethodHotness;
        Hotness hotness = info_->GetMethodHotness(MethodReference(dex_file, method_id->GetIndex()));
        LayoutType state = LayoutType::kLayoutTypeUnused;
        if (hotness.IsHot()) {
          // Hot code is compiled, maybe one day it won't be accessed. So lay it out together for
          // now.
          state = LayoutType::kLayoutTypeHot;
        } else if (is_startup_clinit || hotness.GetFlags() == Hotness::kFlagStartup) {
          // Startup clinit or a method that only has the startup flag.
          state = LayoutType::kLayoutTypeStartupOnly;
        } else if (is_clinit) {
          state = LayoutType::kLayoutTypeUsedOnce;
        } else if (hotness.IsInProfile()) {
          state = LayoutType::kLayoutTypeSometimesUsed;
        }
        auto it = code_item_layout.emplace(code_item, state);
        if (!it.second) {
          LayoutType& layout_type = it.first->second;
          // Already exists, merge the hotness.
          layout_type = MergeLayoutType(layout_type, state);
        }
      }
    }
  }

  dex_ir::CollectionVector<dex_ir::CodeItem>::Vector& code_items =
        header_->GetCollections().CodeItems();
  if (VLOG_IS_ON(dex)) {
    size_t layout_count[static_cast<size_t>(LayoutType::kLayoutTypeCount)] = {};
    for (const std::unique_ptr<dex_ir::CodeItem>& code_item : code_items) {
      auto it = code_item_layout.find(code_item.get());
      DCHECK(it != code_item_layout.end());
      ++layout_count[static_cast<size_t>(it->second)];
    }
    for (size_t i = 0; i < static_cast<size_t>(LayoutType::kLayoutTypeCount); ++i) {
      LOG(INFO) << "Code items in category " << i << " count=" << layout_count[i];
    }
  }

  // Sort the code items vector by new layout. The writing process will take care of calculating
  // all the offsets. Stable sort to preserve any existing locality that might be there.
  std::stable_sort(code_items.begin(),
                   code_items.end(),
                   [&](const std::unique_ptr<dex_ir::CodeItem>& a,
                       const std::unique_ptr<dex_ir::CodeItem>& b) {
    auto it_a = code_item_layout.find(a.get());
    auto it_b = code_item_layout.find(b.get());
    DCHECK(it_a != code_item_layout.end());
    DCHECK(it_b != code_item_layout.end());
    const LayoutType layout_type_a = it_a->second;
    const LayoutType layout_type_b = it_b->second;
    return layout_type_a < layout_type_b;
  });
}

void DexLayout::LayoutOutputFile(const DexFile* dex_file) {
  LayoutStringData(dex_file);
  LayoutClassDefsAndClassData(dex_file);
  LayoutCodeItems(dex_file);
}

bool DexLayout::OutputDexFile(const DexFile* input_dex_file,
                              bool compute_offsets,
                              std::unique_ptr<DexContainer>* dex_container,
                              std::string* error_msg) {
  const std::string& dex_file_location = input_dex_file->GetLocation();
  std::unique_ptr<File> new_file;
  // If options_.output_dex_directory_ is non null, we are outputting to a file.
  if (options_.output_dex_directory_ != nullptr) {
    std::string output_location(options_.output_dex_directory_);
    size_t last_slash = dex_file_location.rfind('/');
    std::string dex_file_directory = dex_file_location.substr(0, last_slash + 1);
    if (output_location == dex_file_directory) {
      output_location = dex_file_location + ".new";
    } else if (last_slash != std::string::npos) {
      output_location += dex_file_location.substr(last_slash);
    } else {
      output_location += "/" + dex_file_location + ".new";
    }
    new_file.reset(OS::CreateEmptyFile(output_location.c_str()));
    if (new_file == nullptr) {
      LOG(ERROR) << "Could not create dex writer output file: " << output_location;
      return false;
    }
  }
  if (!DexWriter::Output(this, dex_container, compute_offsets, error_msg)) {
    return false;
  }
  if (new_file != nullptr) {
    DexContainer* const container = dex_container->get();
    DexContainer::Section* const main_section = container->GetMainSection();
    if (!new_file->WriteFully(main_section->Begin(), main_section->Size())) {
      LOG(ERROR) << "Failed to write main section for dex file " << dex_file_location;
      new_file->Erase();
      return false;
    }
    DexContainer::Section* const data_section = container->GetDataSection();
    if (!new_file->WriteFully(data_section->Begin(), data_section->Size())) {
      LOG(ERROR) << "Failed to write data section for dex file " << dex_file_location;
      new_file->Erase();
      return false;
    }
    UNUSED(new_file->FlushCloseOrErase());
  }
  return true;
}

/*
 * Dumps the requested sections of the file.
 */
bool DexLayout::ProcessDexFile(const char* file_name,
                               const DexFile* dex_file,
                               size_t dex_file_index,
                               std::unique_ptr<DexContainer>* dex_container,
                               std::string* error_msg) {
  const bool has_output_container = dex_container != nullptr;
  const bool output = options_.output_dex_directory_ != nullptr || has_output_container;

  // Try to avoid eagerly assigning offsets to find bugs since GetOffset will abort if the offset
  // is unassigned.
  bool eagerly_assign_offsets = false;
  if (options_.visualize_pattern_ || options_.show_section_statistics_ || options_.dump_) {
    // These options required the offsets for dumping purposes.
    eagerly_assign_offsets = true;
  }
  std::unique_ptr<dex_ir::Header> header(dex_ir::DexIrBuilder(*dex_file,
                                                               eagerly_assign_offsets,
                                                               GetOptions()));
  SetHeader(header.get());

  if (options_.verbose_) {
    fprintf(out_file_, "Opened '%s', DEX version '%.3s'\n",
            file_name, dex_file->GetHeader().magic_ + 4);
  }

  if (options_.visualize_pattern_) {
    VisualizeDexLayout(header_, dex_file, dex_file_index, info_);
    return true;
  }

  if (options_.show_section_statistics_) {
    ShowDexSectionStatistics(header_, dex_file_index);
    return true;
  }

  // Dump dex file.
  if (options_.dump_) {
    DumpDexFile();
  }

  // In case we are outputting to a file, keep it open so we can verify.
  if (output) {
    // Layout information about what strings and code items are hot. Used by the writing process
    // to generate the sections that are stored in the oat file.
    bool do_layout = info_ != nullptr;
    if (do_layout) {
      LayoutOutputFile(dex_file);
    }
    // The output needs a dex container, use a temporary one.
    std::unique_ptr<DexContainer> temp_container;
    if (dex_container == nullptr) {
      dex_container = &temp_container;
    }
    // If we didn't set the offsets eagerly, we definitely need to compute them here.
    if (!OutputDexFile(dex_file, do_layout || !eagerly_assign_offsets, dex_container, error_msg)) {
      return false;
    }

    // Clear header before verifying to reduce peak RAM usage.
    const size_t file_size = header_->FileSize();
    header.reset();

    // Verify the output dex file's structure, only enabled by default for debug builds.
    if (options_.verify_output_ && has_output_container) {
      std::string location = "memory mapped file for " + std::string(file_name);
      // Dex file verifier cannot handle compact dex.
      bool verify = options_.compact_dex_level_ == CompactDexLevel::kCompactDexLevelNone;
      const ArtDexFileLoader dex_file_loader;
      DexContainer::Section* const main_section = (*dex_container)->GetMainSection();
      DexContainer::Section* const data_section = (*dex_container)->GetDataSection();
      DCHECK_EQ(file_size, main_section->Size())
          << main_section->Size() << " " << data_section->Size();
      std::unique_ptr<const DexFile> output_dex_file(
          dex_file_loader.OpenWithDataSection(
              main_section->Begin(),
              main_section->Size(),
              data_section->Begin(),
              data_section->Size(),
              location,
              /* checksum */ 0,
              /*oat_dex_file*/ nullptr,
              verify,
              /*verify_checksum*/ false,
              error_msg));
      CHECK(output_dex_file != nullptr) << "Failed to re-open output file:" << *error_msg;

      // Do IR-level comparison between input and output. This check ignores potential differences
      // due to layout, so offsets are not checked. Instead, it checks the data contents of each
      // item.
      //
      // Regenerate output IR to catch any bugs that might happen during writing.
      std::unique_ptr<dex_ir::Header> output_header(
          dex_ir::DexIrBuilder(*output_dex_file,
                               /*eagerly_assign_offsets*/ true,
                               GetOptions()));
      std::unique_ptr<dex_ir::Header> orig_header(
          dex_ir::DexIrBuilder(*dex_file,
                               /*eagerly_assign_offsets*/ true,
                               GetOptions()));
      CHECK(VerifyOutputDexFile(output_header.get(), orig_header.get(), error_msg)) << *error_msg;
    }
  }
  return true;
}

/*
 * Processes a single file (either direct .dex or indirect .zip/.jar/.apk).
 */
int DexLayout::ProcessFile(const char* file_name) {
  if (options_.verbose_) {
    fprintf(out_file_, "Processing '%s'...\n", file_name);
  }

  // If the file is not a .dex file, the function tries .zip/.jar/.apk files,
  // all of which are Zip archives with "classes.dex" inside.
  const bool verify_checksum = !options_.ignore_bad_checksum_;
  std::string error_msg;
  const ArtDexFileLoader dex_file_loader;
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  if (!dex_file_loader.Open(
        file_name, file_name, /* verify */ true, verify_checksum, &error_msg, &dex_files)) {
    // Display returned error message to user. Note that this error behavior
    // differs from the error messages shown by the original Dalvik dexdump.
    LOG(ERROR) << error_msg;
    return -1;
  }

  // Success. Either report checksum verification or process
  // all dex files found in given file.
  if (options_.checksum_only_) {
    fprintf(out_file_, "Checksum verified\n");
  } else {
    for (size_t i = 0; i < dex_files.size(); i++) {
      // Pass in a null container to avoid output by default.
      if (!ProcessDexFile(file_name,
                          dex_files[i].get(),
                          i,
                          /*dex_container*/ nullptr,
                          &error_msg)) {
        LOG(WARNING) << "Failed to run dex file " << i << " in " << file_name << " : " << error_msg;
      }
    }
  }
  return 0;
}

}  // namespace art
