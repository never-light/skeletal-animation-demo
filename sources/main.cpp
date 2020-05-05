#define SDL_MAIN_HANDLED

#include <iostream>
#include <cassert>

#include <SDL.h>
#include <GL/gl3w.h>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Resources/Raw/RawSkeletalAnimationClip.h"
#include "Resources/Raw/RawSkeleton.h"
#include "Resources/Raw/RawMesh.h"
#include "Utils/files.h"
#include "Utils/memory.h"

#include "Animation/AnimationClip.h"

constexpr int FRAMES_PER_SECOND = 30;
constexpr int SKIP_TICKS = 1000 / FRAMES_PER_SECOND;

constexpr int WINDOW_WIDTH = 1024;
constexpr int WINDOW_HEIGHT = 768;

constexpr float g_FOVy = 60.0f;
constexpr float g_aspectRatio = static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT;
constexpr float g_nearClipDistance = 0.1f;
constexpr float g_farClipDistance = 100.0f;

static SDL_Window* g_window = nullptr;
static SDL_GLContext g_glContext = nullptr;

// OpenGL resources
static GLuint g_vertexShader = 0;
static GLuint g_fragmentShader = 0;

static GLuint g_shadersPipeline = 0;

static GLuint g_vertexBuffer = 0;
static GLuint g_indexBuffer = 0;
static GLuint g_vertexArrayObject = 0;

static size_t g_meshIndicesCount = 0;

static GLuint g_texture = 0;

static glm::mat4 g_viewMatrix;
static glm::mat4 g_projectionMatrix;
static glm::mat4 g_modelMatrix;

// Animation data
static std::unique_ptr<AnimationClip> g_animationClip = nullptr;

struct VertexPos3Norm3UVSkinned {
  glm::vec3 pos = {0.0f, 0.0f, 0.0f};
  glm::vec3 norm = {0.0f, 0.0f, 0.0f};
  glm::vec2 uv = {0.0f, 0.0f};
  glm::u8vec4 bonesIds = {0, 0, 0, 0};
  glm::u8vec4 bonesWeights = {0, 0, 0, 0};
};

