#include "ForestScene.hpp"
#include "config.hpp"
#include "core/helpers.hpp"
#include "core/parametric_shapes.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <random>

ForestScene::ForestScene(WindowManager &windowManager)
    : _windowManager(windowManager),
      _camera(0.5f * glm::half_pi<float>(),
              static_cast<float>(config::resolution_x) /
                  static_cast<float>(config::resolution_y),
              0.01f, 1000.0f),
      _isPaused(false), // 默认自动播放
      _sunTime(0.0f),   // 从 0 开始
      _daySpeed(0.5f)   // 默认速度
{
  _camera.mWorld.SetTranslate(glm::vec3(0.0f, 10.0f, 20.0f));
  _camera.mMouseSensitivity = glm::vec2(0.003f);
  _camera.mMovementSpeed = glm::vec3(3.0f);

  _treeCount = 20;
  _grassCount = 500;
  _elapsedTimeS = 0.0f;
  _isLeavesMesh = false;
  _lightPosition = glm::vec3(-2.0f, 4.0f, -2.0f);

  _cullMode = bonobo::cull_mode_t::disabled;
  _polygonMode = bonobo::polygon_mode_t::fill;
  _showGui = true;
  _fallbackShader = 0;
  _instanceVBO = 0;
}

ForestScene::~ForestScene() {
  if (_instanceVBO != 0)
    glDeleteBuffers(1, &_instanceVBO);
  // Node 的析构函数会自动清理它管理的资源，不需要手动 glDeleteProgram 等
}
GLuint ForestScene::createQuadsForPatch() {
  std::vector<float> vertices;
  float width = 100.f;
  float height = 100.f;
  unsigned rez = 100;
  for (unsigned i = 0; i <= rez - 1; i++) {
    for (unsigned j = 0; j <= rez - 1; j++) {
      vertices.push_back(-width / 2.0f + width * i / (float)rez);   // v.x
      vertices.push_back(0.0f);                                     // v.y
      vertices.push_back(-height / 2.0f + height * j / (float)rez); // v.z
      vertices.push_back(i / (float)rez);                           // u
      vertices.push_back(j / (float)rez);                           // v

      vertices.push_back(-width / 2.0f + width * (i + 1) / (float)rez); // v.x
      vertices.push_back(0.0f);                                         // v.y
      vertices.push_back(-height / 2.0f + height * j / (float)rez);     // v.z
      vertices.push_back((i + 1) / (float)rez);                         // u
      vertices.push_back(j / (float)rez);                               // v

      vertices.push_back(-width / 2.0f + width * i / (float)rez);         // v.x
      vertices.push_back(0.0f);                                           // v.y
      vertices.push_back(-height / 2.0f + height * (j + 1) / (float)rez); // v.z
      vertices.push_back(i / (float)rez);                                 // u
      vertices.push_back((j + 1) / (float)rez);                           // v

      vertices.push_back(-width / 2.0f + width * (i + 1) / (float)rez);   // v.x
      vertices.push_back(0.0f);                                           // v.y
      vertices.push_back(-height / 2.0f + height * (j + 1) / (float)rez); // v.z
      vertices.push_back((i + 1) / (float)rez);                           // u
      vertices.push_back((j + 1) / (float)rez);                           // v
    }
  }
  std::cout << "Loaded " << rez * rez << " patches of 4 control points each"
            << std::endl;
  std::cout << "Processing " << rez * rez * 4 << " vertices in vertex shader"
            << std::endl;
  // first, configure the cube's VAO (and terrainVBO)
  unsigned int terrainVAO, terrainVBO;
  glGenVertexArrays(1, &terrainVAO);
  glBindVertexArray(terrainVAO);

  glGenBuffers(1, &terrainVBO);
  glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), &vertices[0],
               GL_STATIC_DRAW);

  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  // texCoord attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(sizeof(float) * 3));
  glEnableVertexAttribArray(1);

  glPatchParameteri(GL_PATCH_VERTICES, _NUM_PATCH_PTS);
  _terrainVao = terrainVAO;
  _terrainVbo = terrainVBO;
  return terrainVAO;
}

