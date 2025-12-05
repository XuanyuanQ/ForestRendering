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

  // --- 纹理 ---
  GLuint _texBark;
  GLuint _texLeaf;
  GLuint _texLeafMask;
  GLuint _texFloor;

  // --- 核心数据 ---
  GLuint _instanceVBO; // 实例化矩阵缓冲
  int _treeCount;      // 树的数量
  int _grassCount;

  // 使用 Node 存储树木的各个部分
  std::unordered_map<std::string, Node> _trees;
  std::unordered_map<std::string, Node> _grass;

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

  GLuint _terrainVao;
  GLuint _terrainVbo;
  // glm::vec3 p;
};