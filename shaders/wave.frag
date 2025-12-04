#version 410

uniform vec3 light_position;

in VS_OUT { vec2 texcoord; }
fs_in;

uniform sampler2D diffuse_texture;

out vec4 frag_color;

void main() { frag_color = texture(diffuse_texture, fs_in.texcoord); }