std::vector<glm::mat4> ForestScene::generateTreeTransforms(int count, int Width,
                                                           int Depth) {
  std::vector<glm::mat4> matrices;
  matrices.reserve(count);
  int forestWidth = Width;
  int forestDepth = Depth;

  for (int i = 0; i < count; i++) {
    glm::mat4 model = glm::mat4(1.0f);
    float x = ((rand() % 10000) / 10000.0f) * forestWidth - forestWidth / 2.0f;
    float z = ((rand() % 10000) / 10000.0f) * forestDepth - forestDepth / 2.0f;
    float y = -4.0f + ((rand() % 1000) / 1000.0f) * 0.5f;
    model = glm::translate(model, glm::vec3(x, y, z));
    float scale = 0.8f + ((rand() % 500) / 1000.0f);
    model = glm::scale(model, glm::vec3(scale));
    float rotAngle = static_cast<float>(rand() % 360);
    model =
        glm::rotate(model, glm::radians(rotAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    matrices.push_back(model);
  }
  return matrices;
}
void ForestScene::createLight() {
  bonobo::mesh_data cone;
  cone.vertices_nb = 6;
  cone.drawing_mode = GL_TRIANGLES;
  float vertexArrayData[6 * 3] = {
      -1.0f, -1.0f, 0.0f, // 左下
      1.0f,  -1.0f, 0.0f, // 右下
      1.0f,  1.0f,  0.0f, // 右上

      -1.0f, -1.0f, 0.0f, // 左下
      1.0f,  1.0f,  0.0f, // 右上
      -1.0f, 1.0f,  0.0f  // 左上
  };

  glGenVertexArrays(1, &cone.vao);
  assert(cone.vao != 0u);
  glBindVertexArray(cone.vao);
  {

    glGenBuffers(1, &cone.bo);
    assert(cone.bo != 0u);
    glBindBuffer(GL_ARRAY_BUFFER, cone.bo);
    glBufferData(GL_ARRAY_BUFFER, cone.vertices_nb * 3 * sizeof(float),
                 vertexArrayData, GL_STATIC_DRAW);

    glVertexAttribPointer(static_cast<int>(bonobo::shader_bindings::vertices),
                          3, GL_FLOAT, GL_FALSE, 0,
                          reinterpret_cast<GLvoid const *>(0x0));
    glEnableVertexAttribArray(
        static_cast<int>(bonobo::shader_bindings::vertices));

    glBindBuffer(GL_ARRAY_BUFFER, 0u);
    lightMesh = cone;
    lightgeometry.set_geometry(cone);
  }
  glBindVertexArray(0u);
}

void ForestScene::initLightMatrix() {
  float scale_lengths = 100.f;
  float const lightProjectionNearPlane = 0.01f * scale_lengths;
  float const lightProjectionFarPlane = 20.0f * scale_lengths;
  auto lightProjection =
      glm::ortho(-scale_lengths, scale_lengths, -scale_lengths, scale_lengths,
                 lightProjectionNearPlane, lightProjectionFarPlane);
  // auto lightProjection = glm::perspective(
  //     0.5f * glm::pi<float>(),
  //     static_cast<float>(SHADOW_WIDTH) / static_cast<float>(SHADOW_HEIGHT),
  //     lightProjectionNearPlane, lightProjectionFarPlane);
  TRSTransformf lightOffsetTransform;
  lightOffsetTransform.SetTranslate(glm::vec3(0.0f, 0.0f, -0.4f) *
                                    scale_lengths);
  TRSTransformf lightTransform;
  lightTransform.SetTranslate(glm::vec3(0.0f, 1.25f, 0.0f) * scale_lengths);
  lightTransform.SetRotate(0.1f * 10, glm::vec3(0.0f, 1.0f, 0.0f));
  auto const light_view_matrix = lightOffsetTransform.GetMatrixInverse() *
                                 lightTransform.GetMatrixInverse();
  light_world_to_clip_matrix = lightProjection * light_view_matrix;
}
void ForestScene::initGbuffer() {
  glGenFramebuffers(1, &gbufferFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, gbufferFBO);

  const unsigned int WIDTH = SHADOW_WIDTH;
  const unsigned int HEIGHT = SHADOW_HEIGHT;

  // --- 1. Diffuse
  // glGenTextures(1, &gDiffuse);
  // glBindTexture(GL_TEXTURE_2D, gDiffuse);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WIDTH, HEIGHT, 0, GL_RGBA,
  //              GL_UNSIGNED_BYTE, nullptr);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
  //                        gDiffuse, 0);

  // // --- 2. Specular
  // glGenTextures(1, &gSpecular);
  // glBindTexture(GL_TEXTURE_2D, gSpecular);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WIDTH, HEIGHT, 0, GL_RGBA,
  //              GL_UNSIGNED_BYTE, nullptr);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
  //                        gSpecular, 0);

  // // --- 3. Normal (float, high precision)
  // glGenTextures(1, &gNormal);
  // glBindTexture(GL_TEXTURE_2D, gNormal);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WIDTH, HEIGHT, 0, GL_RGBA,
  //              GL_FLOAT, nullptr);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D,
  //                        gNormal, 0);

  // // --- 4. Object Type (integer buffer)
  // glGenTextures(1, &gObjectType);
  // glBindTexture(GL_TEXTURE_2D, gObjectType);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, WIDTH, HEIGHT, 0, GL_RED_INTEGER,
  //              GL_INT, nullptr);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D,
  //                        gObjectType, 0);

  // // --- 5. Depth
  // glGenTextures(1, &gDepth);
  // glBindTexture(GL_TEXTURE_2D, gDepth);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, WIDTH, HEIGHT, 0,
  //              GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
  //                        gDepth, 0);

  // // Set draw buffers
  // GLenum attachments[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
  //                          GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
  // glDrawBuffers(4, attachments);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cerr << "GBuffer not complete!" << std::endl;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ForestScene::initLightContribution() {

  glGenFramebuffers(1, &lightFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);

  const unsigned int WIDTH = SHADOW_WIDTH;
  const unsigned int HEIGHT = SHADOW_HEIGHT;

  // ====================
  // 1. Light Diffuse
  // ====================
  glGenTextures(1, &lDiffuse);
  glBindTexture(GL_TEXTURE_2D, lDiffuse);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WIDTH, HEIGHT, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         lDiffuse, 0);

  // ====================
  // 2. Light Specular
  // ====================
  glGenTextures(1, &lSpecular);
  glBindTexture(GL_TEXTURE_2D, lSpecular);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WIDTH, HEIGHT, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                         lSpecular, 0);

  // ====================
  // 3. Ambient
  // ====================
  glGenTextures(1, &lAmbient);
  glBindTexture(GL_TEXTURE_2D, lAmbient);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WIDTH, HEIGHT, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D,
                         lAmbient, 0);

  glGenTextures(1, &lboDepth);
  glBindTexture(GL_TEXTURE_2D, lboDepth);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, WIDTH, HEIGHT, 0,
               GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         lboDepth, 0);

  // ====================
  // Set draw buffers
  // ====================
  GLenum attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                           GL_COLOR_ATTACHMENT2};
  glDrawBuffers(3, attachments);

  // ====================
  // Check
  // ====================
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cerr << "Light Contribution FBO not complete!" << std::endl;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ForestScene::initShadowMap() {

  glGenFramebuffers(1, &shadowFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);

  // Create depth texture
  glGenTextures(1, &shadowMap);
  glBindTexture(GL_TEXTURE_2D, shadowMap);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, // internal format
               SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT,
               GL_FLOAT, // external format & data type
               nullptr);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

  GLfloat borderColor[] = {1.0, 1.0, 1.0, 1.0};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

  // Attach to FBO
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         shadowMap, 0);

  // No color buffer
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cerr << "ShadowMap FBO not complete!" << std::endl;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ForestScene::renderFinalResult() {
  glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
  // glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glUseProgram(_resolve_deferred_shader);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gDiffuse);
  glUniform1i(glGetUniformLocation(_resolve_deferred_shader, "diffuse_texture"),
              0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, gSpecular);
  glUniform1i(
      glGetUniformLocation(_resolve_deferred_shader, "specular_texture"), 1);

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, lDiffuse);
  glUniform1i(glGetUniformLocation(_resolve_deferred_shader, "light_d_texture"),
              2);

  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, lSpecular);
  glUniform1i(glGetUniformLocation(_resolve_deferred_shader, "light_s_texture"),
              3);

  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, lAmbient);
  glUniform1i(glGetUniformLocation(_resolve_deferred_shader, "light_a_texture"),
              4);

  glActiveTexture(GL_TEXTURE5);
  glBindTexture(GL_TEXTURE_2D, gNormal);
  glUniform1i(glGetUniformLocation(_resolve_deferred_shader, "normal_texture"),
              5);

  glBindVertexArray(fullScreenVAO);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0u);
  // glBindSampler(3, 0u);
  // glBindSampler(2, 0u);
  // glBindSampler(1, 0u);
  // glBindSampler(0, 0u);
  glUseProgram(0u);
  glBindTexture(GL_TEXTURE_2D, 0);
}
void ForestScene::renderAllobjects(GLuint shaderProgram) {

  int label = 0;

  // ================
  // 1. 渲染地形
  // ================
  glUniform1i(glGetUniformLocation(shaderProgram, "lables"), label);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _grass_tex);
  glUniform1i(glGetUniformLocation(shaderProgram, "txture"), 0);

  glm::mat4 terrainWorld =
      glm::mat4(1.0f) * _quadNode.get_transform().GetMatrix();

  glUniformMatrix4fv(
      glGetUniformLocation(shaderProgram, "vertex_model_to_world"), 1, GL_FALSE,
      glm::value_ptr(terrainWorld));

  glBindVertexArray(_waveMesh.vao);
  glDrawElements(GL_TRIANGLES, _waveMesh.indices_nb, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);

  // // ================
  // // 2. 渲染树
  // // ================
  for (auto &t : _trees) {

    //   // 叶子 = 1，树干 = 2
    if (t.first.find("leaves") != std::string::npos)
      label = 1;
    else
      label = 2;

    glm::mat4 world = glm::mat4(1.0f) * t.second.get_transform().GetMatrix();

    glUniformMatrix4fv(
        glGetUniformLocation(shaderProgram, "vertex_model_to_world"), 1,
        GL_FALSE, glm::value_ptr(world));

    glUniform1i(glGetUniformLocation(shaderProgram, "lables"), label);

    // ========== 区分叶子和树干的 alpha mask ==========
    glActiveTexture(GL_TEXTURE0);
    if (label == 1) // leaves
    {
      glEnable(GL_CULL_FACE);
      glBindTexture(GL_TEXTURE_2D, _texLeafMask);
    }

    else // trunk
    {
      glDisable(GL_CULL_FACE);
      glBindTexture(GL_TEXTURE_2D, 0); // 没 mask
    }

    glUniform1i(glGetUniformLocation(shaderProgram, "txture_alpha"), 0);

    glActiveTexture(GL_TEXTURE1);
    if (label == 1) // leaves
      glBindTexture(GL_TEXTURE_2D, _texLeaf);
    else                                      // trunk
      glBindTexture(GL_TEXTURE_2D, _texBark); //

    glUniform1i(glGetUniformLocation(shaderProgram, "txture"), 1);
    glActiveTexture(GL_TEXTURE2);
    if (label == 1) // leaves
      glBindTexture(GL_TEXTURE_2D, _normalLeaf);
    else                                         // trunk
      glBindTexture(GL_TEXTURE_2D, _normalBark); //
    glUniform1i(glGetUniformLocation(shaderProgram, "normals_texture"), 2);

    glBindVertexArray(_tree_meshes[t.first].vao);
    glDrawElementsInstanced(GL_TRIANGLES, t.second.get_indices_nb(),
                            GL_UNSIGNED_INT, nullptr, _treeCount);
    glBindVertexArray(0);
  }

  // ================
  // 3. 渲染草
  // ================
  for (auto &g : _grass) {

    label = 3;

    glm::mat4 world = glm::mat4(1.0f) * g.second.get_transform().GetMatrix();

    glUniformMatrix4fv(
        glGetUniformLocation(shaderProgram, "vertex_model_to_world"), 1,
        GL_FALSE, glm::value_ptr(world));

    glUniform1i(glGetUniformLocation(shaderProgram, "lables"), label);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texGrassMask);
    glUniform1i(glGetUniformLocation(shaderProgram, "txture_alpha"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _texGrass);
    glUniform1i(glGetUniformLocation(shaderProgram, "txture"), 1);

    glBindVertexArray(_grass_meshes[g.first].vao);
    glDrawElementsInstanced(GL_TRIANGLES, g.second.get_indices_nb(),
                            GL_UNSIGNED_INT, nullptr, _grassCount);
    glBindVertexArray(0);
  }
}

