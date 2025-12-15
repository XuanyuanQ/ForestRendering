#include "ForestScene.hpp"
#include "config.hpp"
#include "core/helpers.hpp"
#include "core/parametric_shapes.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>
#include <random>

ForestScene::ForestScene(WindowManager &windowManager)
    : _windowManager(windowManager),
      _camera(0.5f * glm::half_pi<float>(),
              static_cast<float>(config::resolution_x) /
                  static_cast<float>(config::resolution_y),
              0.01f, 1000.0f),
      _isPaused(false),
      _applyShadow(false), _sunTime(0.0f),
      _daySpeed(0.5f),
      _isWindEnabled(false), _windStrength(0.5f) {
  _isVolumetricLight = false;
  _camera.mWorld.SetTranslate(glm::vec3(0.0f, 10.0f, 20.0f));
  _camera.mMouseSensitivity = glm::vec2(0.003f);
  _camera.mMovementSpeed = glm::vec3(3.0f);
  _particel_count = 150;
  _treeCount = 15;
  _grassCount = 100;
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
}
GLuint ForestScene::createQuadsForPatch() {
  float planeVertices[] = {
      // positions            // normals         // texcoords
      25.0f,  -0.5f, 25.0f,  0.0f, 1.0f, 0.0f, 25.0f, 0.0f,
      -25.0f, -0.5f, 25.0f,  0.0f, 1.0f, 0.0f, 0.0f,  0.0f,
      -25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 0.0f,  25.0f,

      25.0f,  -0.5f, 25.0f,  0.0f, 1.0f, 0.0f, 25.0f, 0.0f,
      -25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 0.0f,  25.0f,
      25.0f,  -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 25.0f, 25.0f};
  // plane VAO
  unsigned int planeVBO;
  unsigned int planeVAO;
  glGenVertexArrays(1, &planeVAO);
  glGenBuffers(1, &planeVBO);
  glBindVertexArray(planeVAO);
  glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices,
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(6 * sizeof(float)));
  glBindVertexArray(0);
  _terrainVao = planeVAO;
  _terrainVbo = planeVBO;

  if (cubeVAO == 0) {
    float vertices[] = {
        // back face
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
        1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,  // bottom-right
        1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,  // top-left
        // front face
        -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
        1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // bottom-right
        1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
        1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
        -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // top-left
        -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
        // left face
        -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
        -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // top-left
        -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
        -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
        -1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
        -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
                                                            // right face
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,     // top-left
        1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   // bottom-right
        1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,    // top-right
        1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   // bottom-right
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,     // top-left
        1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,    // bottom-left
        // bottom face
        -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
        1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,  // top-left
        1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
        1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
        -1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
        -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
        // top face
        -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
        1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
        1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // top-right
        1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
        -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
        -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f   // bottom-left
    };
    // vertices = vertices * 0.1;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    // fill buffer
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // link vertex attributes
    glBindVertexArray(cubeVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void *)(6 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
  }
  return _terrainVao;
}

std::vector<InstanceData>
ForestScene::generateTreeTransforms(int count, int Width, int Depth) {
  std::vector<InstanceData> InstanceDatas;
  InstanceDatas.reserve(count);
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
    (float)rand() / RAND_MAX;
    InstanceData data;
    data.modelMatrix = model;
    data.windSpeed = (float)rand() / RAND_MAX;

    InstanceDatas.push_back(data);
  }
  return InstanceDatas;
}


void ForestScene::updateLightMatrix(const glm::vec3 light_pos) {

  // Defines the size of the area covered by the shadow.
  float boxSize = 150.0f;

  // Near/Far
  // float nearPlane = 1.0f;
  // float farPlane = 3000.0f;
  float nearPlane = 1.0f, farPlane = 1000.5f;
  auto lightProjection = glm::ortho(-boxSize, boxSize, -boxSize, boxSize, nearPlane, farPlane);

  glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f); // center of forest
  glm::mat4 lightView = glm::lookAt(light_pos, target, glm::vec3(0.0, 1.0, 0.2));

  light_world_to_clip_matrix = lightProjection * lightView;
}


