#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/InputHandler.h"
#include "core/WindowManager.hpp"
#include "core/ShaderProgramManager.hpp"
#include "core/node.hpp"

class ForestScene {
public:
	ForestScene(WindowManager& windowManager);
	~ForestScene();

	bool setup();
	void update(double deltaTimeUs);
	void render(GLFWwindow* window);

	InputHandler& getInputHandler() { return _inputHandler; }
	FPSCameraf& getCamera() { return _camera; }

private:
	// 生成矩阵数据的辅助函数
	std::vector<glm::mat4> generateTreeTransforms(int count);

	WindowManager& _windowManager;
	InputHandler _inputHandler;
	FPSCameraf   _camera;
	ShaderProgramManager _programManager;

	// --- 资源 ID ---
	GLuint _fallbackShader; // 树木用的 Shader
	GLuint _waveShader;     // 地面用的 Wave Shader
	
	// --- 纹理 ---
	GLuint _texBark;
	GLuint _texLeaf;
	GLuint _texLeafMask;
	GLuint _texFloor;

	// --- 核心数据 ---
	GLuint _instanceVBO; // 实例化矩阵缓冲
	int _treeCount;      // 树的数量

	// 使用 Node 存储树木的各个部分
	std::unordered_map<std::string, Node> _trees;

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
};
