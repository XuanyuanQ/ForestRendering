#version 410

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texcoord;
layout(location = 3) in vec3 tangent;

layout(location = 7) in vec4 instanceMatrix1;
layout(location = 8) in vec4 instanceMatrix2;
layout(location = 9) in vec4 instanceMatrix3;
layout(location = 10) in vec4 instanceMatrix4;

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;
// uniform mat4 normal_model_to_world;
uniform vec3 light_position;
uniform vec3 camera_position;

uniform float elapsed_time_s;
uniform float wind_strength;

out VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 fV;
  vec3 fL;
}
vs_out;

void main() {
	
  float wave = sin(elapsed_time_s * 3.0 + vertex.x * 5.0 + vertex.z * 2.0);
  vec3 newPos = vertex;
 
  //wind_strength (总力度) * wave (波动) * texcoord.y (根部不动权重)
	newPos.x += wave * wind_strength * texcoord.y * 20.0;
    newPos.z += cos(elapsed_time_s * 2.5 + vertex.x * 3.0) * wind_strength * texcoord.y * 10;
	
  mat4 instanceMatrix = mat4(instanceMatrix1, instanceMatrix2, instanceMatrix3, instanceMatrix4);
  mat4 model_to_world = vertex_model_to_world * instanceMatrix;
  mat4 normal_model_to_world = transpose(inverse(model_to_world));

  vec4 worldPos = model_to_world * vec4(newPos * 0.02, 1.0);
  vs_out.texcoord = texcoord.xy;
  vs_out.normal = vec3(normal_model_to_world * vec4(normal, 0.0));
  vs_out.fV = camera_position - vec3(worldPos);
  vs_out.fL = light_position;
  gl_Position = vertex_world_to_clip * worldPos ;
}
