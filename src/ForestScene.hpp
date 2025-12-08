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
#define SHADOW_WIDTH 1920
#define SHADOW_HEIGHT 1920
class ForestScene {
public:
  ForestScene(WindowManager &windowManager);
  ~ForestScene();

  bool setup();
  void update(double deltaTimeUs);
  void render(GLFWwindow *window);

  InputHandler &getInputHandler() { return _inputHandler; }
  FPSCameraf &getCamera() { return _camera; }

private:
  // 生成矩阵数据的辅助函数
  std::vector<glm::mat4> generateTreeTransforms(int count, int Width = 100,
                                                int Depth = 100);

  GLuint createQuadsForPatch();
  void rendtest(GLuint shaderProgram);
  void simulationSun(GLuint shaderProgram);
  void simulationForest(GLuint shaderProgram);
  void initShadowMap();
  void initGbuffer();
  void initLightContribution();
  void updateLightMatrix(const glm::vec3 light_pos);
  void renderShadowMap();
  void renderGbuffer();
  void renderLightContribution();
  void renderFinalResult();
  void renderAllobjects(GLuint shaderProgram);

  void initSkybox(); // 初始化函数
  void renderSkybox(glm::mat4 const &view,
                    glm::mat4 const &projection); // 渲染函数

private:
  WindowManager &_windowManager;
  InputHandler _inputHandler;
  FPSCameraf _camera;
  ShaderProgramManager _programManager;

  // --- 资源 ID ---
  GLuint _fallbackShader; // 树木用的 Shader
  GLuint _grassShader;
  GLuint _waveShader; // 地面用的 Wave Shader
  GLuint _tessHeightMapShader;
  GLuint _shadowMapShader;
  GLuint _gBufferShader;
  GLuint _lightContributionShader;
  GLuint _resolve_deferred_shader;

  GLuint _sunTestShader;
  GLuint _forestTestShader;

  // --- 纹理 ---
  GLuint _texBark;
  GLuint _texLeaf;
  GLuint _normalBark;
  GLuint _normalLeaf;
  GLuint _texLeafMask;
  GLuint _texFloor;
  GLuint _texGrassMask;
  GLuint _texGrass;

  // --- 核心数据 ---
  GLuint _instanceVBO; // 实例化矩阵缓冲
  int _treeCount;      // 树的数量
  int _grassCount;

  // 使用 Node 存储树木的各个部分
  std::unordered_map<std::string, Node> _trees;
  std::unordered_map<std::string, Node> _grass;
  std::unordered_map<std::string, bonobo::mesh_data> _tree_meshes;
  std::unordered_map<std::string, bonobo::mesh_data> _grass_meshes;

  // 地面 Node 和 Mesh
  Node _quadNode;
  bonobo::mesh_data _waveMesh; //

  // --- 状态变量 ---
  float _elapsedTimeS;
  bool _isLeavesMesh;
  glm::vec3 _lightPosition;

  // UI 控制
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

  bool _isPaused;  // 暂停开关
  float _sunTime;  // 太阳的专属时间
  float _daySpeed; // 太阳移动速度
  bool _applyShadow;
  float lightX{3.0f};
  float lightY{14.0f};
  float lightZ{11.0f};

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
};
