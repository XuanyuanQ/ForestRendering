#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <random> // 用于随机生成

#include "config.hpp"
#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/InputHandler.h"
#include "core/WindowManager.hpp"
#include "core/parametric_shapes.hpp"
#include "core/ShaderProgramManager.hpp"
#include "core/helpers.hpp"

using namespace std::literals::chrono_literals;

// ---------------------------------------------------------------------
// 辅助函数：生成 N 个随机变换矩阵 (位置、旋转、缩放)
// ---------------------------------------------------------------------
std::vector<glm::mat4> generateTreeTransforms(int count, float area_size) {
	std::vector<glm::mat4> matrices;
	matrices.reserve(count);

	std::random_device rd;
	std::mt19937 gen(rd());
	// 分布范围：-50 到 50
	std::uniform_real_distribution<float> dis_pos(-area_size / 2.0f, area_size / 2.0f);
	// 缩放范围：GLB模型可能很大，这里给个合适的缩放
	std::uniform_real_distribution<float> dis_scale(0.05f, 0.12f);
	// 旋转范围：0 到 360 度
	std::uniform_real_distribution<float> dis_rot(0.0f, 360.0f);

	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f);
		
		// 1. 平移 (X, Z随机，Y=0贴地)
		float x = dis_pos(gen);
		float z = dis_pos(gen);
		model = glm::translate(model, glm::vec3(x, 0.0f, z));

		// 2. 旋转 (绕Y轴)
		float angle = dis_rot(gen);
		model = glm::rotate(model, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

		model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		// 3. 缩放
		float scale = dis_scale(gen);
		model = glm::scale(model, glm::vec3(scale));

		matrices.push_back(model);
	}
	return matrices;
}

