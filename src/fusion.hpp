#pragma once
#include "mars.hpp"
namespace itl {
struct Fusion {std::vector<float> counts,ne;};
// Bounded, reusable storage: avoids allocating and clearing full images per frame.
struct FusionWorkspace {
 Fusion causal,candidate,result;
 std::array<std::vector<float>,8> delta;
 std::array<std::vector<uint8_t>,8> valid;
 std::vector<std::array<float,2>> energy;
};
const Fusion& causalCPU(const std::vector<const uint16_t*>&raw,const std::vector<Background>&align,float sigma,FusionWorkspace&workspace);
const Fusion& motionCPU(const std::vector<const uint16_t*>&raw,const std::vector<std::array<float,4>>&models,const std::vector<std::array<float,2>>&align,const Fusion&base,float sigma,FusionWorkspace&workspace);
Fusion causalCPU(const std::vector<const uint16_t*>&raw,const std::vector<Background>&align,float sigma);
Fusion motionCPU(const std::vector<const uint16_t*>&raw,const std::vector<std::array<float,4>>&models,const std::vector<std::array<float,2>>&align,const Fusion&base,float sigma);
}
