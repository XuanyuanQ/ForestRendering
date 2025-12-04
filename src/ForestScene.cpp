#include "ForestScene.hpp"
#include "core/parametric_shapes.hpp"
#include "core/helpers.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <imgui.h>
#include "config.hpp"

ForestScene::ForestScene(WindowManager& windowManager)
	: _windowManager(windowManager),
	  _camera(0.5f * glm::half_pi<float>(),
			  static_cast<float>(config::resolution_x) / static_cast<float>(config::resolution_y),
			  0.01f, 1000.0f)
{
	_camera.mWorld.SetTranslate(glm::vec3(0.0f, 10.0f, 20.0f));
	_camera.mMouseSensitivity = glm::vec2(0.003f);
	_camera.mMovementSpeed = glm::vec3(3.0f);
	
	_treeCount = 25;
	_elapsedTimeS = 0.0f;
	_isLeavesMesh = false;
	_lightPosition = glm::vec3(-2.0f, 4.0f, 2.0f);

	_cullMode = bonobo::cull_mode_t::disabled;
	_polygonMode = bonobo::polygon_mode_t::fill;
	_showGui = true;
	_fallbackShader = 0;
	_instanceVBO = 0;
}

ForestScene::~ForestScene() {
	if (_instanceVBO != 0) glDeleteBuffers(1, &_instanceVBO);
	// Node 的析构函数会自动清理它管理的资源，不需要手动 glDeleteProgram 等
}

std::vector<glm::mat4> ForestScene::generateTreeTransforms(int count) {
	std::vector<glm::mat4> matrices;
	matrices.reserve(count);
	int forestWidth = 100;
	int forestDepth = 100;

	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f);
		float x = ((rand() % 10000) / 10000.0f) * forestWidth - forestWidth / 2.0f;
		float z = ((rand() % 10000) / 10000.0f) * forestDepth - forestDepth / 2.0f;
		float y = -4.0f + ((rand() % 1000) / 1000.0f) * 0.5f;
		model = glm::translate(model, glm::vec3(x, y, z));
		float scale = 0.8f + ((rand() % 500) / 1000.0f);
		model = glm::scale(model, glm::vec3(scale));
		float rotAngle = static_cast<float>(rand() % 360);
		model = glm::rotate(model, glm::radians(rotAngle), glm::vec3(0.0f, 1.0f, 0.0f));
		matrices.push_back(model);
	}
	return matrices;
}