void ForestScene::renderGbuffer() {
  // glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
  // glBindFramebuffer(GL_FRAMEBUFFER, gbufferFBO);
  // glClearDepthf(1.0f);
  // glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  // glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  glUseProgram(_gBufferShader);
  {

    glUniformMatrix4fv(
        glGetUniformLocation(_gBufferShader, "vertex_world_to_view"), 1,
        GL_FALSE, glm::value_ptr(_camera.GetWorldToViewMatrix()));
    glUniformMatrix4fv(
        glGetUniformLocation(_gBufferShader, "vertex_view_to_projection"), 1,
        GL_FALSE, glm::value_ptr(_camera.GetViewToClipMatrix()));
    glUniformMatrix4fv(
        glGetUniformLocation(_gBufferShader, "light_world_to_clip_matrix"), 1,
        GL_FALSE, glm::value_ptr(light_world_to_clip_matrix));
    glUniform3fv(glGetUniformLocation(_gBufferShader, "light_position"), 1,
                 glm::value_ptr(_lightPosition));
    glUniform3fv(glGetUniformLocation(_gBufferShader, "camera_position"), 1,
                 glm::value_ptr(_camera.mWorld.GetTranslation()));
    glUniform2f(
        glGetUniformLocation(_gBufferShader, "inverse_screen_resolution"),
        1.0f / static_cast<float>(SHADOW_WIDTH),
        1.0f / static_cast<float>(SHADOW_HEIGHT));
    glUniform3fv(glGetUniformLocation(_gBufferShader, "light_direction"), 1,
                 glm::value_ptr(lightgeometry.get_transform().GetFront()));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    glUniform1i(glGetUniformLocation(_gBufferShader, "shadow_texture"), 0);

    renderAllobjects(_gBufferShader);
  }
  // glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindVertexArray(0u);
  glUseProgram(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void ForestScene::renderShadowMap() {

  glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
  glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
  glClear(GL_DEPTH_BUFFER_BIT);
  glUseProgram(_shadowMapShader);

  glUniformMatrix4fv(
      glGetUniformLocation(_shadowMapShader, "light_world_to_clip_matrix"), 1,
      GL_FALSE, glm::value_ptr(light_world_to_clip_matrix));

  renderAllobjects(_shadowMapShader);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindVertexArray(0u);
  glUseProgram(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void ForestScene::renderLightContribution() {

  glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
  glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(_lightContributionShader);

  auto world =
      glm::mat4(glm::mat4(1.0f) * lightgeometry.get_transform().GetMatrix());
  glUniformMatrix4fv(
      glGetUniformLocation(_lightContributionShader, "vertex_model_to_world"),
      1, GL_FALSE, glm::value_ptr(world));

  glUniformMatrix4fv(glGetUniformLocation(_lightContributionShader,
                                          "light_world_to_clip_matrix"),
                     1, GL_FALSE, glm::value_ptr(light_world_to_clip_matrix));
  glUniformMatrix4fv(
      glGetUniformLocation(_lightContributionShader, "vertex_world_to_view"), 1,
      GL_FALSE, glm::value_ptr(_camera.GetWorldToViewMatrix()));
  glUniformMatrix4fv(glGetUniformLocation(_lightContributionShader,
                                          "vertex_view_to_projection"),
                     1, GL_FALSE,
                     glm::value_ptr(_camera.GetViewToClipMatrix()));

  glUniform3fv(glGetUniformLocation(_lightContributionShader, "light_position"),
               1, glm::value_ptr(_lightPosition));
  glUniform3fv(
      glGetUniformLocation(_lightContributionShader, "camera_position"), 1,
      glm::value_ptr(_camera.mWorld.GetTranslation()));
  glUniform2f(glGetUniformLocation(_lightContributionShader,
                                   "inverse_screen_resolution"),
              1.0f / static_cast<float>(SHADOW_WIDTH),
              1.0f / static_cast<float>(SHADOW_HEIGHT));
  glUniform3fv(
      glGetUniformLocation(_lightContributionShader, "light_direction"), 1,
      glm::value_ptr(lightgeometry.get_transform().GetFront()));

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, shadowMap);
  glUniform1i(glGetUniformLocation(_lightContributionShader, "shadow_texture"),
              0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, gNormal);
  glUniform1i(glGetUniformLocation(_lightContributionShader, "normal_texture"),
              1);

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, gDepth);
  glUniform1i(glGetUniformLocation(_lightContributionShader, "depth_texture"),
              2);

  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, gObjectType);
  glUniform1i(glGetUniformLocation(_lightContributionShader, "object_type"), 3);

  glBindVertexArray(lightMesh.vao);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindVertexArray(0u);
  glUseProgram(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

bool ForestScene::setup() {

  // ----------------------------------------------------------------
  // 1. 加载 Shader
  // ----------------------------------------------------------------
  _programManager.CreateAndRegisterProgram(
      "Fallback",
      {{ShaderType::vertex, "default.vert"},
       {ShaderType::fragment, "default.frag"}},
      _fallbackShader);

  _programManager.CreateAndRegisterProgram(
      "Wave",
      {{ShaderType::vertex, "wave.vert"}, {ShaderType::fragment, "wave.frag"}},
      _waveShader);
  if (_waveShader == 0u) {
    LogError("Failed to load _waveShader shader");
    return -1;
  }
  _programManager.CreateAndRegisterProgram(
      "shadowMapShader",
      {{ShaderType::vertex, "shadowDepth.vert"},
       {ShaderType::fragment, "shadowDepth.frag"}},
      _shadowMapShader);
  if (_shadowMapShader == 0u) {
    LogError("Failed to load shadowMapShader shader");
    return -1;
  }
  _programManager.CreateAndRegisterProgram(
      "gBufferShader",
      {{ShaderType::vertex, "Gbuffer.vert"},
       {ShaderType::fragment, "Gbuffer.frag"}},
      _gBufferShader);
  if (_gBufferShader == 0u) {
    LogError("Failed to load _gBufferShader shader");
    return -1;
  }
  _programManager.CreateAndRegisterProgram(
      "lightContributionShader",
      {{ShaderType::vertex, "ligihtContribution.vert"},
       {ShaderType::fragment, "ligihtContribution.frag"}},
      _lightContributionShader);
  if (_lightContributionShader == 0u) {
    LogError("Failed to load _lightContributionShader shader");
    return -1;
  }
  _programManager.CreateAndRegisterProgram(
      "Resolve deferred",
      {{ShaderType::vertex, "resolve_deferred.vert"},
       {ShaderType::fragment, "resolve_deferred.frag"}},
      _resolve_deferred_shader);
  if (_resolve_deferred_shader == 0u) {
    LogError("Failed to load deferred resolution shader");
    return -1;
  }
  _programManager.CreateAndRegisterProgram(
      "grassShader",
      {{ShaderType::vertex, "grass.vert"},
       {ShaderType::fragment, "grass.frag"}},
      _grassShader);
  GLuint tessHeightMapShader;
  _programManager.CreateAndRegisterProgram(
      "tessHeightMap",
      {{ShaderType::vertex, "terrain.vert"},
       {ShaderType::tess_ctrl, "terrain.tcs"},
       {ShaderType::tess_eval, "terrain.tes"},
       {ShaderType::fragment, "terrain.frag"}},
      _tessHeightMapShader);

  if (_tessHeightMapShader == 0) {
    LogError("11Failed to load shaders.");
    return false;
  }
  if (_fallbackShader == 0 || _tessHeightMapShader == 0 || _grassShader == 0) {
    LogError("Failed to load shaders.");
    return false;
  }

  // ----------------------------------------------------------------
  // 2. 准备 Instancing VBO Setup 函数
  // ----------------------------------------------------------------
  // 先生成矩阵数据
  auto tree_matrices = generateTreeTransforms(_treeCount);

  // 把矩阵上传到 VBO
  glGenBuffers(1, &_instanceVBO);
  glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, _treeCount * sizeof(glm::mat4),
               tree_matrices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0); // 解绑

  // 定义一个 Lambda，用于在 loadObjects 时回调配置 VAO
  auto setupInstanceVBO = [this]() -> GLuint {
    glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);

    size_t vec4Size = sizeof(glm::vec4);

    int baseLocation = 7; // <--- 从 7 开始，避开 tangent(3) 和 binormal(4)
    for (int i = 0; i < 4; i++) {
      glEnableVertexAttribArray(baseLocation + i);
      glVertexAttribPointer(baseLocation + i, 4, GL_FLOAT, GL_FALSE,
                            sizeof(glm::mat4), (void *)(i * vec4Size));
      glVertexAttribDivisor(baseLocation + i, 1);
    }

    // 返回 _instanceVBO 的 ID，
    return _instanceVBO;
  };

  // ----------------------------------------------------------------
  // 3. 加载模型 (传入 setupInstanceVBO 回调)
  // ----------------------------------------------------------------
  // 把矩阵上传到 VBO

  std::vector<bonobo::mesh_data> tree_meshes = bonobo::loadObjects(
      config::resources_path("47-mapletree/MapleTree.obj"), setupInstanceVBO);

  if (tree_meshes.empty()) {
    LogError("Failed to load res/MapleTree.obj");
    return false;
  }
  auto grass_matrices = generateTreeTransforms(_grassCount, 100, 100);
  glGenBuffers(1, &_instanceVBO);
  glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, _grassCount * sizeof(glm::mat4),
               grass_matrices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0); // 解绑
  std::vector<bonobo::mesh_data> grass_meshes = bonobo::loadObjects(
      config::resources_path("91-trava-kolosok/TravaKolosok.obj"),
      setupInstanceVBO);
  if (grass_meshes.empty()) {
    LogError("Failed to load res/MapleTree.obj");
    return false;
  }
  // ----------------------------------------------------------------
  // 4. 加载纹理
  // ----------------------------------------------------------------
  GLuint maple_bark = bonobo::loadTexture2D(
      config::resources_path("47-mapletree/maple_bark.png"));
  GLuint maple_leaf = bonobo::loadTexture2D(
      config::resources_path("47-mapletree/maple_leaf.png"));
  GLuint leaves_alpha = bonobo::loadTexture2D(
      config::resources_path("47-mapletree/maple_leaf_Mask.jpg"));
  GLuint maple_leaf_normal = bonobo::loadTexture2D(
      config::resources_path("47-mapletree/maple_leaf_normal.png"));
  GLuint maple_bark_normal = bonobo::loadTexture2D(
      config::resources_path("47-mapletree/maple_bark_normal.png"));
  GLuint grass_leaf = bonobo::loadTexture2D(
      config::resources_path("91-trava-kolosok/TravaKolosok.jpg"));
  GLuint grass_alpha = bonobo::loadTexture2D(
      config::resources_path("91-trava-kolosok/TravaKolosokCut.jpg"));
  _texLeafMask = leaves_alpha;
  _texGrassMask = grass_alpha;
  _texGrass = grass_leaf;
  _texLeaf = maple_leaf;
  _texBark = maple_bark;
  _normalBark = maple_bark_normal;
  _normalLeaf = maple_leaf_normal;
  // ----------------------------------------------------------------
  // 5. 构建 Node 结构
  // ----------------------------------------------------------------
  auto p = _camera.mWorld.GetTranslation();
  // 定义 Uniform 设置回调
  auto set_uniforms = [this, &p](GLuint program) {
    glUniform1i(glGetUniformLocation(program, "is_leaves"),
                _isLeavesMesh ? 1 : 0);
    glUniform3fv(glGetUniformLocation(program, "light_position"), 1,
                 glm::value_ptr(_lightPosition));
    glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
                 glm::value_ptr(_camera.mWorld.GetTranslation()));
    glUniform1f(glGetUniformLocation(program, "elapsed_time_s"), _elapsedTimeS);
  };

  // 遍历所有 Mesh，创建 Node
  for (auto &obj : tree_meshes) {
    Node node;
    node.set_geometry(obj);

    // 设置 Shader 和 Uniform 回调
    node.set_program(&_fallbackShader, set_uniforms);

    // 添加纹理 (Node 会自动绑定到 Texture Unit 0, 1, 2...)
    node.add_texture("maple_bark", maple_bark, GL_TEXTURE_2D);
    node.add_texture("maple_leaf", maple_leaf, GL_TEXTURE_2D);
    node.add_texture("leaves_alpha", leaves_alpha, GL_TEXTURE_2D);
    node.add_texture("maple_leaf_normal", maple_leaf_normal, GL_TEXTURE_2D);
    node.add_texture("maple_bark_normal", maple_bark_normal, GL_TEXTURE_2D);
    _trees.insert({obj.name, node});
    _tree_meshes.insert({obj.name, obj});
  }

  for (auto &obj : grass_meshes) {
    Node node;
    node.set_geometry(obj);
    std::cout << "grass_meshes" << std::endl;
    // 设置 Shader 和 Uniform 回调
    node.set_program(&_grassShader, set_uniforms);

    // 添加纹理 (Node 会自动绑定到 Texture Unit 0, 1, 2...)
    node.add_texture("grass_texture", grass_leaf, GL_TEXTURE_2D);
    node.add_texture("grass_alpha", grass_alpha, GL_TEXTURE_2D);

    _grass.insert({obj.name, node});
    _grass_meshes.insert({obj.name, obj});
  }
  // ----------------------------------------------------------------
  // 6. 配置 Wave 地面 Node
  // ----------------------------------------------------------------
  _waveMesh = parametric_shapes::createQuad(100.0f, 100.0f, 1000, 1000);

  auto wave_uniforms = [this](GLuint program) {
    glUniform1i(glGetUniformLocation(program, "use_normal_mapping"), 0);
    glUniform3fv(glGetUniformLocation(program, "light_position"), 1,
                 glm::value_ptr(_lightPosition));
    glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
                 glm::value_ptr(_camera.mWorld.GetTranslation()));
    glUniform1f(glGetUniformLocation(program, "elapsed_time_s"), _elapsedTimeS);
    // 材质参数
    glUniform3f(glGetUniformLocation(program, "ambient"), 0.1f, 0.1f, 0.1f);
    glUniform3f(glGetUniformLocation(program, "diffuse"), 0.7f, 0.2f, 0.4f);
    glUniform3f(glGetUniformLocation(program, "specular"), 1.0f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(program, "shininess"), 10.0f);
  };

  auto light_uniforms = [this](GLuint program) {
    glUniformMatrix4fv(glGetUniformLocation(program, "vertex_world_to_view"), 1,
                       GL_FALSE,
                       glm::value_ptr(_camera.GetWorldToViewMatrix()));
    glUniformMatrix4fv(
        glGetUniformLocation(program, "vertex_view_to_projection"), 1, GL_FALSE,
        glm::value_ptr(_camera.GetViewToClipMatrix()));
  };

  _quadNode.set_geometry(_waveMesh);
  _quadNode.set_program(&_waveShader, wave_uniforms);

  GLuint floor_tex = bonobo::loadTexture2D(
      config::resources_path("forested-floor/textures/KiplingerFLOOR.png"));
  _quadNode.add_texture("diffuse_texture", floor_tex, GL_TEXTURE_2D);

  _terrain_Uniforms = [this](GLuint program) {
    glUniformMatrix4fv(glGetUniformLocation(program, "vertex_model_to_world"),
                       1, GL_FALSE, glm::value_ptr(this->_terrain_world));
    glUniformMatrix4fv(glGetUniformLocation(program, "vertex_world_to_view"), 1,
                       GL_FALSE,
                       glm::value_ptr(_camera.GetWorldToViewMatrix()));
    glUniformMatrix4fv(
        glGetUniformLocation(program, "vertex_view_to_projection"), 1, GL_FALSE,
        glm::value_ptr(_camera.GetViewToClipMatrix()));
  };
  _grass_tex = bonobo::loadTexture2D(
      config::resources_path("forested-floor/textures/KiplingerFLOOR.png"));
  _floor_tex = bonobo::loadTexture2D(
      config::resources_path("forested-floor/textures/iceland_heightmap.png"));
  //   _quadNode.add_texture("diffuse_texture", _floor_tex, GL_TEXTURE_2D);
  createQuadsForPatch();

  initSkybox();

  glGenVertexArrays(1, &fullScreenVAO);
  glBindVertexArray(fullScreenVAO);
  createLight();
  initLightMatrix();
  // initGbuffer();
  initShadowMap();
  // initLightContribution();
  lightgeometry.set_program(&_lightContributionShader, light_uniforms);

  // ----------------------------------------------------------------
  // 7. GL 状态
  // ----------------------------------------------------------------
  glClearDepthf(1.0f);
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  bonobo::changeCullMode(_cullMode);

  return true;
}

