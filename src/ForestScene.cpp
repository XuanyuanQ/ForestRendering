#include "ForestScene.hpp"
#include "core/parametric_shapes.hpp"
#include "core/ShaderProgramManager.hpp"
#include "core/helpers.hpp" // 包含 loadObjects
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include "config.hpp" // 包含分辨率配置

ForestScene::ForestScene(WindowManager& windowManager)
	: _windowManager(windowManager),
	  // 初始化相机参数
	  _camera(0.5f * glm::half_pi<float>(),
			  static_cast<float>(config::resolution_x) / static_cast<float>(config::resolution_y),
			  0.01f, 1000.0f)
{
	// 设置相机初始位置
	_camera.mWorld.SetTranslate(glm::vec3(0.0f, 5.0f, 20.0f));
	_camera.mWorld.LookAt(glm::vec3(0.0f, 2.0f, 0.0f));
	_camera.mMouseSensitivity = glm::vec2(0.003f);
	_camera.mMovementSpeed = glm::vec3(3.0f);
	
	_treeCount = 500; // 设置树的数量
	_treeShader = 0;
	_instanceVBO = 0;
}

ForestScene::~ForestScene() {
	// 清理资源
	if (_instanceVBO != 0) glDeleteBuffers(1, &_instanceVBO);
	if (_groundMesh.vao != 0) glDeleteVertexArrays(1, &_groundMesh.vao);
	// Shader 和 Texture 通常由管理器清理，或者是退出时自动清理
}

std::vector<glm::mat4> ForestScene::generateTreeTransforms(int count, float area_size) {
	std::vector<glm::mat4> matrices;
	matrices.reserve(count);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dis_pos(-area_size / 2.0f, area_size / 2.0f);
	std::uniform_real_distribution<float> dis_scale(0.05f, 0.12f);
	std::uniform_real_distribution<float> dis_rot(0.0f, 360.0f);

	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f);
		float x = dis_pos(gen);
		float z = dis_pos(gen);
		model = glm::translate(model, glm::vec3(x, 0.0f, z));
		float angle = dis_rot(gen);
		model = glm::rotate(model, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
		// 修正树木倒伏
		model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		float scale = dis_scale(gen);
		model = glm::scale(model, glm::vec3(scale));
		matrices.push_back(model);
	}
	return matrices;
}

bool ForestScene::setup() {
	// 1. 加载 Shader
	_treeShader = bonobo::createProgram("tree.vert", "tree.frag");
	if (_treeShader == 0u) {
		LogError("Failed to load tree shader");
		return false;
	}

	// 2. 加载树木模型
	_treeMeshes = bonobo::loadObjects(config::resources_path("scenes/maple_tree.glb"));
	if (_treeMeshes.empty()) {
		LogError("Failed to load tree model");
		return false;
	}

	// 3. 生成并绑定实例化矩阵
	auto tree_matrices = generateTreeTransforms(_treeCount, 100.0f);

	glGenBuffers(1, &_instanceVBO);
	glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, _treeCount * sizeof(glm::mat4), tree_matrices.data(), GL_STATIC_DRAW);

	// 4. 配置 VAO ( Instancing )
	for (auto& mesh : _treeMeshes) {
		if (mesh.vao == 0) continue;

		glBindVertexArray(mesh.vao);
		glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);

		size_t vec4Size = sizeof(glm::vec4);
		for (int i = 0; i < 4; i++) {
			glEnableVertexAttribArray(3 + i);
			glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * vec4Size));
			glVertexAttribDivisor(3 + i, 1);
		}
		glBindVertexArray(0);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// 5. 创建地面
	_groundMesh = parametric_shapes::createQuad(100.0f, 100.0f, 20, 20);

	// 6. 全局 OpenGL 设置
	glClearDepthf(1.0f);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	return true;
}

void ForestScene::update(double deltaTimeUs) {
	_inputHandler.Advance();
	_camera.Update(std::chrono::microseconds((long)deltaTimeUs), _inputHandler);
}

void ForestScene::render(GLFWwindow* window) {
	int framebuffer_width, framebuffer_height;
	glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
	glViewport(0, 0, framebuffer_width, framebuffer_height);
	
	_windowManager.NewImGuiFrame();
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	// --- 渲染树木 ---
	glUseProgram(_treeShader);
//	// 关闭面剔除以显示双面叶子
//	glDisable(GL_CULL_FACE);

	glm::mat4 vp = _camera.GetWorldToClipMatrix();
	glUniformMatrix4fv(glGetUniformLocation(_treeShader, "view_projection"), 1, GL_FALSE, glm::value_ptr(vp));

	for (auto& mesh : _treeMeshes) {
		if (mesh.vao == 0) continue;
		glBindVertexArray(mesh.vao);
		glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)mesh.indices_nb, GL_UNSIGNED_INT, 0, _treeCount);
	}
	glBindVertexArray(0);
	glEnable(GL_CULL_FACE); // 恢复剔除

	// --- 渲染 ImGui ---
	_windowManager.RenderImGuiFrame(true);
}
