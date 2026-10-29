// SPDX-License-Identifier: GPL-3.0-or-later
// The tool version (CMake passes -DBBR_TOOL_VERSION=<BB_RECOMP_VERSION>; this is the fallback of the other builds) and the ABI version of the .so it writes.
#pragma once
#ifndef BBR_TOOL_VERSION
#define BBR_TOOL_VERSION "1.3.0"
#endif
#ifndef BBR_GIT_HASH
#define BBR_GIT_HASH ""
#endif
// Bumped by hand whenever the code the tool generates for a given input changes: the lifter (FEX), the lowering, the optimiser/codegen settings, the LLVM
// version, the naming of symbols. It is what --resume keys and the build manifest are tied to (docs/COMPILER.md, "Compiler version"); the tool version is not.
#define BBR_COMPILER_VERSION "1"
constexpr unsigned kBbrAbiVersion = 2; // bbr_abi_version in the table object (compile.cpp), abi_version of libbbaot.json, and what the runtime (libbbcpu) must accept (== 2). 2: the layout of 1; bumped in tool 1.2.0 to force a rebuild with the fixes (docs/RECOMP_ABI.md)