void ForestScene::update(double deltaTimeUs) {
  _inputHandler.Advance();
  _camera.Update(std::chrono::microseconds((long)deltaTimeUs), _inputHandler);
  _elapsedTimeS += (float)(deltaTimeUs / 1000000.0);

  if (!_isPaused) {
    float dt = (float)(deltaTimeUs / 1000000.0);
    _sunTime += dt * _daySpeed;
  }
  // 新增 模拟太阳轨道
  float daySpeed = 0.5f; // 控制时间流逝速度
  float sunRadius =
      100.0f; // 太阳距离 (对于方向光，这个值只影响方向，不影响衰减)

  // 利用 sin/cos 让太阳绕 Z 轴旋转 (模拟东升西落)
  // 假设太阳从 X 正方向(东)升起，到 Y 正方向(正午)，落向 X 负方向(西)
  // timeOffset 用来调整初始时间，让程序一开始是白天
  _lightPosition.x = sin(_sunTime) * sunRadius; // 东西移动
  _lightPosition.y = cos(_sunTime) * sunRadius; // 上下移动
  _lightPosition.z = 10.0f; // 稍微偏南或偏北一点，产生好看的阴影角度
  // lightgeometry.get_transform().SetTranslate(_lightPosition);

  if (_inputHandler.GetKeycodeState(GLFW_KEY_F2) & JUST_RELEASED)
    _showGui = !_showGui;
}

