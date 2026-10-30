#version 330

// Input
in vec3 frag_position;
in vec3 frag_normal;
in vec2 frag_texture;
in vec3 frag_color;

uniform vec2 TimeInfo;

// Output
layout(location = 0) out vec4 out_color;

void main()
{
	out_color = vec4(frag_color.x + TimeInfo.x * 120 / 255 / TimeInfo.y, 
					 frag_color.y - TimeInfo.x * 120 / 255 / TimeInfo.y,
					 frag_color.z - TimeInfo.x * 135 / 255 / TimeInfo.y,
					 1);
}
