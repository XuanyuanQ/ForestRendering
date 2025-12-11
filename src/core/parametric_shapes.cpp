#include "parametric_shapes.hpp"
#include "core/Log.h"

#include <glm/glm.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

bonobo::mesh_data
parametric_shapes::createQuad(float const width, float const height,
                              unsigned int const horizontal_split_count,
                              unsigned int const vertical_split_count,
                              std::function<int()> const &setupInstance) {
  auto const vertices_nb =
      (horizontal_split_count + 1) * (vertical_split_count + 1);
  auto const indexs_nb = (horizontal_split_count) * (vertical_split_count) * 2;

  auto vertices = std::vector<glm::vec3>(vertices_nb);
  auto normals = std::vector<glm::vec3>(vertices_nb);
  auto texcoords = std::vector<glm::vec3>(vertices_nb);
  auto tangents = std::vector<glm::vec3>(vertices_nb);
  auto binormals = std::vector<glm::vec3>(vertices_nb);

  auto index_sets = std::vector<glm::uvec3>(indexs_nb);

  int vertices_count = 0;
  for (int i = 0; i < horizontal_split_count + 1; i++) { // x
    for (int j = 0; j < vertical_split_count + 1; j++) { // z
      vertices.at(vertices_count) =
          glm::vec3(j * width / vertical_split_count - 0.5 * width, 0.0f,
                    i * height / horizontal_split_count - 0.5 * height);
      auto pt = glm::vec3(j * width / vertical_split_count, 0.0f, 0.0f);
      auto pb = glm::vec3(0.0f, 0.0f, i * height / horizontal_split_count);

      tangents.at(vertices_count) = glm::normalize(pt);
      binormals.at(vertices_count) = glm::normalize(pb);
      normals.at(vertices_count) =
          glm::normalize(glm::cross(glm::normalize(pt), glm::normalize(pb)));
      texcoords.at(vertices_count) =
          glm::vec3(float(1.0 * j / vertical_split_count),
                    float(1.0 * i / horizontal_split_count), 0.0f);
      ++vertices_count;
    }
  }
  int index_count = 0;
  for (int i = 0; i < horizontal_split_count; ++i) {
    for (int j = 0; j < vertical_split_count; ++j) {

      int top_left = j + i * (vertical_split_count + 1);
      int top_right = (j + 1) + i * (vertical_split_count + 1);
      int bottom_left = j + (i + 1) * (vertical_split_count + 1);
      int bottom_right = (j + 1) + (i + 1) * (vertical_split_count + 1);

      // 第一个三角形 (逆时针)
      index_sets.at(index_count++) =
          glm::vec3(top_left, bottom_left, bottom_right);

      // 第二个三角形 (逆时针)
      index_sets.at(index_count++) =
          glm::vec3(top_left, bottom_right, top_right);
    }
  }
  std::cout << index_sets.size() << "," << vertices.size() << std::endl;
  // for (auto texcoord : texcoords) {
  //     std::cout << "texcoord:" << texcoord[0] << texcoord[1] << texcoord[2]
  //     << std::endl;
  // }
  bonobo::mesh_data data;

  auto const vertices_offset = 0u;
  auto const vertices_size =
      static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
  auto const normals_offset = vertices_size;
  auto const normals_size =
      static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
  auto const texcoords_offset = normals_offset + normals_size;
  auto const texcoords_size =
      static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
  auto const tangents_offset = texcoords_offset + texcoords_size;
  auto const tangents_size =
      static_cast<GLsizeiptr>(tangents.size() * sizeof(glm::vec3));
  auto const binormals_offset = tangents_offset + tangents_size;
  auto const binormals_size =
      static_cast<GLsizeiptr>(binormals.size() * sizeof(glm::vec3));
  auto const bo_size =
      static_cast<GLsizeiptr>(vertices_size + normals_size + texcoords_size +
                              tangents_size + binormals_size);

  glGenVertexArrays(1, &data.vao);
  assert(data.vao != 0u);
  glBindVertexArray(data.vao);
  glGenBuffers(1, &data.bo);
  assert(data.bo != 0u);
  glBindBuffer(GL_ARRAY_BUFFER, data.bo);
  glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

  glBufferSubData(GL_ARRAY_BUFFER, vertices_offset, vertices_size,
                  static_cast<GLvoid const *>(vertices.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(0x0));

  glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size,
                  static_cast<GLvoid const *>(normals.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::normals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(normals_offset));

  glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size,
                  static_cast<GLvoid const *>(texcoords.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(texcoords_offset));

  glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size,
                  static_cast<GLvoid const *>(tangents.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(tangents_offset));

  glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size,
                  static_cast<GLvoid const *>(binormals.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(binormals_offset));

  if (setupInstance) {
    setupInstance();
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0u);

  data.indices_nb = static_cast<GLsizei>(index_sets.size() * 3u);
  glGenBuffers(1, &data.ibo);
  assert(data.ibo != 0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(index_sets.size() * sizeof(glm::uvec3)),
               reinterpret_cast<GLvoid const *>(index_sets.data()),
               GL_STATIC_DRAW);

  glBindVertexArray(0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

  return data;
}

bonobo::mesh_data
parametric_shapes::createSphere(float const radius,
                                unsigned int const longitude_split_count,
                                unsigned int const latitude_split_count) {

  //! \todo Implement this function
  auto const vertices_nb =
      (longitude_split_count + 1) * (latitude_split_count + 1);

  auto vertices = std::vector<glm::vec3>(vertices_nb);
  auto normals = std::vector<glm::vec3>(vertices_nb);
  auto texcoords = std::vector<glm::vec3>(vertices_nb);
  auto tangents = std::vector<glm::vec3>(vertices_nb);
  auto binormals = std::vector<glm::vec3>(vertices_nb);

  // generate vertices iteratively 0 ≤ θ ≤ 2π and 0 ≤ ϕ ≤ π.
  size_t index = 0u;
  float theta = 0.0f;
  float phi = 0.0f;
  float theta_angle_step =
      glm::two_pi<float>() /
      static_cast<float>(longitude_split_count); // azimuth angle
  float phi_angle_step =
      glm::pi<float>() /
      static_cast<float>(latitude_split_count);              // polar angle
  for (unsigned i = 0u; i < latitude_split_count + 1; i++) { // down -> up
    phi = i * phi_angle_step;
    for (unsigned j = 0u; j < longitude_split_count + 1; j++) { // right ->left
      theta = j * theta_angle_step;
      vertices[index] = glm::vec3(radius * std::sin(theta) * std::sin(phi),
                                  -radius * std::cos(phi),
                                  radius * std::cos(theta) * std::sin(phi));

      if (phi == 0.0f || phi == glm::pi<float>()) {
        tangents[index] = glm::vec3(1.0f, 0.0f, 0.0f);    // 固定切向量
        normals[index] = glm::normalize(vertices[index]); // 指向球心
        binormals[index] = glm::cross(normals[index], tangents[index]);
      } else {
        tangents[index] = glm::normalize(
            glm::vec3(radius * std::cos(theta) * std::sin(phi), 0.0f,
                      -radius * std::sin(theta) * std::sin(phi)));
        binormals[index] = glm::normalize(glm::vec3(
            radius * std::sin(theta) * std::cos(phi), radius * std::sin(phi),
            radius * std::cos(theta) * std::cos(phi)));
        normals[index] =
            glm::normalize(glm::cross(tangents[index], binormals[index]));
      }
      // 打印顶点
      // printf("v[%zu] = (%f, %f, %f)\n", index, vertices[index].x,
      // vertices[index].y, vertices[index].z);
      float u = theta / glm::two_pi<float>(); // θ 对应水平方向
      float v = phi / glm::pi<float>();       // φ 对应竖直方向
      texcoords[index] = glm::vec3(u, v, 0.0f);
      index++;
    }
  }

  // create index array
  index = 0u;
  auto index_sets =
      std::vector<glm::uvec3>(latitude_split_count * longitude_split_count * 2);

  // generate indices iteratively
  for (unsigned i = 0; i < latitude_split_count; i++) {
    for (unsigned j = 0; j < longitude_split_count; j++) {
      unsigned int a = i * (longitude_split_count + 1) + j;
      unsigned int b = a + 1;
      unsigned int c = (i + 1) * (longitude_split_count + 1) + j;
      unsigned int d = c + 1;
      // 三角形 1
      index_sets[index] = glm::uvec3(a, c, b);
      index++;

      // 三角形 2
      index_sets[index] = glm::uvec3(b, c, d);
      index++;
    }
  }

  bonobo::mesh_data data;
  glGenVertexArrays(1, &data.vao);
  assert(data.vao != 0u);
  glBindVertexArray(data.vao);

  auto const vertices_offset = 0u;
  auto const vertices_size =
      static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
  auto const normals_offset = vertices_size;
  auto const normals_size =
      static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
  auto const texcoords_offset = normals_offset + normals_size;
  auto const texcoords_size =
      static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
  auto const tangents_offset = texcoords_offset + texcoords_size;
  auto const tangents_size =
      static_cast<GLsizeiptr>(tangents.size() * sizeof(glm::vec3));
  auto const binormals_offset = tangents_offset + tangents_size;
  auto const binormals_size =
      static_cast<GLsizeiptr>(binormals.size() * sizeof(glm::vec3));
  auto const bo_size =
      static_cast<GLsizeiptr>(vertices_size + normals_size + texcoords_size +
                              tangents_size + binormals_size);
  glGenBuffers(1, &data.bo);
  assert(data.bo != 0u);
  glBindBuffer(GL_ARRAY_BUFFER, data.bo);
  glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

  glBufferSubData(GL_ARRAY_BUFFER, vertices_offset, vertices_size,
                  static_cast<GLvoid const *>(vertices.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(0x0));

  glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size,
                  static_cast<GLvoid const *>(normals.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::normals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(normals_offset));

  glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size,
                  static_cast<GLvoid const *>(texcoords.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(texcoords_offset));

  glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size,
                  static_cast<GLvoid const *>(tangents.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(tangents_offset));

  glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size,
                  static_cast<GLvoid const *>(binormals.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(binormals_offset));

  glBindBuffer(GL_ARRAY_BUFFER, 0u);

  data.indices_nb = static_cast<GLsizei>(index_sets.size() * 3u);
  glGenBuffers(1, &data.ibo);
  assert(data.ibo != 0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(index_sets.size() * sizeof(glm::uvec3)),
               reinterpret_cast<GLvoid const *>(index_sets.data()),
               GL_STATIC_DRAW);

  glBindVertexArray(0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

  return data;
}

bonobo::mesh_data
parametric_shapes::createTorus(float const major_radius,
                               float const minor_radius,
                               unsigned int const major_split_count,
                               unsigned int const minor_split_count) {

  auto const vertices_nb = (major_split_count + 1) * (minor_split_count + 1);
  auto vertices = std::vector<glm::vec3>(vertices_nb);
  auto normals = std::vector<glm::vec3>(vertices_nb);
  ;
  auto tangents = std::vector<glm::vec3>(vertices_nb);
  auto binormals = std::vector<glm::vec3>(vertices_nb);
  auto texcoords = std::vector<glm::vec2>(vertices_nb);

  size_t index = 0u;
  float const major_step =
      glm::two_pi<float>() / static_cast<float>(major_split_count);
  float const minor_step =
      glm::two_pi<float>() / static_cast<float>(minor_split_count);

  // ===== 顶点环绕生成 =====
  for (unsigned int i = 0u; i <= major_split_count; ++i) {
    float u = i * major_step; // 环的角度
    float cos_u = std::cos(u);
    float sin_u = std::sin(u);

    for (unsigned int j = 0u; j <= minor_split_count; ++j, ++index) {
      float v = j * minor_step; // 管的角度
      float cos_v = std::cos(v);
      float sin_v = std::sin(v);

      // 顶点坐标
      vertices[index] = glm::vec3((major_radius + minor_radius * cos_v) * cos_u,
                                  (major_radius + minor_radius * cos_v) * sin_u,
                                  minor_radius * sin_v);

      // 法线方向（指向圆管外侧）
      normals[index] =
          glm::normalize(glm::vec3(cos_u * cos_v, sin_u * cos_v, sin_v));

      // 切线方向（沿大圆方向）
      tangents[index] = glm::normalize(glm::vec3(-sin_u, cos_u, 0.0f));

      // 副切线方向
      binormals[index] =
          glm::normalize(glm::cross(normals[index], tangents[index]));

      // 纹理坐标
      texcoords[index] = glm::vec2(static_cast<float>(i) / major_split_count,
                                   static_cast<float>(j) / minor_split_count);
    }
  }

  auto index_sets =
      std::vector<glm::uvec3>(major_split_count * minor_split_count * 2);
  index = 0u;
  for (unsigned i = 0; i < major_split_count; i++) {
    for (unsigned j = 0; j < minor_split_count; j++) {
      unsigned int a = i * (minor_split_count + 1) + j;
      unsigned int b = a + 1;
      unsigned int c = (i + 1) * (minor_split_count + 1) + j;
      unsigned int d = c + 1;

      index_sets[index++] = glm::uvec3(a, c, b);
      index_sets[index++] = glm::uvec3(b, c, d);
    }
  }

  bonobo::mesh_data data;
  glGenVertexArrays(1, &data.vao);
  assert(data.vao != 0u);
  glBindVertexArray(data.vao);

  auto const vertices_offset = 0u;
  auto const vertices_size =
      static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
  auto const normals_offset = vertices_size;
  auto const normals_size =
      static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
  auto const texcoords_offset = normals_offset + normals_size;
  auto const texcoords_size =
      static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec2));
  auto const tangents_offset = texcoords_offset + texcoords_size;
  auto const tangents_size =
      static_cast<GLsizeiptr>(tangents.size() * sizeof(glm::vec3));
  auto const binormals_offset = tangents_offset + tangents_size;
  auto const binormals_size =
      static_cast<GLsizeiptr>(binormals.size() * sizeof(glm::vec3));
  auto const bo_size =
      static_cast<GLsizeiptr>(vertices_size + normals_size + texcoords_size +
                              tangents_size + binormals_size);
  glGenBuffers(1, &data.bo);
  assert(data.bo != 0u);
  glBindBuffer(GL_ARRAY_BUFFER, data.bo);
  glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

  glBufferSubData(GL_ARRAY_BUFFER, vertices_offset, vertices_size,
                  static_cast<GLvoid const *>(vertices.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(0x0));

  glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size,
                  static_cast<GLvoid const *>(normals.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::normals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(normals_offset));

  glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size,
                  static_cast<GLvoid const *>(texcoords.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 2,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(texcoords_offset));

  glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size,
                  static_cast<GLvoid const *>(tangents.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(tangents_offset));

  glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size,
                  static_cast<GLvoid const *>(binormals.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(binormals_offset));

  glBindBuffer(GL_ARRAY_BUFFER, 0u);

  data.indices_nb = static_cast<GLsizei>(index_sets.size() * 3u);
  glGenBuffers(1, &data.ibo);
  assert(data.ibo != 0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(index_sets.size() * sizeof(glm::uvec3)),
               reinterpret_cast<GLvoid const *>(index_sets.data()),
               GL_STATIC_DRAW);

  glBindVertexArray(0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

  return data;
}

bonobo::mesh_data
parametric_shapes::createCircleRing(float const radius,
                                    float const spread_length,
                                    unsigned int const circle_split_count,
                                    unsigned int const spread_split_count) {
  auto const circle_slice_edges_count = circle_split_count + 1u;
  auto const spread_slice_edges_count = spread_split_count + 1u;
  auto const circle_slice_vertices_count = circle_slice_edges_count + 1u;
  auto const spread_slice_vertices_count = spread_slice_edges_count + 1u;
  auto const vertices_nb =
      circle_slice_vertices_count * spread_slice_vertices_count;

  auto vertices = std::vector<glm::vec3>(vertices_nb);
  auto normals = std::vector<glm::vec3>(vertices_nb);
  auto texcoords = std::vector<glm::vec3>(vertices_nb);
  auto tangents = std::vector<glm::vec3>(vertices_nb);
  auto binormals = std::vector<glm::vec3>(vertices_nb);

  float const spread_start = radius - 0.5f * spread_length;
  float const d_theta =
      glm::two_pi<float>() / (static_cast<float>(circle_slice_edges_count));
  float const d_spread =
      spread_length / (static_cast<float>(spread_slice_edges_count));

  // generate vertices iteratively
  size_t index = 0u;
  float theta = 0.0f;
  for (unsigned int i = 0u; i < circle_slice_vertices_count; ++i) {
    float const cos_theta = std::cos(theta);
    float const sin_theta = std::sin(theta);

    float distance_to_centre = spread_start;
    for (unsigned int j = 0u; j < spread_slice_vertices_count; ++j) {
      // vertex
      vertices[index] = glm::vec3(distance_to_centre * cos_theta,
                                  distance_to_centre * sin_theta, 0.0f);

      // texture coordinates
      texcoords[index] =
          glm::vec3(static_cast<float>(j) /
                        (static_cast<float>(spread_slice_vertices_count)),
                    static_cast<float>(i) /
                        (static_cast<float>(circle_slice_vertices_count)),
                    0.0f);

      // tangent
      auto const t = glm::vec3(cos_theta, sin_theta, 0.0f);
      tangents[index] = t;

      // binormal
      auto const b = glm::vec3(-sin_theta, cos_theta, 0.0f);
      binormals[index] = b;

      // normal
      auto const n = glm::cross(t, b);
      normals[index] = n;

      distance_to_centre += d_spread;
      ++index;
    }

    theta += d_theta;
  }

  // create index array
  auto index_sets = std::vector<glm::uvec3>(2u * circle_slice_edges_count *
                                            spread_slice_edges_count);

  // generate indices iteratively
  index = 0u;
  for (unsigned int i = 0u; i < circle_slice_edges_count; ++i) {
    for (unsigned int j = 0u; j < spread_slice_edges_count; ++j) {
      index_sets[index] =
          glm::uvec3(spread_slice_vertices_count * (i + 0u) + (j + 0u),
                     spread_slice_vertices_count * (i + 0u) + (j + 1u),
                     spread_slice_vertices_count * (i + 1u) + (j + 1u));
      ++index;

      index_sets[index] =
          glm::uvec3(spread_slice_vertices_count * (i + 0u) + (j + 0u),
                     spread_slice_vertices_count * (i + 1u) + (j + 1u),
                     spread_slice_vertices_count * (i + 1u) + (j + 0u));
      ++index;
    }
  }

  bonobo::mesh_data data;
  glGenVertexArrays(1, &data.vao);
  assert(data.vao != 0u);
  glBindVertexArray(data.vao);

  auto const vertices_offset = 0u;
  auto const vertices_size =
      static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
  auto const normals_offset = vertices_size;
  auto const normals_size =
      static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
  auto const texcoords_offset = normals_offset + normals_size;
  auto const texcoords_size =
      static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
  auto const tangents_offset = texcoords_offset + texcoords_size;
  auto const tangents_size =
      static_cast<GLsizeiptr>(tangents.size() * sizeof(glm::vec3));
  auto const binormals_offset = tangents_offset + tangents_size;
  auto const binormals_size =
      static_cast<GLsizeiptr>(binormals.size() * sizeof(glm::vec3));
  auto const bo_size =
      static_cast<GLsizeiptr>(vertices_size + normals_size + texcoords_size +
                              tangents_size + binormals_size);
  glGenBuffers(1, &data.bo);
  assert(data.bo != 0u);
  glBindBuffer(GL_ARRAY_BUFFER, data.bo);
  glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

  glBufferSubData(GL_ARRAY_BUFFER, vertices_offset, vertices_size,
                  static_cast<GLvoid const *>(vertices.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(0x0));

  glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size,
                  static_cast<GLvoid const *>(normals.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::normals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(normals_offset));

  glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size,
                  static_cast<GLvoid const *>(texcoords.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(texcoords_offset));

  glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size,
                  static_cast<GLvoid const *>(tangents.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT,
      GL_FALSE, 0, reinterpret_cast<GLvoid const *>(tangents_offset));

  glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size,
                  static_cast<GLvoid const *>(binormals.data()));
  glEnableVertexAttribArray(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals));
  glVertexAttribPointer(
      static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3,
      GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<GLvoid const *>(binormals_offset));

  glBindBuffer(GL_ARRAY_BUFFER, 0u);

  data.indices_nb = static_cast<GLsizei>(index_sets.size() * 3u);
  glGenBuffers(1, &data.ibo);
  assert(data.ibo != 0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(index_sets.size() * sizeof(glm::uvec3)),
               reinterpret_cast<GLvoid const *>(index_sets.data()),
               GL_STATIC_DRAW);

  glBindVertexArray(0u);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

  return data;
}
