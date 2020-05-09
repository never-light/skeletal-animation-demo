#version 450 core

in VertexData
{
	vec2 uv;
} inVertexData;

uniform sampler2D tex;

out vec4 out_fragColor;

void main() {
	out_fragColor = texture(tex, inVertexData.uv);
}