bool ForestScene::setup() {
	// ----------------------------------------------------------------
	// 1. 加载 Shader
	// ----------------------------------------------------------------
	_programManager.CreateAndRegisterProgram("Fallback",
											 { { ShaderType::vertex, "default.vert" },
											   { ShaderType::fragment, "default.frag" } },
											 _fallbackShader);
	
	_programManager.CreateAndRegisterProgram("Wave",
											 { { ShaderType::vertex, "wave.vert" },
											   { ShaderType::fragment, "wave.frag" } },
											 _waveShader);

	if (_fallbackShader == 0 || _waveShader == 0) {
		LogError("Failed to load shaders.");
		return false;
	}

	// ----------------------------------------------------------------
	// 2. 准备 Instancing VBO Setup 函数
	// ----------------------------------------------------------------
	// 先生成矩阵数据
	auto tree_matrices = generateTreeTransforms(_treeCount);
	
	// 把矩阵上传到 VBO
	glGenBuffers(1, &_instanceVBO);
	glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, _treeCount * sizeof(glm::mat4), tree_matrices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0); // 解绑

	// 定义一个 Lambda，用于在 loadObjects 时回调配置 VAO
	auto setupInstanceVBO = [this]() -> GLuint {
		glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);
		
		size_t vec4Size = sizeof(glm::vec4);

		int baseLocation = 7; // <--- 从 7 开始，避开 tangent(3) 和 binormal(4)
			for (int i = 0; i < 4; i++) {
				glEnableVertexAttribArray(baseLocation + i);
				glVertexAttribPointer(baseLocation + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * vec4Size));
				glVertexAttribDivisor(baseLocation + i, 1);
			}
		
		// 返回 _instanceVBO 的 ID，
		return _instanceVBO;
	};

	// ----------------------------------------------------------------
	// 3. 加载模型 (传入 setupInstanceVBO 回调)
	// ----------------------------------------------------------------

	std::vector<bonobo::mesh_data> tree_meshes = bonobo::loadObjects(config::resources_path("47-mapletree/MapleTree.obj"), setupInstanceVBO);
	
	if (tree_meshes.empty()) {
		LogError("Failed to load res/MapleTree.obj");
		return false;
	}

	// ----------------------------------------------------------------
	// 4. 加载纹理
	// ----------------------------------------------------------------
	GLuint maple_bark = bonobo::loadTexture2D(config::resources_path("47-mapletree/maple_bark.png"));
	GLuint maple_leaf = bonobo::loadTexture2D(config::resources_path("47-mapletree/maple_leaf.png"));
	GLuint leaves_alpha = bonobo::loadTexture2D(config::resources_path("47-mapletree/maple_leaf_Mask.jpg"));
	GLuint maple_leaf_normal = bonobo::loadTexture2D(config::resources_path("47-mapletree/maple_leaf_normal.png"));
	GLuint maple_bark_normal = bonobo::loadTexture2D(config::resources_path("47-mapletree/maple_bark_normal.png"));
	// ----------------------------------------------------------------
	// 5. 构建 Node 结构
	// ----------------------------------------------------------------
	
	// 定义 Uniform 设置回调
	auto set_uniforms = [this](GLuint program) {
		glUniform1i(glGetUniformLocation(program, "is_leaves"), _isLeavesMesh ? 1 : 0);
		glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(_lightPosition));
		glUniform3fv(glGetUniformLocation(program, "camera_position"), 1, glm::value_ptr(_camera.mWorld.GetTranslation()));
		glUniform1f(glGetUniformLocation(program, "elapsed_time_s"), _elapsedTimeS);
	};

	// 遍历所有 Mesh，创建 Node
	for (auto &obj : tree_meshes) {
		Node node;
		node.set_geometry(obj);
		
		// 设置 Shader 和 Uniform 回调
		node.set_program(&_fallbackShader, set_uniforms);
		
		// 添加纹理 (Node 会自动绑定到 Texture Unit 0, 1, 2...)
		node.add_texture("maple_bark", maple_bark, GL_TEXTURE_2D);
		node.add_texture("maple_leaf", maple_leaf, GL_TEXTURE_2D);
		node.add_texture("leaves_alpha", leaves_alpha, GL_TEXTURE_2D);
		node.add_texture("maple_leaf_normal", maple_leaf_normal, GL_TEXTURE_2D);
		node.add_texture("maple_bark_normal", maple_bark_normal, GL_TEXTURE_2D);
		_trees.insert({obj.name, node});
	}

	// ----------------------------------------------------------------
	// 6. 配置 Wave 地面 Node
	// ----------------------------------------------------------------
	_waveMesh = parametric_shapes::createQuad(100.0f, 100.0f, 1000, 1000);
	
	auto wave_uniforms = [this](GLuint program) {
		glUniform1i(glGetUniformLocation(program, "use_normal_mapping"), 0);
		glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(_lightPosition));
		glUniform3fv(glGetUniformLocation(program, "camera_position"), 1, glm::value_ptr(_camera.mWorld.GetTranslation()));
		glUniform1f(glGetUniformLocation(program, "elapsed_time_s"), _elapsedTimeS);
		// 材质参数
		glUniform3f(glGetUniformLocation(program, "ambient"), 0.1f, 0.1f, 0.1f);
		glUniform3f(glGetUniformLocation(program, "diffuse"), 0.7f, 0.2f, 0.4f);
		glUniform3f(glGetUniformLocation(program, "specular"), 1.0f, 1.0f, 1.0f);
		glUniform1f(glGetUniformLocation(program, "shininess"), 10.0f);
	};

	_quadNode.set_geometry(_waveMesh);
	_quadNode.set_program(&_waveShader, wave_uniforms);
	
	GLuint floor_tex = bonobo::loadTexture2D(config::resources_path("forested-floor/textures/KiplingerFLOOR.png"));
	_quadNode.add_texture("diffuse_texture", floor_tex, GL_TEXTURE_2D);


	// ----------------------------------------------------------------
	// 7. GL 状态
	// ----------------------------------------------------------------
	glClearDepthf(1.0f);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	bonobo::changeCullMode(_cullMode);

	return true;
}

void ForestScene::update(double deltaTimeUs) {
	_inputHandler.Advance();
	_camera.Update(std::chrono::microseconds((long)deltaTimeUs), _inputHandler);
	_elapsedTimeS += (float)(deltaTimeUs / 1000000.0);

	if (_inputHandler.GetKeycodeState(GLFW_KEY_F2) & JUST_RELEASED)
		_showGui = !_showGui;
}

void ForestScene::render(GLFWwindow* window) {
	int w, h;
	glfwGetFramebufferSize(window, &w, &h);
	glViewport(0, 0, w, h);
	
	_windowManager.NewImGuiFrame();
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	bonobo::changePolygonMode(_polygonMode);

	// 1. 渲染地面
	// Node::render 通常接受 (VP矩阵, Model矩阵)
	_quadNode.render(_camera.GetWorldToClipMatrix(), glm::mat4(1.0f));

	// 2. 渲染树木
	for (auto &t : _trees) {
		// 根据名字判断是否为树叶，并设置状态变量
		// 这个变量会被上面定义的 set_uniforms lambda 捕获并传给 shader
		if (t.first.find("leaves") != std::string::npos) {
			_isLeavesMesh = true;
			glDisable(GL_CULL_FACE); // 树叶双面渲染
		} else {
			_isLeavesMesh = false;
			glEnable(GL_CULL_FACE);
		}

		// 调用 Node 的渲染
		t.second.render(_camera.GetWorldToClipMatrix(), glm::mat4(1.0f), _treeCount);
	}
	
	// 恢复 Cull Mode
	bonobo::changeCullMode(_cullMode);

	// 3. ImGui
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	bool opened = ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_None);
	if (opened) {
		if (bonobo::uiSelectCullMode("Cull mode", _cullMode)) {
			bonobo::changeCullMode(_cullMode);
		}
		bonobo::uiSelectPolygonMode("Polygon mode", _polygonMode);
		ImGui::Text("Time: %.2f", _elapsedTimeS);
	}
	ImGui::End();

	_windowManager.RenderImGuiFrame(_showGui);
}
