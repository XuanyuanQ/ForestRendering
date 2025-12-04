#version 410

// Remember how we enabled vertex attributes in assignment 2 and attached some
// data to each of them, here we retrieve that data. Attribute 0 pointed to the
// vertices inside the OpenGL buffer object, so if we say that our input
// variable `vertex` is at location 0, which corresponds to attribute 0 of our
// vertex array, vertex will be effectively filled with vertices from our
// buffer.
// Similarly, normal is set to location 1, which corresponds to attribute 1 of
// the vertex array, and therefore will be filled with normals taken out of our
// buffer.
layout(location = 0) in vec3 vertex;
// layout(location = 2) in vec3 normal;
layout(location = 1) in vec3 texcoord;
// layout(location = 3) in vec3 tangent;
// layout(location = 4) in vec3 binormal;

// uniform mat4 vertex_model_to_world;
// uniform mat4 normal_model_to_world;
// uniform mat4 vertex_world_to_clip;
uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_view;
uniform mat4 vertex_view_to_projection;

// This is the custom output of this shader. If you want to retrieve this data
// from another shader further down the pipeline, you need to declare the exact
// same structure as in (for input), with matching name for the structure
// members and matching structure type. Have a look at
// shaders/EDAF80/diffuse.frag.
out VS_OUT { vec2 texcoord; }
vs_out;

uniform vec3 light_position;
uniform vec3 camera_position;
uniform float elapsed_time_s;

float waveFun(float time, float A, float f, float p, float k, vec2 D,
              vec3 point) {
  float a = sin((D.x * point.x + (D.y) * point.z) * f + time * p) * 0.5 + 0.5;
  return A * pow(a, k);
}

float derivativeMain(float time, float A, float f, float p, float k, vec2 D,
                     vec3 point) {
  float wave = waveFun(time, A, f, p, max(0, k - 1.0), D, point);
  return 0.5 * k * f * wave *
         cos((D.x * point.x + (D.y) * point.z) * f + time * p);
}

void main() {

  vec3 point = vertex;
  float time = 1.0;
  float wave1 = waveFun(time, 1.0, 0.2, 0.5, 2.0, vec2(-1.0, 0.0), point);
  float wave2 = waveFun(time, 0.5, 0.4, 1.3, 2.0, vec2(-0.7, 0.7), point);

  vs_out.texcoord = texcoord.xy;
  gl_Position = vertex_view_to_projection * vertex_world_to_view *
                vertex_model_to_world *
                vec4(vec3(point.x, 1.0 * (wave1 + wave2), point.z), 1.0);
}