// ---------------------------------------------------------------------
// 主程序
// ---------------------------------------------------------------------
int main() {
	// 1. 设置框架
	Bonobo framework;

	// 2. 设置摄像机
	InputHandler input_handler;
	FPSCameraf camera(0.5f * glm::half_pi<float>(),
					  static_cast<float>(config::resolution_x) / static_cast<float>(config::resolution_y),
					  0.01f, 1000.0f);
	
	// 调整相机位置：看整个森林
	camera.mWorld.SetTranslate(glm::vec3(0.0f, 5.0f, 20.0f));
	camera.mWorld.LookAt(glm::vec3(0.0f, 2.0f, 0.0f));
	camera.mMouseSensitivity = glm::vec2(0.003f);
	camera.mMovementSpeed = glm::vec3(3.0f);

	// 3. 创建窗口
	WindowManager& window_manager = framework.GetWindowManager();
	WindowManager::WindowDatum window_datum{ input_handler, camera, config::resolution_x, config::resolution_y, 0, 0, 0, 0};
	
	GLFWwindow* window = window_manager.CreateGLFWWindow("Forest Scene", window_datum, config::msaa_rate);
	if (window == nullptr) {
		LogError("Failed to get a window: exiting.");
		return EXIT_FAILURE;
	}

	bonobo::init();

	// ----------------------------------------------------------------
	// 4. 加载资源
	// ----------------------------------------------------------------

	// A. 加载专门用于实例化的 Shader
	// 注意：这里必须用 tree.vert，不能用 fallback，因为 fallback 没法接收 instance_matrix
	GLuint tree_shader = bonobo::createProgram("tree.vert", "tree.frag");
	if (tree_shader == 0u) {
		LogError("Failed to load tree shader");
		return EXIT_FAILURE;
	}

	// B. 加载 GLB 树木模型
	std::vector<bonobo::mesh_data> tree_meshes = bonobo::loadObjects(config::resources_path("scenes/maple_tree.glb"));
	if (tree_meshes.empty()) {
		LogError("Failed to load tree model: res/maple_tree.glb");
		return EXIT_FAILURE;
	}

	// C. 生成实例化数据 (100 棵树)
	int tree_count = 100;
	auto tree_matrices = generateTreeTransforms(tree_count, 100.0f);

	// D. 创建 Instance VBO 并上传矩阵数据
	GLuint instanceVBO;
	glGenBuffers(1, &instanceVBO);
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, tree_count * sizeof(glm::mat4), tree_matrices.data(), GL_STATIC_DRAW);

	// E. 配置 VAO：将 Instance VBO 挂载到树木的每一个 mesh 上
	// 一个 GLB 可能包含多个 mesh (比如树干和叶子是分开的)
	for (auto& mesh : tree_meshes) {
		if (mesh.vao == 0) continue; // 安全检查

		glBindVertexArray(mesh.vao);
		glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

		// 矩阵 mat4 占用 4 个 vec4 的位置
		// 我们假设 shader 里 layout (location = 3) 开始
		size_t vec4Size = sizeof(glm::vec4);
		for (int i = 0; i < 4; i++) {
			glEnableVertexAttribArray(3 + i);
			glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * vec4Size));
			// 关键：告诉 OpenGL 这个属性每画一个实例更新一次
			glVertexAttribDivisor(3 + i, 1);
		}
		glBindVertexArray(0);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0); // 解绑

	// F. 创建地面 (用于对比)
	auto ground_mesh = parametric_shapes::createQuad(100.0f, 100.0f, 20, 20);
	GLuint ground_vao = ground_mesh.vao;

	// 5. 设置 OpenGL 状态
	glClearDepthf(1.0f);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	auto last_time = std::chrono::high_resolution_clock::now();

	// ----------------------------------------------------------------
	// 6. 主循环
	// ----------------------------------------------------------------
	while (!glfwWindowShouldClose(window)) {
		auto const now_time = std::chrono::high_resolution_clock::now();
		auto const delta_time_us = std::chrono::duration_cast<std::chrono::microseconds>(now_time - last_time);
		last_time = now_time;

		glfwPollEvents();
		input_handler.Advance();
		camera.Update(delta_time_us, input_handler);

		if (input_handler.GetKeycodeState(GLFW_KEY_ESCAPE) & JUST_PRESSED) {
			glfwSetWindowShouldClose(window, GL_TRUE);
		}

		int framebuffer_width, framebuffer_height;
		glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
		glViewport(0, 0, framebuffer_width, framebuffer_height);
		window_manager.NewImGuiFrame();
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		// --- 渲染开始 ---
	
		// 1. 渲染森林 (使用 tree_shader)
		glUseProgram(tree_shader);
		
		glm::mat4 vp = camera.GetWorldToClipMatrix();
		glUniformMatrix4fv(glGetUniformLocation(tree_shader, "view_projection"), 1, GL_FALSE, glm::value_ptr(vp));

		// 遍历树的所有部分并进行实例化渲染
		for (auto& mesh : tree_meshes) {
			if (mesh.vao == 0) continue;
			
			glBindVertexArray(mesh.vao);
			// 这里的 tree_count 就是我们要画的树的数量
			glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)mesh.indices_nb, GL_UNSIGNED_INT, 0, tree_count);
		}
		glBindVertexArray(0);

		// 2. 渲染地面 (可以用同一个 shader，只是矩阵不一样)
		// 为了简单，我们手动把地面的 instance_matrix 设为单位矩阵
		// 但由于我们上面把 attributes 绑定到了 VBO，这里复用 Shader 稍微麻烦
		// 这里简单起见，我们假设地面在原点，如果你想用 tree_shader 画地面，需要给地面也搞个 instance buffer
		// 或者简单点，我们只画树，地面暂时不画或者用 fallback 画 (防止 shader 冲突)
		
		// 如果想画地面，最好换回 fallback shader 或者单独处理
		// 这里演示只画森林，避免代码太复杂出错
		// (地面代码暂时注释掉，先把树画出来)
		/*
		glUseProgram(fallback_shader);
		glUniformMatrix4fv(glGetUniformLocation(fallback_shader, "vertex_transform"), 1, GL_FALSE, glm::value_ptr(vp));
		glBindVertexArray(ground_vao);
		glDrawElements(GL_TRIANGLES, (GLsizei)ground_mesh.indices_nb, GL_UNSIGNED_INT, 0);
		*/

		// --- 渲染结束 ---

		window_manager.RenderImGuiFrame(true);
		glfwSwapBuffers(window);
	}

	glDeleteVertexArrays(1, &ground_vao);
	bonobo::deinit();

	return EXIT_SUCCESS;
}
