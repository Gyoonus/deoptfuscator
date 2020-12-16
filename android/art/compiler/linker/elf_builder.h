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

#ifndef ART_COMPILER_LINKER_ELF_BUILDER_H_
#define ART_COMPILER_LINKER_ELF_BUILDER_H_

#include <vector>

#include "arch/instruction_set.h"
#include "arch/mips/instruction_set_features_mips.h"
#include "base/array_ref.h"
#include "base/bit_utils.h"
#include "base/casts.h"
#include "base/leb128.h"
#include "base/unix_file/fd_file.h"
#include "elf_utils.h"
#include "linker/error_delaying_output_stream.h"

namespace art {
namespace linker {

// Writes ELF file.
//
// The basic layout of the elf file:
//   Elf_Ehdr                    - The ELF header.
//   Elf_Phdr[]                  - Program headers for the linker.
//   .note.gnu.build-id          - Optional build ID section (SHA-1 digest).
//   .rodata                     - Oat metadata.
//   .text                       - Compiled code.
//   .bss                        - Zero-initialized writeable section.
//   .dex                        - Reserved NOBITS space for dex-related data.
//   .MIPS.abiflags              - MIPS specific section.
//   .dynstr                     - Names for .dynsym.
//   .dynsym                     - A few oat-specific dynamic symbols.
//   .hash                       - Hash-table for .dynsym.
//   .dynamic                    - Tags which let the linker locate .dynsym.
//   .strtab                     - Names for .symtab.
//   .symtab                     - Debug symbols.
//   .eh_frame                   - Unwind information (CFI).
//   .eh_frame_hdr               - Index of .eh_frame.
//   .debug_frame                - Unwind information (CFI).
//   .debug_frame.oat_patches    - Addresses for relocation.
//   .debug_info                 - Debug information.
//   .debug_info.oat_patches     - Addresses for relocation.
//   .debug_abbrev               - Decoding information for .debug_info.
//   .debug_str                  - Strings for .debug_info.
//   .debug_line                 - Line number tables.
//   .debug_line.oat_patches     - Addresses for relocation.
//   .text.oat_patches           - Addresses for relocation.
//   .shstrtab                   - Names of ELF sections.
//   Elf_Shdr[]                  - Section headers.
//
// Some section are optional (the debug sections in particular).
//
// We try write the section data directly into the file without much
// in-memory buffering.  This means we generally write sections based on the
// dependency order (e.g. .dynamic points to .dynsym which points to .text).
//
// In the cases where we need to buffer, we write the larger section first
// and buffer the smaller one (e.g. .strtab is bigger than .symtab).
//
// The debug sections are written last for easier stripping.
//
template <typename ElfTypes>
class ElfBuilder FINAL {
 public:
  static constexpr size_t kMaxProgramHeaders = 16;
  // SHA-1 digest.  Not using SHA_DIGEST_LENGTH from openssl/sha.h to avoid
  // spreading this header dependency for just this single constant.
  static constexpr size_t kBuildIdLen = 20;

  using Elf_Addr = typename ElfTypes::Addr;
  using Elf_Off = typename ElfTypes::Off;
  using Elf_Word = typename ElfTypes::Word;
  using Elf_Sword = typename ElfTypes::Sword;
  using Elf_Ehdr = typename ElfTypes::Ehdr;
  using Elf_Shdr = typename ElfTypes::Shdr;
  using Elf_Sym = typename ElfTypes::Sym;
  using Elf_Phdr = typename ElfTypes::Phdr;
  using Elf_Dyn = typename ElfTypes::Dyn;

  // Base class of all sections.
  class Section : public OutputStream {
   public:
    Section(ElfBuilder<ElfTypes>* owner,
            const std::string& name,
            Elf_Word type,
            Elf_Word flags,
            const Section* link,
            Elf_Word info,
            Elf_Word align,
            Elf_Word entsize)
        : OutputStream(name),
          owner_(owner),
          header_(),
          section_index_(0),
          name_(name),
          link_(link),
          phdr_flags_(PF_R),
          phdr_type_(0) {
      DCHECK_GE(align, 1u);
      header_.sh_type = type;
      header_.sh_flags = flags;
      header_.sh_info = info;
      header_.sh_addralign = align;
      header_.sh_entsize = entsize;
    }

    // Allocate chunk of virtual memory for this section from the owning ElfBuilder.
    // This must be done at the start for all SHF_ALLOC sections (i.e. mmaped by linker).
    // It is fine to allocate section but never call Start/End() (e.g. the .bss section).
    void AllocateVirtualMemory(Elf_Word size) {
      AllocateVirtualMemory(owner_->virtual_address_, size);
    }

    void AllocateVirtualMemory(Elf_Addr addr, Elf_Word size) {
      CHECK_NE(header_.sh_flags & SHF_ALLOC, 0u);
      Elf_Word align = AddSection();
      CHECK_EQ(header_.sh_addr, 0u);
      header_.sh_addr = RoundUp(addr, align);
      CHECK(header_.sh_size == 0u || header_.sh_size == size);
      header_.sh_size = size;
      CHECK_LE(owner_->virtual_address_, header_.sh_addr);
      owner_->virtual_address_ = header_.sh_addr + header_.sh_size;
    }

    // Start writing file data of this section.
    void Start() {
      CHECK(owner_->current_section_ == nullptr);
      Elf_Word align = AddSection();
      CHECK_EQ(header_.sh_offset, 0u);
      header_.sh_offset = owner_->AlignFileOffset(align);
      owner_->current_section_ = this;
    }