static void initPlatform()
{
  int initStatus = SDL_Init(SDL_INIT_EVERYTHING);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  g_window = SDL_CreateWindow("Animation demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);

  g_glContext = SDL_GL_CreateContext(g_window);

  gl3wInit();
  assert(gl3wIsSupported(4, 5));
}

static void deinitPlatform()
{
  SDL_GL_DeleteContext(g_glContext);
  SDL_Quit();
}

static Skeleton loadSkeleton(const std::string& path)
{
  RawSkeleton rawSkeleton = RawSkeleton::readFromFile(path);
  std::vector<Bone> bones(rawSkeleton.bones.size());

  for (size_t boneIndex = 0; boneIndex < rawSkeleton.bones.size(); boneIndex++) {
    const RawBone& rawBone = rawSkeleton.bones[boneIndex];

    bones[boneIndex].parentId = rawBone.parentId;
    bones[boneIndex].inverseBindPoseMatrix = rawMatrix4ToGLMMatrix4(rawBone.inverseBindPoseMatrix);
  }

  return Skeleton{bones};
}

static AnimationClip loadAnimationClip(const Skeleton& skeleton, const std::string& path)
{
  // Read raw mesh
  RawSkeletalAnimationClip rawClip = RawSkeletalAnimationClip::readFromFile(path);

  // Convert raw animation clip to internal animation clip object
  const uint16_t skeletonBonesCount = rawClip.header.skeletonBonesCount;

  std::vector<BoneAnimationChannel> animationChannels;
  animationChannels.reserve(skeletonBonesCount);

  for (size_t channelIndex = 0; channelIndex < skeletonBonesCount; channelIndex++) {
    RawBoneAnimationChannel& channel = rawClip.bonesAnimationChannels[channelIndex];

    std::vector<BoneAnimationPositionFrame> positionFrames =
      MemoryUtils::createBinaryCompatibleVector<RawBonePositionFrame, BoneAnimationPositionFrame>(channel
        .positionFrames);

    std::vector<BoneAnimationOrientationFrame> orientationFrames =
      MemoryUtils::createBinaryCompatibleVector<RawBoneOrientationFrame, BoneAnimationOrientationFrame>(channel
        .orientationFrames);

    animationChannels.push_back({positionFrames, orientationFrames});
  }

  float animationDuration = rawClip.header.duration;
  float animationRate = rawClip.header.rate;

  return AnimationClip(skeleton,
    animationDuration,
    animationRate,
    animationChannels);
}


static GLuint loadShader(GLenum type, const std::string& source)
{
  const char* rawSource = source.c_str();

  GLuint shader = 0;

  switch (type) {
    case GL_VERTEX_SHADER:
      shader = glCreateShader(GL_VERTEX_SHADER);
      break;
    case GL_FRAGMENT_SHADER:
      shader = glCreateShader(GL_FRAGMENT_SHADER);
      break;
    default:
      assert(false);
      break;
  }

  glShaderSource(shader, 1, &rawSource, nullptr);
  glCompileShader(shader);

  GLuint shaderProgram = glCreateProgram();

  glAttachShader(shaderProgram, shader);
  glProgramParameteri(shaderProgram, GL_PROGRAM_SEPARABLE, GL_TRUE);

  glLinkProgram(shaderProgram);

  glDetachShader(shaderProgram, shader);
  glDeleteShader(shader);

  return shaderProgram;
}

static void setShaderParameter(GLuint shader, const std::string& name, int value)
{
  GLint location = glGetUniformLocation(shader, name.data());
  glProgramUniform1i(shader, location, value);
}

static void setShaderParameter(GLuint shader, const std::string& name, const glm::mat4& value)
{
  GLint location = glGetUniformLocation(shader, name.data());
  glProgramUniformMatrix4fv(shader, location, 1, GL_FALSE, &value[0][0]);
}

static void setShaderArrayParameter(GLuint shader, const std::string& name, const std::vector<glm::mat4>& array)
{
  GLint location = glGetUniformLocation(shader, name.data());
  glProgramUniformMatrix4fv(shader, location, static_cast<GLsizei>(array.size()), GL_FALSE, glm::value_ptr(array[0]));
}

static void initScene()
{
  // Shaders
  g_vertexShader = loadShader(GL_VERTEX_SHADER, FileUtils::readFile("../resources/vertex.glsl"));
  g_fragmentShader = loadShader(GL_FRAGMENT_SHADER, FileUtils::readFile("../resources/fragment.glsl"));

  glCreateProgramPipelines(1, &g_shadersPipeline);
  glUseProgramStages(g_shadersPipeline, GL_VERTEX_SHADER_BIT, g_vertexShader);
  glUseProgramStages(g_shadersPipeline, GL_FRAGMENT_SHADER_BIT, g_fragmentShader);

  // Mesh
  RawMesh mesh = RawMesh::readFromFile("../resources/human.mesh");

  std::vector<glm::vec3> positions = MemoryUtils::createBinaryCompatibleVector<RawVector3, glm::vec3>(mesh.positions);
  std::vector<glm::vec3> normals = MemoryUtils::createBinaryCompatibleVector<RawVector3, glm::vec3>(mesh.normals);
  std::vector<glm::vec2> uv = MemoryUtils::createBinaryCompatibleVector<RawVector2, glm::vec2>(mesh.uv);
  std::vector<glm::u8vec4> bonesIDs =
    MemoryUtils::createBinaryCompatibleVector<RawU8Vector4, glm::u8vec4>(mesh.bonesIds);
  std::vector<glm::u8vec4> bonesWeights =
    MemoryUtils::createBinaryCompatibleVector<RawU8Vector4, glm::u8vec4>(mesh.bonesWeights);

  std::vector<VertexPos3Norm3UVSkinned> vertices(mesh.header.verticesCount);

  for (size_t vertexIndex = 0; vertexIndex < mesh.header.verticesCount; vertexIndex++) {
    vertices[vertexIndex] = {
      positions[vertexIndex],
      normals[vertexIndex],
      uv[vertexIndex],
      bonesIDs[vertexIndex],
      bonesWeights[vertexIndex],
    };
  }

  glCreateBuffers(1, &g_vertexBuffer);
  glNamedBufferStorage(g_vertexBuffer,
    static_cast<GLsizeiptr>(vertices.size() * sizeof(vertices[0])),
    vertices.data(), 0);

  auto indicesMemorySize = static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(mesh.indices[0]));

  glCreateBuffers(1, &g_indexBuffer);
  glNamedBufferStorage(g_indexBuffer, indicesMemorySize, mesh.indices.data(), 0);

  g_meshIndicesCount = mesh.indices.size();

  glCreateVertexArrays(1, &g_vertexArrayObject);

  glVertexArrayVertexBuffer(g_vertexArrayObject, 0, g_vertexBuffer, 0, sizeof(VertexPos3Norm3UVSkinned));
  glVertexArrayElementBuffer(g_vertexArrayObject, g_indexBuffer);

  // Position
  glEnableVertexArrayAttrib(g_vertexArrayObject, 0);
  // Normal
  glEnableVertexArrayAttrib(g_vertexArrayObject, 1);
  // UV
  glEnableVertexArrayAttrib(g_vertexArrayObject, 2);
  // Bones IDs
  glEnableVertexArrayAttrib(g_vertexArrayObject, 4);
  // Bones Weights
  glEnableVertexArrayAttrib(g_vertexArrayObject, 5);

  glVertexArrayAttribFormat(g_vertexArrayObject, 0, 3, GL_FLOAT, GL_FALSE, offsetof(VertexPos3Norm3UVSkinned, pos));
  glVertexArrayAttribFormat(g_vertexArrayObject, 1, 3, GL_FLOAT, GL_FALSE, offsetof(VertexPos3Norm3UVSkinned, norm));
  glVertexArrayAttribFormat(g_vertexArrayObject, 2, 2, GL_FLOAT, GL_FALSE, offsetof(VertexPos3Norm3UVSkinned, uv));

  glVertexArrayAttribIFormat(g_vertexArrayObject, 4, 4, GL_UNSIGNED_BYTE,
    offsetof(VertexPos3Norm3UVSkinned, bonesIds));
  glVertexArrayAttribIFormat(g_vertexArrayObject, 5, 4, GL_UNSIGNED_BYTE,
    offsetof(VertexPos3Norm3UVSkinned, bonesWeights));

  glVertexArrayAttribBinding(g_vertexArrayObject, 0, 0);
  glVertexArrayAttribBinding(g_vertexArrayObject, 1, 0);
  glVertexArrayAttribBinding(g_vertexArrayObject, 2, 0);
  glVertexArrayAttribBinding(g_vertexArrayObject, 4, 0);
  glVertexArrayAttribBinding(g_vertexArrayObject, 5, 0);

  // Skeleton and animation clip
  auto skeleton = loadSkeleton("../resources/human.skeleton");
  auto animationClip = loadAnimationClip(skeleton, "../resources/human_running.anim");

  g_animationClip = std::make_unique<AnimationClip>(animationClip);

  // Texture
  int width, height;
  int nrChannels;
  auto* data = reinterpret_cast<std::byte*>(stbi_load("../resources/human.png",
    &width, &height, &nrChannels, 0));

  assert(nrChannels == 3);

  glCreateTextures(GL_TEXTURE_2D, 1, &g_texture);
  glTextureStorage2D(g_texture, 1, static_cast<GLenum>(GL_RGBA8), width, height);

  glTextureSubImage2D(g_texture, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE,
    static_cast<const void*>(data));

  glGenerateTextureMipmap(g_texture);
  glTextureParameteri(g_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTextureParameteri(g_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTextureParameteri(g_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(g_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTextureParameterf(g_texture, GL_TEXTURE_MAX_ANISOTROPY, 4.0f);

  // Model and camera positions
  g_modelMatrix = glm::toMat4(glm::angleAxis(glm::radians(-90.0f), glm::vec3{1.0f, 0.0f, 0.0f})) *
    glm::scale(glm::identity<glm::mat4>(), glm::vec3{0.6f, 0.6f, 0.6f});

  g_viewMatrix = glm::lookAt(glm::vec3{0.0f, 1.0, 6.5f},
    glm::vec3{0.0f, 1.0, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0});

  g_projectionMatrix = glm::perspective(glm::radians(g_FOVy), g_aspectRatio, g_nearClipDistance, g_farClipDistance);
}

static void deinitScene()
{
  glDeleteTextures(1, &g_texture);

  glDeleteBuffers(1, &g_vertexBuffer);
  glDeleteBuffers(1, &g_indexBuffer);
  glDeleteVertexArrays(1, &g_vertexArrayObject);

  glDeleteProgramPipelines(1, &g_shadersPipeline);

  glDeleteProgram(g_fragmentShader);
  glDeleteProgram(g_vertexShader);
}

static void updateScene(float delta)
{
  g_animationClip->increaseCurrentTime(delta);
}

static void renderScene()
{
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glBindProgramPipeline(g_shadersPipeline);

  glBindTextureUnit(0, g_texture);
  setShaderParameter(g_fragmentShader, "tex", 0);

  setShaderParameter(g_vertexShader, "scene.worldToCamera", g_viewMatrix);
  setShaderParameter(g_vertexShader, "scene.cameraToProjection", g_projectionMatrix);
  setShaderParameter(g_vertexShader, "transform.localToWorld", g_modelMatrix);

  const AnimationMatrixPalette& currentMatrixPalette = g_animationClip->getCurrentPose().getMatrixPalette();

  setShaderArrayParameter(g_vertexShader, "animation.palette[0]", currentMatrixPalette.bonesTransforms);

  glBindVertexArray(g_vertexArrayObject);

  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(g_meshIndicesCount), GL_UNSIGNED_SHORT, nullptr);

  SDL_GL_SwapWindow(g_window);
}

static void startGameLoop()
{
  SDL_ShowWindow(g_window);

  unsigned long nextTick = SDL_GetTicks();

  long sleepTime = 0;
  SDL_Event event;

  bool isMainLoopActive = true;

  while (true) {
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT || (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_CLOSE)) {
        isMainLoopActive = false;
        break;
      }
    }

    if (!isMainLoopActive) {
      break;
    }

    updateScene(1.0f / FRAMES_PER_SECOND);
    renderScene();

    nextTick += SKIP_TICKS;
    sleepTime = static_cast<long>(nextTick) - static_cast<long>(SDL_GetTicks());

    if (sleepTime >= 0) {
      SDL_Delay(static_cast<unsigned long>(sleepTime));
    }
  }
}

int main(int argc, char* argv[])
{
  initPlatform();
  initScene();

  startGameLoop();

  deinitScene();
  deinitPlatform();

  return 0;
}
