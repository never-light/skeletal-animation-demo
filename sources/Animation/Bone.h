#pragma once

#include <string>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

struct Bone {
  Bone() = default;

  Bone(uint8_t parentId, const glm::mat4& inverseBindPoseMatrix)
    : parentId(parentId), inverseBindPoseMatrix(inverseBindPoseMatrix)
  {

  }

  uint8_t parentId;
  glm::mat4 inverseBindPoseMatrix;

  static constexpr uint8_t ROOT_BONE_PARENT_ID = 255;
};

struct BonePose {
  BonePose() = default;

  BonePose(const glm::vec3& position, const glm::quat& orientation)
    : position(position), orientation(orientation)
  {
  }

  [[nodiscard]] glm::mat4 getBoneMatrix() const
  {
    return glm::translate(glm::identity<glm::mat4>(), position) * glm::mat4_cast(orientation);
  }

  glm::vec3 position = glm::vec3(0.0f);
  glm::quat orientation = glm::identity<glm::quat>();
};