    // Finish writing file data of this section.
    void End() {
      CHECK(owner_->current_section_ == this);
      Elf_Word position = GetPosition();
      CHECK(header_.sh_size == 0u || header_.sh_size == position);
      header_.sh_size = position;
      owner_->current_section_ = nullptr;
    }

    // Get the number of bytes written so far.
    // Only valid while writing the section.
    Elf_Word GetPosition() const {
      CHECK(owner_->current_section_ == this);
      off_t file_offset = owner_->stream_.Seek(0, kSeekCurrent);
      DCHECK_GE(file_offset, (off_t)header_.sh_offset);
      return file_offset - header_.sh_offset;
    }

    // Get the location of this section in virtual memory.
    Elf_Addr GetAddress() const {
      DCHECK_NE(header_.sh_flags & SHF_ALLOC, 0u);
      DCHECK_NE(header_.sh_addr, 0u);
      return header_.sh_addr;
    }

    // This function always succeeds to simplify code.
    // Use builder's Good() to check the actual status.
    bool WriteFully(const void* buffer, size_t byte_count) OVERRIDE {
      CHECK(owner_->current_section_ == this);
      return owner_->stream_.WriteFully(buffer, byte_count);
    }

    // This function always succeeds to simplify code.
    // Use builder's Good() to check the actual status.
    off_t Seek(off_t offset, Whence whence) OVERRIDE {
      // Forward the seek as-is and trust the caller to use it reasonably.
      return owner_->stream_.Seek(offset, whence);
    }

    // This function flushes the output and returns whether it succeeded.
    // If there was a previous failure, this does nothing and returns false, i.e. failed.
    bool Flush() OVERRIDE {
      return owner_->stream_.Flush();
    }

    Elf_Word GetSectionIndex() const {
      DCHECK_NE(section_index_, 0u);
      return section_index_;
    }

    // Returns true if this section has been added.
    bool Exists() const {
      return section_index_ != 0;
    }

   protected:
    // Add this section to the list of generated ELF sections (if not there already).
    // It also ensures the alignment is sufficient to generate valid program headers,
    // since that depends on the previous section. It returns the required alignment.
    Elf_Word AddSection() {
      if (section_index_ == 0) {
        std::vector<Section*>& sections = owner_->sections_;
        Elf_Word last = sections.empty() ? PF_R : sections.back()->phdr_flags_;
        if (phdr_flags_ != last) {
          header_.sh_addralign = kPageSize;  // Page-align if R/W/X flags changed.
        }
        sections.push_back(this);
        section_index_ = sections.size();  // First ELF section has index 1.
      }
      return owner_->write_program_headers_ ? header_.sh_addralign : 1;
    }

    ElfBuilder<ElfTypes>* owner_;
    Elf_Shdr header_;
    Elf_Word section_index_;
    const std::string name_;
    const Section* const link_;
    Elf_Word phdr_flags_;
    Elf_Word phdr_type_;

    friend class ElfBuilder;

    DISALLOW_COPY_AND_ASSIGN(Section);
  };

  class CachedSection : public Section {
   public:
    CachedSection(ElfBuilder<ElfTypes>* owner,
                  const std::string& name,
                  Elf_Word type,
                  Elf_Word flags,
                  const Section* link,
                  Elf_Word info,
                  Elf_Word align,
                  Elf_Word entsize)
        : Section(owner, name, type, flags, link, info, align, entsize), cache_() { }

    Elf_Word Add(const void* data, size_t length) {
      Elf_Word offset = cache_.size();
      const uint8_t* d = reinterpret_cast<const uint8_t*>(data);
      cache_.insert(cache_.end(), d, d + length);
      return offset;
    }

    Elf_Word GetCacheSize() {
      return cache_.size();
    }

    void Write() {
      this->WriteFully(cache_.data(), cache_.size());
      cache_.clear();
      cache_.shrink_to_fit();
    }

    void WriteCachedSection() {
      this->Start();
      Write();
      this->End();
    }

   private:
    std::vector<uint8_t> cache_;
  };

  // Writer of .dynstr section.
  class CachedStringSection FINAL : public CachedSection {
   public:
    CachedStringSection(ElfBuilder<ElfTypes>* owner,
                        const std::string& name,
                        Elf_Word flags,
                        Elf_Word align)
        : CachedSection(owner,
                        name,
                        SHT_STRTAB,
                        flags,
                        /* link */ nullptr,
                        /* info */ 0,
                        align,
                        /* entsize */ 0) { }

    Elf_Word Add(const std::string& name) {
      if (CachedSection::GetCacheSize() == 0u) {
        DCHECK(name.empty());
      }
      return CachedSection::Add(name.c_str(), name.length() + 1);
    }
  };

  // Writer of .strtab and .shstrtab sections.
  class StringSection FINAL : public Section {
   public:
    StringSection(ElfBuilder<ElfTypes>* owner,
                  const std::string& name,
                  Elf_Word flags,
                  Elf_Word align)
        : Section(owner,
                  name,
                  SHT_STRTAB,
                  flags,
                  /* link */ nullptr,
                  /* info */ 0,
                  align,
                  /* entsize */ 0),
          current_offset_(0),
          last_offset_(0) {
    }

    Elf_Word Write(const std::string& name) {
      if (current_offset_ == 0) {
        DCHECK(name.empty());
      } else if (name == last_name_) {
        return last_offset_;  // Very simple string de-duplication.
      }
      last_name_ = name;
      last_offset_ = current_offset_;
      this->WriteFully(name.c_str(), name.length() + 1);
      current_offset_ += name.length() + 1;
      return last_offset_;
    }