void ForestScene::initGbuffer() {

  glGenFramebuffers(1, &gbufferFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, gbufferFBO);

 
  // Create and configure depth textures
  glGenTextures(1, &gDepth);
  glBindTexture(GL_TEXTURE_2D, gDepth);


  // Use GL_DEPTH24_STENCIL8
  // floating point depth provides higher precision and reduces ripple
  // when reconstructing world coordinates over long distances.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, gbufffer_w, gbufffer_h, 0,
               GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

  // Filtering
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Wrapping
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Bind textures to depth attachment points of FBOs
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         gDepth, 0);

  glReadBuffer(GL_NONE);
  // ---------------------------------------------
  // 4.Check integrity
  // ---------------------------------------------
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!"
              << std::endl;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ForestScene::initShadowMap() {
  // 1. create FBO
  glGenFramebuffers(1, &shadowFBO);

  // 2. crete depth texture
  glGenTextures(1, &shadowMap);
  glBindTexture(GL_TEXTURE_2D, shadowMap);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH,
               SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

  // basic parameter
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);


  // 3. bind FBO
  glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         shadowMap, 0);

  // 4. disable color read/write
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "ShadowMap FBO Error!" << std::endl;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


//renders a falling leaf particle effect. It uses Instancing technology.
void ForestScene::renderPartical(GLuint shaderProgram) {
	glUseProgram(shaderProgram);

	//Bind the color texture of the leaves
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _texLeaf);
	glUniform1i(glGetUniformLocation(shaderProgram, "txture"), 0);

	//Bind the transparency mask of the leaf
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, _texLeafMask);
	glUniform1i(glGetUniformLocation(shaderProgram, "mask"), 1);

	//Passing the MVP matrix
	glm::mat4 model = glm::mat4(1.0f);
	glUniformMatrix4fv(
	  glGetUniformLocation(shaderProgram, "vertex_model_to_world"), 1, GL_FALSE,
	  glm::value_ptr(model));
	glUniformMatrix4fv(
	  glGetUniformLocation(shaderProgram, "vertex_world_to_view"), 1, GL_FALSE,
	  glm::value_ptr(_camera.GetWorldToViewMatrix()));
	glUniformMatrix4fv(
	  glGetUniformLocation(shaderProgram, "vertex_view_to_projection"), 1,
	  GL_FALSE, glm::value_ptr(_camera.GetViewToClipMatrix()));
	
	//Set particle motion parameters
	glm::vec3 u_TreeCrownCenter(20.0f, 43.0f, 20.0f);
	glUniform1f(glGetUniformLocation(shaderProgram, "u_Time"), _elapsedTimeS);
	glUniform3fv(glGetUniformLocation(shaderProgram, "u_TreeCrownCenter"), 1,
			   glm::value_ptr(u_TreeCrownCenter));
	glm::vec3 u_TreeCrownSize(10.0f, 20.0f, 10.0f);
	glUniform3fv(glGetUniformLocation(shaderProgram, "u_TreeCrownSize"), 1,
			   glm::value_ptr(u_TreeCrownSize));

	//Lighting parameters
	glUniform3fv(glGetUniformLocation(shaderProgram, "light_position"), 1,
			   glm::value_ptr(_lightPosition));
	glUniform3fv(glGetUniformLocation(shaderProgram, "camera_position"), 1,
			   glm::value_ptr(_camera.mWorld.GetTranslation()));

	//Draw Call
	glBindVertexArray(_particelMesh.vao);
	glDrawElementsInstanced(GL_TRIANGLES, _particelMesh.indices_nb,
						  GL_UNSIGNED_INT, nullptr, _particel_count);
	glBindVertexArray(0);
}


