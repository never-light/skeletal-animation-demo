#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include <iterator>
#include <utility>

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Skeleton.h"
#include "BoneAnimationChannel.h"
#include "Bone.h"
#include "AnimationPose.h"

class AnimationClip {
 public:
  AnimationClip(
    const Skeleton& skeleton,
    float duration,
    float rate,
    std::vector<BoneAnimationChannel> bonesAnimationChannels)
    : m_skeleton(skeleton),
      m_bonesAnimationChannels(std::move(bonesAnimationChannels)),
      m_currentPose(skeleton, std::vector<BonePose>(skeleton.bones.size())),
      m_duration(duration),
      m_rate(rate)
  {

  }

  [[nodiscard]] const AnimationPose& getCurrentPose() const {
    m_currentPose.bonesLocalPoses[0] = getBoneLocalPose(0, m_currentTime);

    auto bonesCount = static_cast<uint8_t>(m_skeleton.bones.size());

    for (uint8_t boneIndex = 1; boneIndex < bonesCount; boneIndex++) {
      m_currentPose.bonesLocalPoses[boneIndex] = getBoneLocalPose(boneIndex, m_currentTime);
    }

    return m_currentPose;
  }

  void increaseCurrentTime(float delta) {
    m_currentTime += delta * m_rate;

    if (m_currentTime > m_duration) {
      int overflowParts = static_cast<int>(m_currentTime / m_duration);
      m_currentTime -= m_duration * static_cast<float>(overflowParts);
    }
  }

 private:
  [[nodiscard]] BonePose getBoneLocalPose(uint8_t boneIndex, float time) const
  {
    const std::vector<BoneAnimationPositionFrame>& positionFrames =
      m_bonesAnimationChannels[boneIndex].positionFrames;

    auto position = getMixedAdjacentFrames<glm::vec3, BoneAnimationPositionFrame>(positionFrames, time);

    const std::vector<BoneAnimationOrientationFrame>& orientationFrames =
      m_bonesAnimationChannels[boneIndex].orientationFrames;

    auto orientation = getMixedAdjacentFrames<glm::quat, BoneAnimationOrientationFrame>(orientationFrames, time);

    return BonePose(position, orientation);
  }

  template<class T, class S>
  [[nodiscard]] T getMixedAdjacentFrames(const std::vector<S>& frames, float time) const
  {
    S tempFrame;
    tempFrame.time = time;

    auto frameIt = std::upper_bound(frames.begin(), frames.end(),
      tempFrame, [](const S& a, const S& b) {
        return a.time < b.time;
      });

    if (frameIt == frames.end()) {
      return (frames.size() > 0) ? getKeyframeValue<T, S>(*frames.rbegin()) : getIdentity<T>();
    }
    else {
      T next = getKeyframeValue<T, S>(*frameIt);
      T prev = (frameIt == frames.begin()) ? getIdentity<T>() : getKeyframeValue<T, S>(*std::prev(frameIt));

      float currentFrameTime = frameIt->time;
      float prevFrameTime = (frameIt == frames.begin()) ? 0 : std::prev(frameIt)->time;

      float framesTimeDelta = currentFrameTime - prevFrameTime;

      return getInterpolatedValue<T>(prev, next, (time - prevFrameTime) / framesTimeDelta);
    }
  }

  template<class T>
  T getIdentity() const { assert(false); }

  template<class T, class S>
  T getKeyframeValue(const S& frame) const { assert(false); }

  template<class T>
  T getInterpolatedValue(const T& first, const T& second, float delta) const { assert(false); }

 private:
  Skeleton m_skeleton;
  std::vector<BoneAnimationChannel> m_bonesAnimationChannels;

  mutable AnimationPose m_currentPose;

  float m_currentTime = 0.0f;
  float m_duration = 0.0f;
  float m_rate = 0.0f;
};

template<>
glm::vec3 AnimationClip::getIdentity() const
{
  return glm::vec3(0.0f);
}

template<>
glm::quat AnimationClip::getIdentity() const
{
  return glm::identity<glm::quat>();
}

template<>
glm::vec3 AnimationClip::getKeyframeValue(const BoneAnimationPositionFrame& frame) const
{
  return frame.position;
}

template<>
glm::quat AnimationClip::getKeyframeValue(const BoneAnimationOrientationFrame& frame) const
{
  return frame.orientation;
}

template<>
glm::vec3 AnimationClip::getInterpolatedValue(const glm::vec3& first, const glm::vec3& second, float delta) const
{
  return glm::mix(first, second, delta);
}

template<>
glm::quat AnimationClip::getInterpolatedValue(const glm::quat& first, const glm::quat& second, float delta) const
{
  return glm::slerp(first, second, delta);
}
