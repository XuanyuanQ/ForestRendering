#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/InputHandler.h"
#include "core/ShaderProgramManager.hpp"
#include "core/WindowManager.hpp"
#include "core/node.hpp"
#define SHADOW_WIDTH 2048
#define SHADOW_HEIGHT 2048

#define CSM 0
#define SSAO 1

struct InstanceData {
  glm::mat4 modelMatrix; // 64 bytes
  float windSpeed;       // 4 bytes
};
struct CSMData {
  glm::mat4 lightSpaceMatrices[16];

  glm::vec4 cascadePlaneDistances[16];

  glm::vec4 lightDir; // no using

  int cascadeCount;
  int padding[3];
};

struct SSAOData {
  glm::vec4 ssaoKernel[64];
  int kernelSize;
  int _pad1;
  int _pad2;
  int _pad3;
};
class ForestScene {
public:
  ForestScene(WindowManager &windowManager);
  ~ForestScene();

  bool setup(GLFWwindow *window);
  void update(double deltaTimeUs);
  void render(GLFWwindow *window);

  InputHandler &getInputHandler() { return _inputHandler; }
  FPSCameraf &getCamera() { return _camera; }

private:
  std::vector<InstanceData> generateTreeTransforms(int count, int Width = 100,
                                                   int Depth = 100);

  GLuint createQuadsForPatch();
  void initShadowMap();
  void initShadowCSM();
  void initGbuffer();
  void initSSAO();
  void updateLightMatrix(const glm::vec3 light_pos);
  void renderShadowMap(GLuint FBO);
  void renderGbuffer();
  void renderAllobjects(GLuint shaderProgram);
  void renderPartical(GLuint shaderProgram);
  void renderSSAO(GLuint shaderProgram);

  std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4 &proj,
                                                     const glm::mat4 &view);
  glm::mat4 getLightSpaceMatrix(const float nearPlane, const float farPlane);
  std::vector<glm::mat4> getLightSpaceMatrices();

  void initSkybox();
  void renderSkybox(glm::mat4 const &view, glm::mat4 const &projection);
  void generateSSAOKernel();
  void generateNoiseTexture();

private:
  std::vector<float> shadowCascadeLevels{25.0f, 80.0f, 200.0f, 500.0f, 1000.0f};

  GLuint uboShadows;
  CSMData csmData;
  WindowManager &_windowManager;
  InputHandler _inputHandler;
  FPSCameraf _camera;
  ShaderProgramManager _programManager;

  // --- resource ID ---
  GLuint _fallbackShader;
  GLuint _grassShader;
  GLuint _particelShader;
  GLuint _SSAOShader;
  GLuint _SSAOBlurShader;

  GLuint _waveShader;

  GLuint _tessHeightMapShader;
  GLuint _shadowMapShader;
  GLuint _gBufferShader;
  GLuint _lightContributionShader;
  GLuint _resolve_deferred_shader;

  GLuint _sunTestShader;
  GLuint _forestTestShader;

  // --- Texture ---
  GLuint _texBark;
  GLuint _texLeaf;
  GLuint _normalBark;
  GLuint _normalLeaf;
  GLuint _texLeafMask;
  GLuint _texFloor;
  GLuint _texGrassMask;
  GLuint _texGrass;

  GLuint _instanceVBO;
  GLuint _particelVBO;
  int _treeCount;
  int _grassCount;

  std::unordered_map<std::string, Node> _trees;
  std::unordered_map<std::string, Node> _grass;
  std::unordered_map<std::string, bonobo::mesh_data> _tree_meshes;
  std::unordered_map<std::string, bonobo::mesh_data> _grass_meshes;

  Node _quadNode;
  bonobo::mesh_data _waveMesh; //
  bonobo::mesh_data _particelMesh;
  bonobo::mesh_data _frogMesh;
  // Node _particelNode;

  float _elapsedTimeS;
  bool _isLeavesMesh;
  glm::vec3 _lightPosition;

  // UI
  bonobo::cull_mode_t _cullMode;
  bonobo::polygon_mode_t _polygonMode;
  bool _showGui;
  const unsigned int _NUM_PATCH_PTS = 4;
  glm::mat4 _terrain_world = glm::mat4(1.0f);
  std::function<void(GLuint)> _terrain_Uniforms;
  GLuint _grass_tex;
  GLuint _floor_tex;

  GLuint _skyboxVAO;
  GLuint _skyboxVBO;
  GLuint _skyboxShader;

  bool _isPaused;
  float _sunTime;
  float _daySpeed;
  bool _applyShadow;
  bool _isVolumetricLight;
  bool _applySSAO;
  float lightX{-52.8f};
  float lightY{50.4f};
  float lightZ{100.0f};

  // shadow
  GLuint shadowFBO;
  GLuint shadowMap;
  GLuint shadowArray;
  glm::mat4 light_world_to_clip_matrix = glm::mat4(1.0f);

  // gbuffer
  GLuint gbufferFBO;
  unsigned int gPosition, gNormal, gAlbedo, gDepth;

  // light contribution
  bonobo::mesh_data lightMesh;
  Node lightgeometry;
  GLuint lightFBO;
  GLuint lDiffuse;
  GLuint lAmbient;
  GLuint lSpecular;
  GLuint lboDepth;

  GLuint fullScreenVAO;

  // test
  unsigned int cubeVAO = 0;
  unsigned int cubeVBO = 0;
  GLuint _terrainVao;
  GLuint _terrainVbo;

  // control wind
  bool _isWindEnabled;
  float _windStrength;

  int _particel_count;
  int gbufffer_w, gbufffer_h;

  // SSAO
  std::vector<glm::vec3> ssaoKernel;
  GLuint noiseTexture;

  GLuint ssaoFBO;
  GLuint ssaoTexture;
  GLuint ssaoVAO;
  GLuint uboSSAO;
  SSAOData ssaoData;

  unsigned int ssaoBlurFBO;
  GLuint ssaoColorBufferBlur;
};