void ForestScene::renderAllobjects(GLuint shaderProgram) {
 
	glUniform1f(glGetUniformLocation(shaderProgram, "elapsed_time_s"),
              _elapsedTimeS);
  float currentWind = _isWindEnabled ? _windStrength : 0.0f;
  glUniform1f(glGetUniformLocation(shaderProgram, "wind_strength"),
              currentWind * 0.1);

  int label = 0;

  // ================
  // 1. render Terrain
  // ================
  glUniform1i(glGetUniformLocation(shaderProgram, "lables"), label);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, _grass_tex);
  glUniform1i(glGetUniformLocation(shaderProgram, "txture"), 1);

  glm::mat4 terrainWorld =
      glm::mat4(1.0f) * _quadNode.get_transform().GetMatrix();

  glUniformMatrix4fv(
      glGetUniformLocation(shaderProgram, "vertex_model_to_world"), 1, GL_FALSE,
      glm::value_ptr(terrainWorld));

  glBindVertexArray(_waveMesh.vao);
  glDrawElements(GL_TRIANGLES, _waveMesh.indices_nb, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);

  // // ================
  // // 2. render tree
  // // ================
  for (auto &t : _trees) {
    if (t.first.find("leaves") != std::string::npos)
      label = 1;
    else
      label = 2;

    glm::mat4 world = glm::mat4(1.0f) * t.second.get_transform().GetMatrix();

    glUniformMatrix4fv(
        glGetUniformLocation(shaderProgram, "vertex_model_to_world"), 1,
        GL_FALSE, glm::value_ptr(world));

    glUniform1i(glGetUniformLocation(shaderProgram, "lables"), label);

    // ========== Alpha mask to distinguish between leaves and trunk==========
    glActiveTexture(GL_TEXTURE0);
    if (label == 1) // leaves
    {
      glEnable(GL_CULL_FACE);
      glBindTexture(GL_TEXTURE_2D, _texLeafMask);
    }

    else // trunk
    {
      // glDisable(GL_CULL_FACE);
      glEnable(GL_CULL_FACE);
      glBindTexture(GL_TEXTURE_2D, 0); //
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
  // 3. render grass
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
    glUniform1i(glGetUniformLocation(_gBufferShader, "isApplyShadow"),
                _applyShadow);
    glUniform1i(glGetUniformLocation(_gBufferShader, "isVolumetricLight"),
                _isVolumetricLight);

    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    glUniform1i(glGetUniformLocation(_gBufferShader, "shadow_texture"), 10);
    // rendtest(_gBufferShader);

    renderAllobjects(_gBufferShader);
  }
  // glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindVertexArray(0u);
  glUseProgram(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void ForestScene::renderShadowMap(GLuint FBO) {
  glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
  if (FBO == gbufferFBO) {
    glViewport(0, 0, gbufffer_w, gbufffer_h);
  }
  // glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, FBO);
  glUseProgram(_shadowMapShader);
  // glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glClearDepth(1.0f);
  // glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_DEPTH_BUFFER_BIT);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cout << "FBO ERROR!" << std::endl;
  }
  // std::cout << "ShadowMap Size: " << SHADOW_WIDTH << " x " << SHADOW_HEIGHT
  //           << std::endl;

  // renderPartical(_particelShader);

  // glCullFace(GL_FRONT);

  bool isGbufferDepth = false;
  if (FBO == gbufferFBO) {
    isGbufferDepth = true;
  }

  glUniform1i(glGetUniformLocation(_shadowMapShader, "isGbufferDepth"),
              int(isGbufferDepth));

  glUniformMatrix4fv(
      glGetUniformLocation(_shadowMapShader, "light_world_to_clip_matrix"), 1,
      GL_FALSE, glm::value_ptr(light_world_to_clip_matrix));
  glm::mat4 viewMat = _camera.GetWorldToViewMatrix();
  glm::mat4 projMat = _camera.GetViewToClipMatrix();

  glUniformMatrix4fv(
      glGetUniformLocation(_shadowMapShader, "vertex_world_to_view"), 1,
      GL_FALSE, glm::value_ptr(viewMat));
  glUniformMatrix4fv(
      glGetUniformLocation(_shadowMapShader, "vertex_view_to_projection"), 1,
      GL_FALSE, glm::value_ptr(_camera.GetViewToClipMatrix()));

  renderAllobjects(_shadowMapShader);
  // rendtest(_shadowMapShader);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindVertexArray(0u);
  glUseProgram(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  // glEnable(GL_CULL_FACE);
  // glCullFace(GL_BACK);
}


bool ForestScene::setup(GLFWwindow *window) {

  // ----------------------------------------------------------------
  // 1. Load Shader
  // ----------------------------------------------------------------

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
      "_particelShader",
      {{ShaderType::vertex, "particle.vert"},
       {ShaderType::fragment, "particle.frag"}},
      _particelShader);
  if (_particelShader == 0u) {
    LogError("Failed to load _particelShader shader");
    return -1;
  }



  // ----------------------------------------------------------------
  // 2. Instancing VBO Setup
  // ----------------------------------------------------------------
  // Generate  _treeCount position, rotation, and scaling matrices.
  std::vector<InstanceData> instances =
      generateTreeTransforms(_treeCount, 100, 100);

  // upload matrix to VBO
  glGenBuffers(1, &_instanceVBO);
  glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(InstanceData),
               instances.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0); // unbind

  // Define a lambda to be used to configure the VAO when loading Objects.
  auto setupInstanceVBO = [this]() -> GLuint {
    glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);

    size_t vec4Size = sizeof(glm::vec4);
    GLsizei stride = sizeof(InstanceData);
    int baseLocation = 7; // <--- Starting from 7, avoid tangent(3) and binormal(4)
    for (int i = 0; i < 4; i++) {
      glEnableVertexAttribArray(baseLocation + i);
      glVertexAttribPointer(baseLocation + i, 4, GL_FLOAT, GL_FALSE, stride,
                            (void *)(i * vec4Size));
      glVertexAttribDivisor(baseLocation + i, 1);
    }
    int windLocation = baseLocation + 4; // Location 11
    glEnableVertexAttribArray(windLocation);
    glVertexAttribPointer(
        windLocation, 1, GL_FLOAT, GL_FALSE, stride,
        (void *)(sizeof(glm::mat4)) // Offset: Skip 64 bytes of the matrix
    );
    glVertexAttribDivisor(windLocation, 1);
    // return _instanceVBO  ID，
    return _instanceVBO;
  };

  // Mesh Loading
  std::vector<bonobo::mesh_data> tree_meshes = bonobo::loadObjects(
      config::resources_path("47-mapletree/MapleTree.obj"), setupInstanceVBO);

  if (tree_meshes.empty()) {
    LogError("Failed to load res/MapleTree.obj");
    return false;
  }
	
  
  auto grass_matrices = generateTreeTransforms(_grassCount, 80, 80);
  glGenBuffers(1, &_instanceVBO);
  glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, grass_matrices.size() * sizeof(InstanceData),
               grass_matrices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	std::vector<bonobo::mesh_data> grass_meshes = bonobo::loadObjects(
		config::resources_path("91-trava-kolosok/TravaKolosok.obj"),
		setupInstanceVBO);
	if (grass_meshes.empty()) {
	  LogError("Failed to load res/MapleTree.obj");
	  return false;
	}


  auto particelVBO = [this]() -> GLuint {
    glBindBuffer(GL_ARRAY_BUFFER, _particelVBO);

    size_t vec4Size = sizeof(glm::vec4);
    GLsizei stride = sizeof(InstanceData);
    int baseLocation = 7; //  <--- Starting from 7, avoid tangent(3) and binormal(4)
    for (int i = 0; i < 4; i++) {
      glEnableVertexAttribArray(baseLocation + i);
      glVertexAttribPointer(baseLocation + i, 4, GL_FLOAT, GL_FALSE, stride,
                            (void *)(i * vec4Size));
      glVertexAttribDivisor(baseLocation + i, 1);
    }
    int windLocation = baseLocation + 4; // Location 11
    glEnableVertexAttribArray(windLocation);
    glVertexAttribPointer(
        windLocation, 1, GL_FLOAT, GL_FALSE, stride,
        (void *)(sizeof(glm::mat4))
    );
    glVertexAttribDivisor(windLocation, 1);
    return _particelVBO;
  };

  auto particel_matrices = generateTreeTransforms(_particel_count, 80, 80);
  glGenBuffers(1, &_particelVBO);
  glBindBuffer(GL_ARRAY_BUFFER, _particelVBO);
  glBufferData(GL_ARRAY_BUFFER, particel_matrices.size() * sizeof(InstanceData),
               particel_matrices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  _particelMesh =
      parametric_shapes::createQuad(100.0f, 100.0f, 10, 10, particelVBO);
  _frogMesh = parametric_shapes::createQuad(100.0f, 100.0f, 1, 1);

 
  // ----------------------------------------------------------------
  // 4. load texture
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
  // 5. construct Node Structure
  // ----------------------------------------------------------------
  auto p = _camera.mWorld.GetTranslation();
  for (auto &obj : tree_meshes) {
    Node node;
    node.set_geometry(obj);
    _trees.insert({obj.name, node});
    _tree_meshes.insert({obj.name, obj});
  }

  for (auto &obj : grass_meshes) {
    Node node;
    node.set_geometry(obj);
    std::cout << "grass_meshes" << std::endl;
  
    _grass.insert({obj.name, node});
    _grass_meshes.insert({obj.name, obj});
  }

  _waveMesh = parametric_shapes::createQuad(100.0f, 100.0f, 1000, 1000);
  _quadNode.set_geometry(_waveMesh);


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
  glfwGetFramebufferSize(window, &gbufffer_w, &gbufffer_h);
  initShadowMap();
  initGbuffer();


  glClearDepthf(1.0f);
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  bonobo::changeCullMode(_cullMode);

  return true;
}

