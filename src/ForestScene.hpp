
#pragma once // 防止重复引用

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "core/FPSCamera.h"
#include "core/InputHandler.h"
#include "core/Bonobo.h"
#include "core/WindowManager.hpp"
#include "core/helpers.hpp" 

class ForestScene {
public:
	// 构造函数：初始化相机和输入
	ForestScene(WindowManager& windowManager);
	
	// 析构函数：清理资源
	~ForestScene();

	// 初始化：加载模型、Shader、生成矩阵
	bool setup();

	// 更新：处理输入、移动相机
	void update(double deltaTimeUs);

	// 渲染：画所有的东西
	void render(GLFWwindow* window);

	// 获取相关对象（供 main 创建窗口用）
	InputHandler& getInputHandler() { return _inputHandler; }
	FPSCameraf& getCamera() { return _camera; }

private:
	// 内部辅助函数：生成随机树木矩阵
	std::vector<glm::mat4> generateTreeTransforms(int count, float area_size);

	// --- 成员变量 ---
	InputHandler _inputHandler;
	FPSCameraf   _camera;
	WindowManager& _windowManager;

	// 资源 ID
	GLuint _treeShader;
	GLuint _instanceVBO;
	
	// 树木数据
	std::vector<bonobo::mesh_data> _treeMeshes;
	int _treeCount;

	// 地面数据
	bonobo::mesh_data _groundMesh;
};
