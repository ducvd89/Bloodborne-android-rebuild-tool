// SPDX-License-Identifier: GPL-3.0-or-later
// PS4 ELF (e_type 0xfe10) loader for bb-recomp: maps the load segments into host memory.
//
// FEX decodes guest code from host memory at the guest RIP (the Decoder reads *(uint8_t*)RIP), so the
// image has to live at the address the IR will name. Vaddr 0 cannot be mapped (mmap_min_addr), so the
// image is mapped at an offset: guest address = base + vaddr. The IR therefore holds absolute
// addresses (rip-relative targets, branch targets) in that space; the lowering subtracts the base.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// The default lift base: the host address image vaddr 0 is mapped at while lifting. The lowering takes every 64-bit IR constant in [base, base + image size) for an
// image address (FEX folds rip-relative arithmetic into such constants) and relocates it to the runtime's image base, so a plain constant of the game that
// falls into the window would be relocated by mistake (with the old base 0x200000000 two packed 32-bit integers such as `movabs rax, 0x200000002` became
// image + 2 on the Thor). The base is hi << 32 (the low word of an entry-point offset is then the vaddr) and hi < 0x80, so that it also fits the 39-bit user
// address space of many Android kernels (user space ends at 0x8000000000). hi = 0x2b is the value for which Bloodborne 1.09 has no constant in the window:
// the lowering relocates exactly the same 1,866,280 constants as with the old base 0x3a7700000000, and the library is byte-identical (docs/NATIVE_CPU.md).
// (hi = 0x20, 0x23 and 0x26, for one, are inside plain constants: 7, 3 and 2 more constants would be relocated.)
#ifdef __APPLE__
// macOS on Apple Silicon reserves everything below 0x7000000000 (the dyld shared cache, then a reserved hole up to 0x7000000000), so none of
// the bases below can be mapped there; the old base 0x3a7700000000 can, and gives the byte-identical library (docs/NATIVE_CPU.md, "The lift base").
constexpr uint64_t kDefaultLiftBase = 0x3a7700000000ull;
constexpr uint64_t kFallbackLiftBases[] = {0x2b00000000ull, 0x3d00000000ull, 0x4700000000ull};
#else
constexpr uint64_t kDefaultLiftBase = 0x2b00000000ull;
// Bases to try when the default cannot be mapped (the address range is taken), see ElfImage::mapFailed. Each one was checked like the default.
constexpr uint64_t kFallbackLiftBases[] = {0x3d00000000ull, 0x4700000000ull};
#endif

struct ElfSegment {
  uint64_t vaddr, filesz, memsz, offset;
  uint32_t flags; // PF_X 1, PF_W 2, PF_R 4
};

// One module of the image: the eboot (base 0) or a linked system module (libc.prx ...). Module code lives at
// `base` + the module's own vaddr, so every address in the tool, the units and the table is relative to the one
// image base the runtime maps (the same layout as scripts/link_modules.py's boot-linked.bin).
struct ModuleInfo {
  std::string name;
  uint64_t base = 0, size = 0;                      // image-relative
  uint64_t ehFrameHdrVaddr = 0, ehFrameHdrSize = 0; // image-relative (PT_GNU_EH_FRAME of the module)
  uint64_t init = 0;                                // image-relative entry the runtime starts the module at (boot mode; 0: none)
};

// Imports and relocations of a boot-linked image: which GOT slot belongs to which import, and the native
// exports (libc.prx's) that satisfy them. Used to find setjmp-like functions by name.
struct BootLinks {
  std::vector<std::string> imports;                         // "NID#lib#mod"
  struct Binding { uint64_t index, address, kind; };        // import index -> export address in the image (kind 1 function, 2 data)
  std::vector<Binding> bindings;
  struct Reloc { uint64_t target, kind, value, addend; };   // kind 0: *target = image+value; 1: function import; 2: data import
  std::vector<Reloc> relocs;
};

struct ElfImage {
  std::vector<ElfSegment> loads;         // PT_LOAD
  uint64_t ehFrameHdrVaddr = 0, ehFrameHdrSize = 0; // PT_GNU_EH_FRAME
  uint64_t entry = 0;
  uint64_t base = 0;                     // host address of vaddr 0
  uint64_t textLo = 0, textHi = 0;       // executable segment [vaddr, vaddr+memsz)
  std::vector<uint8_t> file;             // the whole file (kept for the .eh_frame_hdr parser); boot mode: the flat image
  std::vector<ModuleInfo> modules;       // module 0 is the eboot; boot mode adds the linked modules
  BootLinks links;                       // boot mode only
  bool boot = false;
  bool mapFailed = false;                // load/loadBoot failed because the address range at `base` cannot be mapped (the caller may try another base)

  // Reads the file and maps every PT_LOAD at base + vaddr (anonymous memory, filled from the file).
  // The mapping stays for the life of the process. Returns false and sets err on failure.
  bool load(const std::string& path, uint64_t baseAddr, std::string& err);

  // Boot mode: the memory image the runtime builds (scripts/link_modules.py output, BBPROBE5: eboot + linked modules),
  // mapped at base + vaddr. `ebootElf` supplies the eboot's PT_GNU_EH_FRAME, the .prx files (in prxDir, named in
  // link.json next to the boot file, else libc.prx and libSceFios2.prx) theirs.
  bool loadBoot(const std::string& bootPath, const std::string& ebootElf, const std::string& prxDir, uint64_t baseAddr, std::string& err);
  const ModuleInfo* moduleOf(uint64_t vaddr) const;

  // Bytes of the file for a vaddr (inside a PT_LOAD), or nullptr.
  const uint8_t* fileBytes(uint64_t vaddr, uint64_t len) const;
};