void ForestScene::update(double deltaTimeUs) {
	//Input and camera updates
  _inputHandler.Advance();
  _camera.Update(std::chrono::microseconds((long)deltaTimeUs), _inputHandler);
  _elapsedTimeS += (float)(deltaTimeUs / 1000000.0);

  //Day/Night Cycle
  if (!_isPaused) {
    float dt = (float)(deltaTimeUs / 1000000.0);
    _sunTime += dt * _daySpeed;
  }
  // Simulated solar orbit
  float daySpeed = 0.5f;
  float sunRadius = 100.0f; // Sun distance (For directional light, this value only affects direction, not attenuation)
  float x_factor = 2.0;
  float y_factor = 4.0;

  if (!_isPaused) {
    _lightPosition.x = sin(_sunTime) * sunRadius;
    _lightPosition.y = cos(_sunTime) * sunRadius;
    _lightPosition.z = -100.0f;
  } else {
    _lightPosition = glm::vec3(lightX, lightY, lightZ);
  }

  updateLightMatrix(_lightPosition);
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

  renderShadowMap(shadowFBO);
  renderShadowMap(gbufferFBO);
  
  glViewport(0, 0, w, h);
  renderSkybox(_camera.GetWorldToViewMatrix(), _camera.GetViewToClipMatrix());
  renderGbuffer();
  renderPartical(_particelShader);
  glEnable(GL_BLEND);

  glBlendFunc(GL_ONE, GL_ONE);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  
  bonobo::changeCullMode(_cullMode);

  // 3. ImGui
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  bool opened = ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_None);
  if (opened) {
    if (bonobo::uiSelectCullMode("Cull mode", _cullMode)) {
      bonobo::changeCullMode(_cullMode);
    }
    bonobo::uiSelectPolygonMode("Polygon mode", _polygonMode);

    ImGui::Separator();
    ImGui::Text("Sun Control");
    ImGui::Checkbox("Pause Sun", &_isPaused);
    ImGui::Checkbox("Apply Shadow", &_applyShadow);
    ImGui::Checkbox("Apply VolumetricLight", &_isVolumetricLight);

    ImGui::SliderFloat("Speed", &_daySpeed, 0.0f, 2.0f);
    ImGui::SliderFloat("lightX", &lightX, -100.0f, 100.0f);
    ImGui::SliderFloat("lightY", &lightY, -100.0f, 100.0f);
    ImGui::SliderFloat("lightZ", &lightZ, -100.0f, 100.0f);
    ImGui::Checkbox("Enable Wind", &_isWindEnabled);
    ImGui::SliderFloat("Wind Strength", &_windStrength, 0.0f, 2.0f);

    ImGui::Text("Time: %.2f", _elapsedTimeS);
  }

  ImGui::End();

  _windowManager.RenderImGuiFrame(_showGui);
}

void ForestScene::initSkybox() {
  // 1. load Shader
  _programManager.CreateAndRegisterProgram(
      "Skybox",
      {{ShaderType::vertex, "skybox.vert"},
       {ShaderType::fragment, "skybox.frag"}},
      _skyboxShader);

  // 2. create cube
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
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);

  glUseProgram(_skyboxShader);

  // Remove the translation part of the View matrix
  glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
  glm::mat4 viewProj = projection * viewNoTrans;

  glUniformMatrix4fv(
      glGetUniformLocation(_skyboxShader, "vertex_world_to_clip"), 1, GL_FALSE,
      glm::value_ptr(viewProj));

  // Transmit the sun's position
  glUniform3fv(glGetUniformLocation(_skyboxShader, "light_position"), 1,
               glm::value_ptr(_lightPosition));
  glUniform1f(glGetUniformLocation(_skyboxShader, "u_Time"), _elapsedTimeS);
  // Draw
  glBindVertexArray(_skyboxVAO);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);
  glEnable(GL_CULL_FACE);

 
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
}