void ForestScene::render(GLFWwindow *window) {
  int w, h;
  glfwGetFramebufferSize(window, &w, &h);
  glViewport(0, 0, w, h);

  _windowManager.NewImGuiFrame();
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

  bonobo::changePolygonMode(_polygonMode);
  // 1. 渲染地面
  // Node::render 通常接受 (VP矩阵, Model矩阵)

  {

    _terrain_world =
        glm::mat4(glm::mat4(1.0f) * _quadNode.get_transform().GetMatrix());
    // _quadNode.render(_camera.GetWorldToClipMatrix(), glm::mat4(1.0f));
  }

  // 2. 渲染树木
  for (auto &t : _trees) {
    // 根据名字判断是否为树叶，并设置状态变量
    // 这个变量会被上面定义的 set_uniforms lambda 捕获并传给 shader
    if (t.first.find("leaves") != std::string::npos) {
      _isLeavesMesh = true;

      glDisable(GL_CULL_FACE); // 树叶双面渲染
    } else {
      _isLeavesMesh = false;
      glEnable(GL_CULL_FACE);
    }

    // 调用 Node 的渲染
    // t.second.render(_camera.GetWorldToClipMatrix(), glm::mat4(1.0f),
    //                 _treeCount);
  }
  for (auto &g : _grass) {
    // glDisable(GL_CULL_FACE);
    // g.second.render(_camera.GetWorldToClipMatrix(), glm::mat4(1.0f),
    //                 _grassCount);
  }
  // lightgeometry.render(_camera.GetWorldToClipMatrix(), glm::mat4(1.0f));

  renderShadowMap();
  renderSkybox(_camera.GetWorldToViewMatrix(), _camera.GetViewToClipMatrix());
  renderGbuffer();

  // renderLightContribution();
  // renderFinalResult();
  // 恢复 Cull Mode
  bonobo::changeCullMode(_cullMode);

  // 3. ImGui
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  bool opened = ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_None);
  if (opened) {
    if (bonobo::uiSelectCullMode("Cull mode", _cullMode)) {
      bonobo::changeCullMode(_cullMode);
    }
    bonobo::uiSelectPolygonMode("Polygon mode", _polygonMode);

    ImGui::Separator(); // 画一条分割线
    ImGui::Text("Sun Control");

    // 暂停复选框
    ImGui::Checkbox("Pause Sun", &_isPaused);

    // 速度滑条控制太阳走得快还是慢
    ImGui::SliderFloat("Speed", &_daySpeed, 0.0f, 2.0f);

    // 手动时间滑条
    if (ImGui::SliderFloat("Time of Day", &_sunTime, 0.0f, 6.28f)) {

      _isPaused = true;
    }
    ImGui::Text("Time: %.2f", _elapsedTimeS);
  }

  ImGui::End();

  _windowManager.RenderImGuiFrame(_showGui);
}

