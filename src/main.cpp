#include <cstdlib>
#include <chrono>

#include "config.hpp"
#include "core/Bonobo.h"
#include "core/WindowManager.hpp"
#include "ForestScene.hpp" // 引入我们写的类
#include "core/helpers.hpp"


using namespace std::literals::chrono_literals;

int main() {
	// 1. 框架基础
	Bonobo framework;
	WindowManager& window_manager = framework.GetWindowManager();

	// 2. 实例化场景 (这里会初始化相机和输入)
	ForestScene scene(window_manager);

	// 3. 创建窗口
	// 注意：我们要从 scene 对象里取出 InputHandler 和 Camera 传给窗口
	WindowManager::WindowDatum window_datum{
		scene.getInputHandler(),
		scene.getCamera(),
		config::resolution_x, config::resolution_y, 0, 0, 0, 0
	};
	
	GLFWwindow* window = window_manager.CreateGLFWWindow("Forest Scene", window_datum, config::msaa_rate);
	if (window == nullptr) {
		LogError("Failed to get a window: exiting.");
		return EXIT_FAILURE;
	}

	// 4. 初始化 OpenGL Loader
	bonobo::init();

	// 5. 场景 Setup (加载模型等)
	if (!scene.setup()) {
		return EXIT_FAILURE;
	}

	// 6. 主循环
	auto last_time = std::chrono::high_resolution_clock::now();

	while (!glfwWindowShouldClose(window)) {
		auto const now_time = std::chrono::high_resolution_clock::now();
		auto const delta_time_us = std::chrono::duration_cast<std::chrono::microseconds>(now_time - last_time);
		last_time = now_time;

		glfwPollEvents();
		
		// 处理 ESC 退出
		if (scene.getInputHandler().GetKeycodeState(GLFW_KEY_ESCAPE) & JUST_PRESSED) {
			glfwSetWindowShouldClose(window, GL_TRUE);
		}

		// 调用类的 update 和 render
		scene.update(delta_time_us.count());
		scene.render(window);
		
		glfwSwapBuffers(window);
	}

	bonobo::deinit();
	return EXIT_SUCCESS;
}