   private:
    Elf_Word current_offset_;
    std::string last_name_;
    Elf_Word last_offset_;
  };

  // Writer of .dynsym and .symtab sections.
  class SymbolSection FINAL : public Section {
   public:
    SymbolSection(ElfBuilder<ElfTypes>* owner,
                  const std::string& name,
                  Elf_Word type,
                  Elf_Word flags,
                  Section* strtab)
        : Section(owner,
                  name,
                  type,
                  flags,
                  strtab,
                  /* info */ 1,
                  sizeof(Elf_Off),
                  sizeof(Elf_Sym)) {
      syms_.push_back(Elf_Sym());  // The symbol table always has to start with NULL symbol.
    }

    // Buffer symbol for this section.  It will be written later.
    // If the symbol's section is null, it will be considered absolute (SHN_ABS).
    // (we use this in JIT to reference code which is stored outside the debug ELF file)
    void Add(Elf_Word name,
             const Section* section,
             Elf_Addr addr,
             Elf_Word size,
             uint8_t binding,
             uint8_t type) {
      Elf_Word section_index;
      if (section != nullptr) {
        DCHECK_LE(section->GetAddress(), addr);
        DCHECK_LE(addr, section->GetAddress() + section->header_.sh_size);
        section_index = section->GetSectionIndex();
      } else {
        section_index = static_cast<Elf_Word>(SHN_ABS);
      }
      Add(name, section_index, addr, size, binding, type);
    }

    // Buffer symbol for this section.  It will be written later.
    void Add(Elf_Word name,
             Elf_Word section_index,
             Elf_Addr addr,
             Elf_Word size,
             uint8_t binding,
             uint8_t type) {
      Elf_Sym sym = Elf_Sym();
      sym.st_name = name;
      sym.st_value = addr;
      sym.st_size = size;
      sym.st_other = 0;
      sym.st_shndx = section_index;
      sym.st_info = (binding << 4) + (type & 0xf);
      syms_.push_back(sym);

      // The sh_info file must be set to index one-past the last local symbol.
      if (binding == STB_LOCAL) {
        this->header_.sh_info = syms_.size();
      }
    }

    Elf_Word GetCacheSize() { return syms_.size() * sizeof(Elf_Sym); }

    void WriteCachedSection() {
      this->Start();
      this->WriteFully(syms_.data(), syms_.size() * sizeof(Elf_Sym));
      this->End();
    }

   private:
    std::vector<Elf_Sym> syms_;  // Buffered/cached content of the whole section.
  };

  class AbiflagsSection FINAL : public Section {
   public:
    // Section with Mips abiflag info.
    static constexpr uint8_t MIPS_AFL_REG_NONE =         0;  // no registers
    static constexpr uint8_t MIPS_AFL_REG_32 =           1;  // 32-bit registers
    static constexpr uint8_t MIPS_AFL_REG_64 =           2;  // 64-bit registers
    static constexpr uint32_t MIPS_AFL_FLAGS1_ODDSPREG = 1;  // Uses odd single-prec fp regs
    static constexpr uint8_t MIPS_ABI_FP_DOUBLE =        1;  // -mdouble-float
    static constexpr uint8_t MIPS_ABI_FP_XX =            5;  // -mfpxx
    static constexpr uint8_t MIPS_ABI_FP_64A =           7;  // -mips32r* -mfp64 -mno-odd-spreg

    AbiflagsSection(ElfBuilder<ElfTypes>* owner,
                    const std::string& name,
                    Elf_Word type,
                    Elf_Word flags,
                    const Section* link,
                    Elf_Word info,
                    Elf_Word align,
                    Elf_Word entsize,
                    InstructionSet isa,
                    const InstructionSetFeatures* features)
        : Section(owner, name, type, flags, link, info, align, entsize) {
      if (isa == InstructionSet::kMips || isa == InstructionSet::kMips64) {
        bool fpu32 = false;    // assume mips64 values
        uint8_t isa_rev = 6;   // assume mips64 values
        if (isa == InstructionSet::kMips) {
          // adjust for mips32 values
          fpu32 = features->AsMipsInstructionSetFeatures()->Is32BitFloatingPoint();
          isa_rev = features->AsMipsInstructionSetFeatures()->IsR6()
              ? 6
              : features->AsMipsInstructionSetFeatures()->IsMipsIsaRevGreaterThanEqual2()
                  ? (fpu32 ? 2 : 5)
                  : 1;
        }
        abiflags_.version = 0;  // version of flags structure
        abiflags_.isa_level = (isa == InstructionSet::kMips) ? 32 : 64;
        abiflags_.isa_rev = isa_rev;
        abiflags_.gpr_size = (isa == InstructionSet::kMips) ? MIPS_AFL_REG_32 : MIPS_AFL_REG_64;
        abiflags_.cpr1_size = fpu32 ? MIPS_AFL_REG_32 : MIPS_AFL_REG_64;
        abiflags_.cpr2_size = MIPS_AFL_REG_NONE;
        // Set the fp_abi to MIPS_ABI_FP_64A for mips32 with 64-bit FPUs (ie: mips32 R5 and R6).
        // Otherwise set to MIPS_ABI_FP_DOUBLE.
        abiflags_.fp_abi =
            (isa == InstructionSet::kMips && !fpu32) ? MIPS_ABI_FP_64A : MIPS_ABI_FP_DOUBLE;
        abiflags_.isa_ext = 0;
        abiflags_.ases = 0;
        // To keep the code simple, we are not using odd FP reg for single floats for both
        // mips32 and mips64 ART. Therefore we are not setting the MIPS_AFL_FLAGS1_ODDSPREG bit.
        abiflags_.flags1 = 0;
        abiflags_.flags2 = 0;
      }
    }

