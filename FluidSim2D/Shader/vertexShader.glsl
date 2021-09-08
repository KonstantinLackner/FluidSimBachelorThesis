#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 2) in vec2 tex;

out vec2 vTex;

void main()
{
	vTex = tex;
	gl_Position = vec4(pos.xy, 0.0, 1.0);
}
