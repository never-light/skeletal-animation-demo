#pragma once

#include <vector>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

struct BoneAnimationPositionFrame {
  float time;
  glm::vec3 position;
};

struct BoneAnimationOrientationFrame {
  float time;
  glm::quat orientation;
};

struct BoneAnimationChannel {
  std::vector<BoneAnimationPositionFrame> positionFrames;
  std::vector<BoneAnimationOrientationFrame> orientationFrames;
};