void ForestScene::initSkybox() {
  // 1. 加载 Shader
  _programManager.CreateAndRegisterProgram(
      "Skybox",
      {{ShaderType::vertex, "skybox.vert"},
       {ShaderType::fragment, "skybox.frag"}},
      _skyboxShader);

  // 2. 创建一个简单的立方体 (只需要位置，-1 到 1)
  float skyboxVertices[] = {
      // positions
      -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
      1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

      -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
      -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

      1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
      1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

      -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
      1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

      -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
      1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

      -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
      1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

  glGenVertexArrays(1, &_skyboxVAO);
  glGenBuffers(1, &_skyboxVBO);
  glBindVertexArray(_skyboxVAO);
  glBindBuffer(GL_ARRAY_BUFFER, _skyboxVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices,
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
}

void ForestScene::renderSkybox(glm::mat4 const &view,
                               glm::mat4 const &projection) {
  // 关闭深度测试和深度写入
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  // 关闭剔除
  glDisable(GL_CULL_FACE);

  glUseProgram(_skyboxShader);

  // 2. 去掉 View 矩阵的平移部分
  glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
  glm::mat4 viewProj = projection * viewNoTrans;

  glUniformMatrix4fv(
      glGetUniformLocation(_skyboxShader, "vertex_world_to_clip"), 1, GL_FALSE,
      glm::value_ptr(viewProj));

  // 3. 传入太阳位置 (必须和渲染树木的一样)
  glUniform3fv(glGetUniformLocation(_skyboxShader, "light_position"), 1,
               glm::value_ptr(_lightPosition));

  // 4. 绘制
  glBindVertexArray(_skyboxVAO);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);
  glEnable(GL_CULL_FACE);

  // 5. 恢复状态
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST); // 重新开启深度测试
  glDepthMask(GL_TRUE);    // 重新开启深度写入
  glDepthFunc(GL_LESS);    // 恢复默认规则
}