    Elf_Word GetSize() const {
      return sizeof(abiflags_);
    }

    void Write() {
      this->WriteFully(&abiflags_, sizeof(abiflags_));
    }

   private:
    struct {
      uint16_t version;  // version of this structure
      uint8_t  isa_level, isa_rev, gpr_size, cpr1_size, cpr2_size;
      uint8_t  fp_abi;
      uint32_t isa_ext, ases, flags1, flags2;
    } abiflags_;
  };

  class BuildIdSection FINAL : public Section {
   public:
    BuildIdSection(ElfBuilder<ElfTypes>* owner,
                   const std::string& name,
                   Elf_Word type,
                   Elf_Word flags,
                   const Section* link,
                   Elf_Word info,
                   Elf_Word align,
                   Elf_Word entsize)
        : Section(owner, name, type, flags, link, info, align, entsize),
          digest_start_(-1) {
    }

    Elf_Word GetSize() {
      return 16 + kBuildIdLen;
    }

    void Write() {
      // The size fields are 32-bit on both 32-bit and 64-bit systems, confirmed
      // with the 64-bit linker and libbfd code. The size of name and desc must
      // be a multiple of 4 and it currently is.
      this->WriteUint32(4);  // namesz.
      this->WriteUint32(kBuildIdLen);  // descsz.
      this->WriteUint32(3);  // type = NT_GNU_BUILD_ID.
      this->WriteFully("GNU", 4);  // name.
      digest_start_ = this->Seek(0, kSeekCurrent);
      static_assert(kBuildIdLen % 4 == 0, "expecting a mutliple of 4 for build ID length");
      this->WriteFully(std::string(kBuildIdLen, '\0').c_str(), kBuildIdLen);  // desc.
      DCHECK_EQ(this->GetPosition(), GetSize());
    }

    off_t GetDigestStart() {
      CHECK_GT(digest_start_, 0);
      return digest_start_;
    }

   private:
    bool WriteUint32(uint32_t v) {
      return this->WriteFully(&v, sizeof(v));
    }

    // File offset where the build ID digest starts.
    // Populated with zeros first, then updated with the actual value as the
    // very last thing in the output file creation.
    off_t digest_start_;
  };

