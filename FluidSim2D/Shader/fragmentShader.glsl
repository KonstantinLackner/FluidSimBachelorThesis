
#version 330 core
/*#version 420 core

out vec4 FragColor;
layout (binding = 0) uniform sampler2D textureSampler;
//layout (origin_upper_left, pixel_center_integer) in vec4 gl_FragCoord;

in vec2 vTex;

void main()
{
   FragColor = vec4(vec2(0.5, 0.5) + vec2(0.5, 0.5) * texture(textureSampler, vTex).rg, 0.0, 1.0);
};
*/

uniform sampler2D field;

in vec2 vTex;

out vec4 FragColor;

void main()
{
	FragColor = vec4(vec2(0.5, 0.5) + vec2(0.5, 0.5) * texture(field, vTex).rg, 0.5, 1.0);
}
