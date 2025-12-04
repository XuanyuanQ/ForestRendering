#version 410

uniform vec3 light_position;

in VS_OUT { vec2 texcoord; }
fs_in;

uniform vec3 diffuse_colour;
uniform vec3 specular_colour;
uniform vec3 ambient_colour;
uniform float shininess_value;
uniform int use_normal_mapping;
uniform samplerCube cubemap;

uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D normal_texture;
uniform sampler2D waveNormal_texture;

out vec4 frag_color;

void main() { frag_color = texture(diffuse_texture, fs_in.texcoord); }