  ElfBuilder(InstructionSet isa, const InstructionSetFeatures* features, OutputStream* output)
      : isa_(isa),
        features_(features),
        stream_(output),
        rodata_(this, ".rodata", SHT_PROGBITS, SHF_ALLOC, nullptr, 0, kPageSize, 0),
        text_(this, ".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, nullptr, 0, kPageSize, 0),
        bss_(this, ".bss", SHT_NOBITS, SHF_ALLOC, nullptr, 0, kPageSize, 0),
        dex_(this, ".dex", SHT_NOBITS, SHF_ALLOC, nullptr, 0, kPageSize, 0),
        dynstr_(this, ".dynstr", SHF_ALLOC, kPageSize),
        dynsym_(this, ".dynsym", SHT_DYNSYM, SHF_ALLOC, &dynstr_),
        hash_(this, ".hash", SHT_HASH, SHF_ALLOC, &dynsym_, 0, sizeof(Elf_Word), sizeof(Elf_Word)),
        dynamic_(this, ".dynamic", SHT_DYNAMIC, SHF_ALLOC, &dynstr_, 0, kPageSize, sizeof(Elf_Dyn)),
        eh_frame_(this, ".eh_frame", SHT_PROGBITS, SHF_ALLOC, nullptr, 0, kPageSize, 0),
        eh_frame_hdr_(this, ".eh_frame_hdr", SHT_PROGBITS, SHF_ALLOC, nullptr, 0, 4, 0),
        strtab_(this, ".strtab", 0, 1),
        symtab_(this, ".symtab", SHT_SYMTAB, 0, &strtab_),
        debug_frame_(this, ".debug_frame", SHT_PROGBITS, 0, nullptr, 0, sizeof(Elf_Addr), 0),
        debug_info_(this, ".debug_info", SHT_PROGBITS, 0, nullptr, 0, 1, 0),
        debug_line_(this, ".debug_line", SHT_PROGBITS, 0, nullptr, 0, 1, 0),
        shstrtab_(this, ".shstrtab", 0, 1),
        abiflags_(this, ".MIPS.abiflags", SHT_MIPS_ABIFLAGS, SHF_ALLOC, nullptr, 0, kPageSize, 0,
                  isa, features),
        build_id_(this, ".note.gnu.build-id", SHT_NOTE, SHF_ALLOC, nullptr, 0, 4, 0),
        current_section_(nullptr),
        started_(false),
        write_program_headers_(false),
        loaded_size_(0u),
        virtual_address_(0) {
    text_.phdr_flags_ = PF_R | PF_X;
    bss_.phdr_flags_ = PF_R | PF_W;
    dex_.phdr_flags_ = PF_R;
    dynamic_.phdr_flags_ = PF_R | PF_W;
    dynamic_.phdr_type_ = PT_DYNAMIC;
    eh_frame_hdr_.phdr_type_ = PT_GNU_EH_FRAME;
    abiflags_.phdr_type_ = PT_MIPS_ABIFLAGS;
    build_id_.phdr_type_ = PT_NOTE;
  }
  ~ElfBuilder() {}

  InstructionSet GetIsa() { return isa_; }
  BuildIdSection* GetBuildId() { return &build_id_; }
  Section* GetRoData() { return &rodata_; }
  Section* GetText() { return &text_; }
  Section* GetBss() { return &bss_; }
  Section* GetDex() { return &dex_; }
  StringSection* GetStrTab() { return &strtab_; }
  SymbolSection* GetSymTab() { return &symtab_; }
  Section* GetEhFrame() { return &eh_frame_; }
  Section* GetEhFrameHdr() { return &eh_frame_hdr_; }
  Section* GetDebugFrame() { return &debug_frame_; }
  Section* GetDebugInfo() { return &debug_info_; }
  Section* GetDebugLine() { return &debug_line_; }

  // Encode patch locations as LEB128 list of deltas between consecutive addresses.
  // (exposed publicly for tests)
  static void EncodeOatPatches(const ArrayRef<const uintptr_t>& locations,
                               std::vector<uint8_t>* buffer) {
    buffer->reserve(buffer->size() + locations.size() * 2);  // guess 2 bytes per ULEB128.
    uintptr_t address = 0;  // relative to start of section.
    for (uintptr_t location : locations) {
      DCHECK_GE(location, address) << "Patch locations are not in sorted order";
      EncodeUnsignedLeb128(buffer, dchecked_integral_cast<uint32_t>(location - address));
      address = location;
    }
  }

  void WritePatches(const char* name, const ArrayRef<const uintptr_t>& patch_locations) {
    std::vector<uint8_t> buffer;
    EncodeOatPatches(patch_locations, &buffer);
    std::unique_ptr<Section> s(new Section(this, name, SHT_OAT_PATCH, 0, nullptr, 0, 1, 0));
    s->Start();
    s->WriteFully(buffer.data(), buffer.size());
    s->End();
    other_sections_.push_back(std::move(s));
  }

  void WriteSection(const char* name, const std::vector<uint8_t>* buffer) {
    std::unique_ptr<Section> s(new Section(this, name, SHT_PROGBITS, 0, nullptr, 0, 1, 0));
    s->Start();
    s->WriteFully(buffer->data(), buffer->size());
    s->End();
    other_sections_.push_back(std::move(s));
  }

  // Reserve space for ELF header and program headers.
  // We do not know the number of headers until later, so
  // it is easiest to just reserve a fixed amount of space.
  // Program headers are required for loading by the linker.
  // It is possible to omit them for ELF files used for debugging.
  void Start(bool write_program_headers = true) {
    int size = sizeof(Elf_Ehdr);
    if (write_program_headers) {
      size += sizeof(Elf_Phdr) * kMaxProgramHeaders;
    }
    stream_.Seek(size, kSeekSet);
    started_ = true;
    virtual_address_ += size;
    write_program_headers_ = write_program_headers;
  }

  void End() {
    DCHECK(started_);

    // Note: loaded_size_ == 0 for tests that don't write .rodata, .text, .bss,
    // .dynstr, dynsym, .hash and .dynamic. These tests should not read loaded_size_.
    // TODO: Either refactor the .eh_frame creation so that it counts towards loaded_size_,
    // or remove all support for .eh_frame. (The currently unused .eh_frame counts towards
    // the virtual_address_ but we don't consider it for loaded_size_.)
    CHECK(loaded_size_ == 0 || loaded_size_ == RoundUp(virtual_address_, kPageSize))
        << loaded_size_ << " " << virtual_address_;

    // Write section names and finish the section headers.
    shstrtab_.Start();
    shstrtab_.Write("");
    for (auto* section : sections_) {
      section->header_.sh_name = shstrtab_.Write(section->name_);
      if (section->link_ != nullptr) {
        section->header_.sh_link = section->link_->GetSectionIndex();
      }
      if (section->header_.sh_offset == 0) {
        section->header_.sh_type = SHT_NOBITS;
      }
    }
    shstrtab_.End();

    // Write section headers at the end of the ELF file.
    std::vector<Elf_Shdr> shdrs;
    shdrs.reserve(1u + sections_.size());
    shdrs.push_back(Elf_Shdr());  // NULL at index 0.
    for (auto* section : sections_) {
      shdrs.push_back(section->header_);
    }
    Elf_Off section_headers_offset;
    section_headers_offset = AlignFileOffset(sizeof(Elf_Off));
    stream_.WriteFully(shdrs.data(), shdrs.size() * sizeof(shdrs[0]));

    // Flush everything else before writing the program headers. This should prevent
    // the OS from reordering writes, so that we don't end up with valid headers
    // and partially written data if we suddenly lose power, for example.
    stream_.Flush();

    // The main ELF header.
    Elf_Ehdr elf_header = MakeElfHeader(isa_, features_);
    elf_header.e_shoff = section_headers_offset;
    elf_header.e_shnum = shdrs.size();
    elf_header.e_shstrndx = shstrtab_.GetSectionIndex();

    // Program headers (i.e. mmap instructions).
    std::vector<Elf_Phdr> phdrs;
    if (write_program_headers_) {
      phdrs = MakeProgramHeaders();
      CHECK_LE(phdrs.size(), kMaxProgramHeaders);
      elf_header.e_phoff = sizeof(Elf_Ehdr);
      elf_header.e_phnum = phdrs.size();
    }

    stream_.Seek(0, kSeekSet);
    stream_.WriteFully(&elf_header, sizeof(elf_header));
    stream_.WriteFully(phdrs.data(), phdrs.size() * sizeof(phdrs[0]));
    stream_.Flush();
  }

  // The running program does not have access to section headers
  // and the loader is not supposed to use them either.
  // The dynamic sections therefore replicates some of the layout
  // information like the address and size of .rodata and .text.
  // It also contains other metadata like the SONAME.
  // The .dynamic section is found using the PT_DYNAMIC program header.
  void PrepareDynamicSection(const std::string& elf_file_path,
                             Elf_Word rodata_size,
                             Elf_Word text_size,
                             Elf_Word bss_size,
                             Elf_Word bss_methods_offset,
                             Elf_Word bss_roots_offset,
                             Elf_Word dex_size) {
    std::string soname(elf_file_path);
    size_t directory_separator_pos = soname.rfind('/');
    if (directory_separator_pos != std::string::npos) {
      soname = soname.substr(directory_separator_pos + 1);
    }

    // Allocate all pre-dynamic sections.
    rodata_.AllocateVirtualMemory(rodata_size);
    text_.AllocateVirtualMemory(text_size);
    if (bss_size != 0) {
      bss_.AllocateVirtualMemory(bss_size);
    }
    if (dex_size != 0) {
      dex_.AllocateVirtualMemory(dex_size);
    }
    if (isa_ == InstructionSet::kMips || isa_ == InstructionSet::kMips64) {
      abiflags_.AllocateVirtualMemory(abiflags_.GetSize());
    }

    // Cache .dynstr, .dynsym and .hash data.
    dynstr_.Add("");  // dynstr should start with empty string.
    Elf_Word oatdata = dynstr_.Add("oatdata");
    dynsym_.Add(oatdata, &rodata_, rodata_.GetAddress(), rodata_size, STB_GLOBAL, STT_OBJECT);
    if (text_size != 0u) {
      // The runtime does not care about the size of this symbol (it uses the "lastword" symbol).
      // We use size 0 (meaning "unknown size" in ELF) to prevent overlap with the debug symbols.
      Elf_Word oatexec = dynstr_.Add("oatexec");
      dynsym_.Add(oatexec, &text_, text_.GetAddress(), /* size */ 0, STB_GLOBAL, STT_OBJECT);
      Elf_Word oatlastword = dynstr_.Add("oatlastword");
      Elf_Word oatlastword_address = text_.GetAddress() + text_size - 4;
      dynsym_.Add(oatlastword, &text_, oatlastword_address, 4, STB_GLOBAL, STT_OBJECT);
    } else if (rodata_size != 0) {
      // rodata_ can be size 0 for dwarf_test.
      Elf_Word oatlastword = dynstr_.Add("oatlastword");
      Elf_Word oatlastword_address = rodata_.GetAddress() + rodata_size - 4;
      dynsym_.Add(oatlastword, &rodata_, oatlastword_address, 4, STB_GLOBAL, STT_OBJECT);
    }
    DCHECK_LE(bss_roots_offset, bss_size);
    if (bss_size != 0u) {
      Elf_Word oatbss = dynstr_.Add("oatbss");
      dynsym_.Add(oatbss, &bss_, bss_.GetAddress(), bss_roots_offset, STB_GLOBAL, STT_OBJECT);
      DCHECK_LE(bss_methods_offset, bss_roots_offset);
      DCHECK_LE(bss_roots_offset, bss_size);
      // Add a symbol marking the start of the methods part of the .bss, if not empty.
      if (bss_methods_offset != bss_roots_offset) {
        Elf_Word bss_methods_address = bss_.GetAddress() + bss_methods_offset;
        Elf_Word bss_methods_size = bss_roots_offset - bss_methods_offset;
        Elf_Word oatbssroots = dynstr_.Add("oatbssmethods");
        dynsym_.Add(
            oatbssroots, &bss_, bss_methods_address, bss_methods_size, STB_GLOBAL, STT_OBJECT);
      }
      // Add a symbol marking the start of the GC roots part of the .bss, if not empty.
      if (bss_roots_offset != bss_size) {
        Elf_Word bss_roots_address = bss_.GetAddress() + bss_roots_offset;
        Elf_Word bss_roots_size = bss_size - bss_roots_offset;
        Elf_Word oatbssroots = dynstr_.Add("oatbssroots");
        dynsym_.Add(
            oatbssroots, &bss_, bss_roots_address, bss_roots_size, STB_GLOBAL, STT_OBJECT);
      }
      Elf_Word oatbsslastword = dynstr_.Add("oatbsslastword");
      Elf_Word bsslastword_address = bss_.GetAddress() + bss_size - 4;
      dynsym_.Add(oatbsslastword, &bss_, bsslastword_address, 4, STB_GLOBAL, STT_OBJECT);
    }
    if (dex_size != 0u) {
      Elf_Word oatdex = dynstr_.Add("oatdex");
      dynsym_.Add(oatdex, &dex_, dex_.GetAddress(), /* size */ 0, STB_GLOBAL, STT_OBJECT);
      Elf_Word oatdexlastword = dynstr_.Add("oatdexlastword");
      Elf_Word oatdexlastword_address = dex_.GetAddress() + dex_size - 4;
      dynsym_.Add(oatdexlastword, &dex_, oatdexlastword_address, 4, STB_GLOBAL, STT_OBJECT);
    }

    Elf_Word soname_offset = dynstr_.Add(soname);

    // We do not really need a hash-table since there is so few entries.
    // However, the hash-table is the only way the linker can actually
    // determine the number of symbols in .dynsym so it is required.
    int count = dynsym_.GetCacheSize() / sizeof(Elf_Sym);  // Includes NULL.
    std::vector<Elf_Word> hash;
    hash.push_back(1);  // Number of buckets.
    hash.push_back(count);  // Number of chains.
    // Buckets.  Having just one makes it linear search.
    hash.push_back(1);  // Point to first non-NULL symbol.
    // Chains.  This creates linked list of symbols.
    hash.push_back(0);  // Dummy entry for the NULL symbol.
    for (int i = 1; i < count - 1; i++) {
      hash.push_back(i + 1);  // Each symbol points to the next one.
    }
    hash.push_back(0);  // Last symbol terminates the chain.
    hash_.Add(hash.data(), hash.size() * sizeof(hash[0]));

    // Allocate all remaining sections.
    dynstr_.AllocateVirtualMemory(dynstr_.GetCacheSize());
    dynsym_.AllocateVirtualMemory(dynsym_.GetCacheSize());
    hash_.AllocateVirtualMemory(hash_.GetCacheSize());

    Elf_Dyn dyns[] = {
      { DT_HASH, { hash_.GetAddress() } },
      { DT_STRTAB, { dynstr_.GetAddress() } },
      { DT_SYMTAB, { dynsym_.GetAddress() } },
      { DT_SYMENT, { sizeof(Elf_Sym) } },
      { DT_STRSZ, { dynstr_.GetCacheSize() } },
      { DT_SONAME, { soname_offset } },
      { DT_NULL, { 0 } },
    };
    dynamic_.Add(&dyns, sizeof(dyns));
    dynamic_.AllocateVirtualMemory(dynamic_.GetCacheSize());

    loaded_size_ = RoundUp(virtual_address_, kPageSize);
  }

  void WriteDynamicSection() {
    dynstr_.WriteCachedSection();
    dynsym_.WriteCachedSection();
    hash_.WriteCachedSection();
    dynamic_.WriteCachedSection();
  }

  Elf_Word GetLoadedSize() {
    CHECK_NE(loaded_size_, 0u);
    return loaded_size_;
  }

  void WriteMIPSabiflagsSection() {
    abiflags_.Start();
    abiflags_.Write();
    abiflags_.End();
  }

  void WriteBuildIdSection() {
    build_id_.Start();
    build_id_.Write();
    build_id_.End();
  }

  void WriteBuildId(uint8_t build_id[kBuildIdLen]) {
    stream_.Seek(build_id_.GetDigestStart(), kSeekSet);
    stream_.WriteFully(build_id, kBuildIdLen);
  }

  // Returns true if all writes and seeks on the output stream succeeded.
  bool Good() {
    return stream_.Good();
  }

  // Returns the builder's internal stream.
  OutputStream* GetStream() {
    return &stream_;
  }

  off_t AlignFileOffset(size_t alignment) {
     return stream_.Seek(RoundUp(stream_.Seek(0, kSeekCurrent), alignment), kSeekSet);
  }

 private:
  static Elf_Ehdr MakeElfHeader(InstructionSet isa, const InstructionSetFeatures* features) {
    Elf_Ehdr elf_header = Elf_Ehdr();
    switch (isa) {
      case InstructionSet::kArm:
        // Fall through.
      case InstructionSet::kThumb2: {
        elf_header.e_machine = EM_ARM;
        elf_header.e_flags = EF_ARM_EABI_VER5;
        break;
      }
      case InstructionSet::kArm64: {
        elf_header.e_machine = EM_AARCH64;
        elf_header.e_flags = 0;
        break;
      }
      case InstructionSet::kX86: {
        elf_header.e_machine = EM_386;
        elf_header.e_flags = 0;
        break;
      }
      case InstructionSet::kX86_64: {
        elf_header.e_machine = EM_X86_64;
        elf_header.e_flags = 0;
        break;
      }
      case InstructionSet::kMips: {
        elf_header.e_machine = EM_MIPS;
        elf_header.e_flags = (EF_MIPS_NOREORDER |
                              EF_MIPS_PIC       |
                              EF_MIPS_CPIC      |
                              EF_MIPS_ABI_O32   |
                              (features->AsMipsInstructionSetFeatures()->IsR6()
                                   ? EF_MIPS_ARCH_32R6
                                   : EF_MIPS_ARCH_32R2));
        break;
      }
      case InstructionSet::kMips64: {
        elf_header.e_machine = EM_MIPS;
        elf_header.e_flags = (EF_MIPS_NOREORDER |
                              EF_MIPS_PIC       |
                              EF_MIPS_CPIC      |
                              EF_MIPS_ARCH_64R6);
        break;
      }
      case InstructionSet::kNone: {
        LOG(FATAL) << "No instruction set";
        break;
      }
      default: {
        LOG(FATAL) << "Unknown instruction set " << isa;
      }
    }

    elf_header.e_ident[EI_MAG0]       = ELFMAG0;
    elf_header.e_ident[EI_MAG1]       = ELFMAG1;
    elf_header.e_ident[EI_MAG2]       = ELFMAG2;
    elf_header.e_ident[EI_MAG3]       = ELFMAG3;
    elf_header.e_ident[EI_CLASS]      = (sizeof(Elf_Addr) == sizeof(Elf32_Addr))
                                         ? ELFCLASS32 : ELFCLASS64;
    elf_header.e_ident[EI_DATA]       = ELFDATA2LSB;
    elf_header.e_ident[EI_VERSION]    = EV_CURRENT;
    elf_header.e_ident[EI_OSABI]      = ELFOSABI_LINUX;
    elf_header.e_ident[EI_ABIVERSION] = 0;
    elf_header.e_type = ET_DYN;
    elf_header.e_version = 1;
    elf_header.e_entry = 0;
    elf_header.e_ehsize = sizeof(Elf_Ehdr);
    elf_header.e_phentsize = sizeof(Elf_Phdr);
    elf_header.e_shentsize = sizeof(Elf_Shdr);
    return elf_header;
  }

  // Create program headers based on written sections.
  std::vector<Elf_Phdr> MakeProgramHeaders() {
    CHECK(!sections_.empty());
    std::vector<Elf_Phdr> phdrs;
    {
      // The program headers must start with PT_PHDR which is used in
      // loaded process to determine the number of program headers.
      Elf_Phdr phdr = Elf_Phdr();
      phdr.p_type    = PT_PHDR;
      phdr.p_flags   = PF_R;
      phdr.p_offset  = phdr.p_vaddr = phdr.p_paddr = sizeof(Elf_Ehdr);
      phdr.p_filesz  = phdr.p_memsz = 0;  // We need to fill this later.
      phdr.p_align   = sizeof(Elf_Off);
      phdrs.push_back(phdr);
      // Tell the linker to mmap the start of file to memory.
      Elf_Phdr load = Elf_Phdr();
      load.p_type    = PT_LOAD;
      load.p_flags   = PF_R;
      load.p_offset  = load.p_vaddr = load.p_paddr = 0;
      load.p_filesz  = load.p_memsz = sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * kMaxProgramHeaders;
      load.p_align   = kPageSize;
      phdrs.push_back(load);
    }
    // Create program headers for sections.
    for (auto* section : sections_) {
      const Elf_Shdr& shdr = section->header_;
      if ((shdr.sh_flags & SHF_ALLOC) != 0 && shdr.sh_size != 0) {
        DCHECK(shdr.sh_addr != 0u) << "Allocate virtual memory for the section";
        // PT_LOAD tells the linker to mmap part of the file.
        // The linker can only mmap page-aligned sections.
        // Single PT_LOAD may contain several ELF sections.
        Elf_Phdr& prev = phdrs.back();
        Elf_Phdr load = Elf_Phdr();
        load.p_type   = PT_LOAD;
        load.p_flags  = section->phdr_flags_;
        load.p_offset = shdr.sh_offset;
        load.p_vaddr  = load.p_paddr = shdr.sh_addr;
        load.p_filesz = (shdr.sh_type != SHT_NOBITS ? shdr.sh_size : 0u);
        load.p_memsz  = shdr.sh_size;
        load.p_align  = shdr.sh_addralign;
        if (prev.p_type == load.p_type &&
            prev.p_flags == load.p_flags &&
            prev.p_filesz == prev.p_memsz &&  // Do not merge .bss
            load.p_filesz == load.p_memsz) {  // Do not merge .bss
          // Merge this PT_LOAD with the previous one.
          Elf_Word size = shdr.sh_offset + shdr.sh_size - prev.p_offset;
          prev.p_filesz = size;
          prev.p_memsz  = size;
        } else {
          // If we are adding new load, it must be aligned.
          CHECK_EQ(shdr.sh_addralign, (Elf_Word)kPageSize);
          phdrs.push_back(load);
        }
      }
    }
    for (auto* section : sections_) {
      const Elf_Shdr& shdr = section->header_;
      if ((shdr.sh_flags & SHF_ALLOC) != 0 && shdr.sh_size != 0) {
        // Other PT_* types allow the program to locate interesting
        // parts of memory at runtime. They must overlap with PT_LOAD.
        if (section->phdr_type_ != 0) {
          Elf_Phdr phdr = Elf_Phdr();
          phdr.p_type   = section->phdr_type_;
          phdr.p_flags  = section->phdr_flags_;
          phdr.p_offset = shdr.sh_offset;
          phdr.p_vaddr  = phdr.p_paddr = shdr.sh_addr;
          phdr.p_filesz = phdr.p_memsz = shdr.sh_size;
          phdr.p_align  = shdr.sh_addralign;
          phdrs.push_back(phdr);
        }
      }
    }
    // Set the size of the initial PT_PHDR.
    CHECK_EQ(phdrs[0].p_type, (Elf_Word)PT_PHDR);
    phdrs[0].p_filesz = phdrs[0].p_memsz = phdrs.size() * sizeof(Elf_Phdr);

    return phdrs;
  }

  InstructionSet isa_;
  const InstructionSetFeatures* features_;

  ErrorDelayingOutputStream stream_;

  Section rodata_;
  Section text_;
  Section bss_;
  Section dex_;
  CachedStringSection dynstr_;
  SymbolSection dynsym_;
  CachedSection hash_;
  CachedSection dynamic_;
  Section eh_frame_;
  Section eh_frame_hdr_;
  StringSection strtab_;
  SymbolSection symtab_;
  Section debug_frame_;
  Section debug_info_;
  Section debug_line_;
  StringSection shstrtab_;
  AbiflagsSection abiflags_;
  BuildIdSection build_id_;
  std::vector<std::unique_ptr<Section>> other_sections_;

  // List of used section in the order in which they were written.
  std::vector<Section*> sections_;
  Section* current_section_;  // The section which is currently being written.

  bool started_;
  bool write_program_headers_;

  // The size of the memory taken by the ELF file when loaded.
  size_t loaded_size_;

  // Used for allocation of virtual address space.
  Elf_Addr virtual_address_;

  DISALLOW_COPY_AND_ASSIGN(ElfBuilder);
};

}  // namespace linker
}  // namespace art

#endif  // ART_COMPILER_LINKER_ELF_BUILDER_H_
