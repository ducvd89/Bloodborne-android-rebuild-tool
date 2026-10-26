# SPDX-License-Identifier: GPL-3.0-or-later
# Included by FEX's project() (CMAKE_PROJECT_FEX_INCLUDE, see ../CMakeLists.txt): bb-recomp is added to
# FEX's build once all of FEX's targets exist.
# FEX's platform check (CMakeLists.txt, "Platform Checks") accepts Linux and Windows only. Bionic is Linux for FEX (the same kernel interface), and the NDK
# toolchain of arm64/build.sh android says Android: the tree is not patched, the name is set back here (ANDROID stays set for bbrecomp.cmake).
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(CMAKE_SYSTEM_NAME Linux)
endif()
cmake_language(EVAL CODE "cmake_language(DEFER DIRECTORY \"${CMAKE_SOURCE_DIR}\" CALL include \"${CMAKE_CURRENT_LIST_DIR}/bbrecomp.cmake\")")
