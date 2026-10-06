// WAYS chunk (DVHikingGuide in the original): patrol routes for NPCs.
//
//   u32 version (1), u16 pathCount, per path: u16 waypointCount, per waypoint:
//     s16 x, s16 y, u16 sector, u16 ?, u8 isScript, u16 len, len bytes
//       isScript != 0: name of a script class
//       isScript == 0: orders: u16 1, u8 mode, u16 ?, u16 optionCount,
//                      optionCount * (u8 probability %, u16 ?), optionCount * (u16 len, commands)
//   commands: 0x02 u16 turn to direction, 0x05 s16 x, s16 y look at point, 0x07 u16 wait ticks,
//             0x81/0x82 f32, 0x83 u16 (unknown), 0x00 end
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct WayCommand {
    enum Op { Face = 0x02, LookAt = 0x05, Wait = 0x07 } op;
    int a = 0, b = 0;
};

struct WayOption {
    int probability = 100;
    std::vector<WayCommand> commands;
};

struct Waypoint {
    int x = 0, y = 0;
    int mode = 0;
    std::string script;
    std::vector<WayOption> options;
};

using PatrolPath = std::vector<Waypoint>;

bool parsePaths(const uint8_t* data, size_t size, std::vector<PatrolPath>& out);
