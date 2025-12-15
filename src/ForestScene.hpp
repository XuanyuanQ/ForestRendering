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

struct InstanceData {
  glm::mat4 modelMatrix; // 64 bytes
  float windSpeed;       // 4 bytes
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
  void rendtest(GLuint shaderProgram);
  void simulationSun(GLuint shaderProgram);
  void simulationForest(GLuint shaderProgram);
  void initShadowMap();
  void initGbuffer();
  void initLightContribution();
  void updateLightMatrix(const glm::vec3 light_pos);
  void renderShadowMap(GLuint FBO);
  void renderGbuffer();
  void renderLightContribution();
  void renderFinalResult();
  void renderAllobjects(GLuint shaderProgram);
  void renderPartical(GLuint shaderProgram);
  void renderFrog(GLuint shaderProgram);

  void initSkybox();
  void renderSkybox(glm::mat4 const &view,
                    glm::mat4 const &projection);

private:
  WindowManager &_windowManager;
  InputHandler _inputHandler;
  FPSCameraf _camera;
  ShaderProgramManager _programManager;

  // --- resource ID ---
  GLuint _fallbackShader;
  GLuint _grassShader;
  GLuint _particelShader;
  GLuint _volumetricLightShader;

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
  float lightX{-52.8f};
  float lightY{50.4f};
  float lightZ{100.0f};

  // shadow
  GLuint shadowFBO;
  GLuint shadowMap;
  glm::mat4 light_world_to_clip_matrix = glm::mat4(1.0f);

  // gbuffer
  GLuint gbufferFBO;
  GLuint gDiffuse;
  GLuint gSpecular;
  GLuint gNormal;
  GLuint gDepth;
  GLuint gObjectType;

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
};
