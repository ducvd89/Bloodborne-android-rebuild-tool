// SPDX-License-Identifier: GPL-3.0-or-later
// The check of a finished libbbaot.so and its libbbaot.json (format 1), shared by player mode and the command line (--game-dir): the app verifies the .so against the json.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct SoInfo {
  uint64_t imageHash = 0, unitCount = 0, funcCount = 0, abiFlags = 0, textEnd = 0, unitsSize = 0;
  uint32_t abiVersion = 0;
  std::vector<std::string> needed;
  uint64_t size = 0;
  std::string sha256;
};

// Reads the .so (an AArch64 shared object: it is never loaded here); returns "" when it is a well-formed bb-recomp library, else what is wrong with it.
std::string readSo(const std::string& path, uint64_t imageSize, SoInfo& out);
// FNV-1a over the flat image of a boot-linked file (bbr_image_hash) and the image size.
bool imageHashOfBoot(const std::string& boot, uint64_t& hash, uint64_t& imageSize);

struct GameMeta { std::string titleId = "?", version = "?", title = "?", contentId = "?"; };
// sce_sys/param.sfo of a game folder (TITLE_ID, APP_VER, TITLE, CONTENT_ID); "" when read, else the reason.
std::string readGameMeta(const std::string& gameDir, GameMeta& out);

// The json next to a .so: "" when written, else the reason.
std::string writeAotJson(const std::string& jsonPath, const GameMeta& meta, const SoInfo& so, uint64_t imageSize);
// readSo + the checks of player mode (ABI version, image hash) + writes the json; "" when ok, else the reason. `so` gets the .so's facts.
std::string checkAndWriteAotJson(const std::string& soPath, const std::string& bootPath, const GameMeta& meta, const std::string& jsonPath, SoInfo& so, uint64_t& imageSize);

std::string isoNow();
std::string hex16(uint64_t v);
std::string jsonStr(const std::string& s);
