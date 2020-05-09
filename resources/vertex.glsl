#version 450 core

layout (location = 0) in vec3 attrPos;
layout (location = 1) in vec3 attrNorm;
layout (location = 2) in vec2 attrUV;
layout (location = 4) in uvec4 attrBonesIds;
layout (location = 5) in uvec4 attrBonesWeights;

struct Scene {
    mat4 worldToCamera;
    mat4 cameraToProjection;
};

struct Transform {
    mat4 localToWorld;
};

struct AnimationPalette {
    mat4 palette[128];
};

uniform Scene scene;
uniform Transform transform;

uniform AnimationPalette animation;

out VertexData
{
    vec2 uv;
} outVertexData;

out gl_PerVertex { vec4 gl_Position; };

void main() {
    vec4 position = vec4(attrPos, 1.0);

    vec4 newPosition = (float(attrBonesWeights[0]) / 255.0) * animation.palette[attrBonesIds[0]] * position +
				(float(attrBonesWeights[1]) / 255.0) * animation.palette[attrBonesIds[1]] * position +
				(float(attrBonesWeights[2]) / 255.0) * animation.palette[attrBonesIds[2]] * position +
				(float(attrBonesWeights[3]) / 255.0) * animation.palette[attrBonesIds[3]] * position;

    outVertexData.uv = attrUV;
    gl_Position = scene.cameraToProjection * scene.worldToCamera * transform.localToWorld * (vec4(newPosition.xyz, 1.0));
}
