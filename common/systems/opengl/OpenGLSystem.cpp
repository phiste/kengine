#include <memory>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "opengl/Program.hpp"

#include "imgui.h"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl3.h"

#include "EntityManager.hpp"

#include "components/ModelLoaderComponent.hpp"
#include "components/TextureLoaderComponent.hpp"

#include "components/ModelComponent.hpp"
#include "components/OpenGLModelComponent.hpp"
#include "components/ImGuiComponent.hpp"
#include "components/AdjustableComponent.hpp"
#include "components/InputComponent.hpp"
#include "components/CameraComponent.hpp"
#include "components/ViewportComponent.hpp"
#include "components/ShaderComponent.hpp"

#include "packets/ImGuiScale.hpp"

#include "common/systems/opengl/ShaderHelper.hpp"

#include "helpers/CameraHelper.hpp"

#include "OpenGLSystem.hpp"
#include "Controllers.hpp"

#include "ShadowMap.hpp"
#include "ShadowCube.hpp"
#include "SpotLight.hpp"
#include "DirLight.hpp"
#include "PointLight.hpp"
#include "LightSphere.hpp"
#include "GodRaysDirLight.hpp"
#include "GodRaysPointLight.hpp"
#include "GodRaysSpotLight.hpp"
#include "Highlight.hpp"
#include "Debug.hpp"
#include "SkyBox.hpp"
#include "Text.hpp"

namespace kengine {
	static bool g_init = false;

	static size_t g_gBufferTextureCount = 0;
	putils::gl::Program::Parameters g_params;
	putils::Point2i g_screenSize = { 1280, 720 };
	static float g_dpiScale = 1.f;

	static GLFWwindow * g_window = nullptr;

	namespace Input {
		struct KeyInfo {
			int key;
			bool pressed;
		};

		static putils::vector<KeyInfo, 128> keys;

		static putils::Point2f lastPos{ FLT_MAX, FLT_MAX };
		struct ClickInfo {
			putils::Point2f pos;
			int button;
			bool pressed;
		};
		static putils::vector<ClickInfo, 128> clicks;
		struct MoveInfo {
			putils::Point2f pos;
			putils::Point2f rel;
		};
		static putils::vector<MoveInfo, 128> positions;
		static putils::vector<float, 128> scrolls;

		static void click(GLFWwindow * g_window, int button, int action, int mods) {
			if (clicks.full())
				return;
			if (action == GLFW_PRESS)
				clicks.push_back(ClickInfo{ lastPos, button, true });
			else if (action == GLFW_RELEASE)
				clicks.push_back(ClickInfo{ lastPos, button, false });
		}

		static void move(GLFWwindow * g_window, double xpos, double ypos) {
			if (positions.full())
				return;

			if (lastPos.x == FLT_MAX) {
				lastPos.x = (float)xpos;
				lastPos.y = (float)ypos;
			}

			MoveInfo info;
			info.pos = { (float)xpos, (float)ypos };
			info.rel = { (float)xpos - lastPos.x, (float)ypos - lastPos.y };
			lastPos.x = (float)xpos;
			lastPos.y = (float)ypos;
			positions.push_back(info);
		}

		static void scroll(GLFWwindow * g_window, double xoffset, double yoffset) {
			if (scrolls.full())
				return;
			scrolls.push_back((float)yoffset);
		}

		static void key(GLFWwindow * g_window, int key, int scancode, int action, int mods) {
			if (keys.full())
				return;
			if (action == GLFW_PRESS)
				keys.push_back(KeyInfo{ key, true });
			else if (action == GLFW_RELEASE)
				keys.push_back(KeyInfo{ key, false });
		}
	}

	OpenGLSystem::OpenGLSystem(EntityManager & em)
		: System(em),
		_em(em)
	{
	}

	void OpenGLSystem::addShaders() noexcept {
		{ // GBuffer
			_em += [=](Entity & e) { e += makeGBufferShaderComponent<Shaders::Debug>(_em); };
			_em += [=](Entity & e) { e += makeGBufferShaderComponent<Shaders::Text>(_em); };
		}

		{ // Lighting
			_em += [&](Entity & e) {
				e += makeLightingShaderComponent<Shaders::ShadowMap>(_em);
				e += ShadowMapShaderComponent{};
			};

			_em += [&](Entity & e) {
				e += makeLightingShaderComponent<Shaders::ShadowCube>(_em);
				e += ShadowCubeShaderComponent{};
			};

			_em += [=](Entity & e) { e += makeLightingShaderComponent<Shaders::SpotLight>(_em); };
			_em += [=](Entity & e) { e += makeLightingShaderComponent<Shaders::DirLight>(_em); };
			_em += [=](Entity & e) { e += makeLightingShaderComponent<Shaders::PointLight>(_em); };
		}

		{ // Post lighting
			_em += [=](Entity & e) { e += makePostLightingShaderComponent<Shaders::GodRaysDirLight>(_em); };
			_em += [=](Entity & e) { e += makePostLightingShaderComponent<Shaders::GodRaysPointLight>(_em); };
			_em += [=](Entity & e) { e += makePostLightingShaderComponent<Shaders::GodRaysSpotLight>(_em); };
		}

		{ // Post process
			_em += [=](Entity & e) { e += makePostProcessShaderComponent<Shaders::LightSphere>(_em); };
			_em += [=](Entity & e) { e += makePostProcessShaderComponent<Shaders::Highlight>(_em); };
			_em += [=](Entity & e) { e += makePostProcessShaderComponent<Shaders::SkyBox>(_em); };
		}
	}

	void OpenGLSystem::init() noexcept {
		g_init = true;

		g_params.viewPort.size = g_screenSize;
		g_params.nearPlane = 1.f;
		g_params.farPlane = 1000.f;

		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

#ifndef KENGINE_NDEBUG
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		g_window = glfwCreateWindow((int)g_screenSize.x, (int)g_screenSize.y, "Kengine", nullptr, nullptr);
		glfwMakeContextCurrent(g_window);
		glfwSetWindowAspectRatio(g_window, (int)g_screenSize.x, (int)g_screenSize.y);
		glfwSetWindowSizeCallback(g_window, [](auto window, int width, int height) {
			g_screenSize.x = width;
			g_screenSize.y = height;
			glViewport(0, 0, width, height);
		});

		glfwSetMouseButtonCallback(g_window, Input::click);
		glfwSetCursorPosCallback(g_window, Input::move);
		glfwSetScrollCallback(g_window, Input::scroll);
		glfwSetKeyCallback(g_window, Input::key);

#ifndef KENGINE_NDEBUG
		ImGui::CreateContext();
		auto & io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigViewportsNoTaskBarIcon = true;

		{ // Stolen from ImGui_ImplOpenGL3_CreateFontsTexture
			ImFontConfig config;
			config.SizePixels = 13.f * g_dpiScale;
			io.Fonts->AddFontDefault(&config);
			unsigned char * pixels;
			int width, height;
			io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

			GLint last_texture;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
			glBindTexture(GL_TEXTURE_2D, (GLuint)io.Fonts->TexID);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			ImGui::GetStyle().ScaleAllSizes(g_dpiScale);
			glBindTexture(GL_TEXTURE_2D, last_texture);
		}

		ImGui_ImplGlfw_InitForOpenGL(g_window, true);
		ImGui_ImplOpenGL3_Init();
#endif

		glewExperimental = true;
		const bool ret = glewInit();
		assert(ret == GLEW_OK);

#ifndef KENGINE_NDEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * message, const void * userParam) {
			if (severity != GL_DEBUG_SEVERITY_NOTIFICATION)
				std::cerr <<
				putils::termcolor::red <<
				"G: severity = 0x" << std::ios::hex << severity << std::ios::dec <<
				", message: " << message << '\n' <<
				putils::termcolor::reset;
		}, nullptr);
#endif
	}
	void OpenGLSystem::handle(packets::RegisterEntity p) {
		if (_gBufferIterator.func == nullptr)
			return;

		if (p.e.has<GBufferShaderComponent>())
			initShader(*p.e.get<GBufferShaderComponent>().shader);

		if (p.e.has<LightingShaderComponent>())
			initShader(*p.e.get<LightingShaderComponent>().shader);

		if (p.e.has<PostLightingShaderComponent>())
			initShader(*p.e.get<PostLightingShaderComponent>().shader);

		if (p.e.has<PostProcessShaderComponent>())
			initShader(*p.e.get<PostProcessShaderComponent>().shader);
	}

	void OpenGLSystem::handle(packets::DefineGBufferSize p) {
		g_gBufferTextureCount = p.nbAttributes;

		for (const auto & [e, gbuffer] : _em.getEntities<GBufferComponent>())
			gbuffer.init((size_t)g_screenSize.x, (size_t)g_screenSize.y, g_gBufferTextureCount);

		for (const auto & [e, shader] : _em.getEntities<GBufferShaderComponent>())
			initShader(*shader.shader);
		for (const auto & [e, shader] : _em.getEntities<LightingShaderComponent>())
			initShader(*shader.shader);
		for (const auto & [e, shader] : _em.getEntities<PostLightingShaderComponent>())
			initShader(*shader.shader);
		for (const auto & [e, shader] : _em.getEntities<PostProcessShaderComponent>())
			initShader(*shader.shader);

		for (const auto & [e, modelInfo] : _em.getEntities<OpenGLModelComponent>())
			for (const auto & meshInfo : modelInfo.meshes) {
				glBindBuffer(GL_ARRAY_BUFFER, meshInfo.vertexBuffer);
				modelInfo.vertexRegisterFunc();
			}
	}

	void OpenGLSystem::handle(packets::GBufferTexturesIterator p) {
		_gBufferIterator = p;
	}

	void OpenGLSystem::handle(packets::CaptureMouse p) {
		if (glfwGetInputMode(g_window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
			glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		}
		else {
			glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
		}
	}

	void OpenGLSystem::initShader(putils::gl::Program & p) {
		p.init(g_gBufferTextureCount);

		assert(_gBufferIterator.func != nullptr);
		int texture = 0;
		_gBufferIterator.func([&](const char * name) {
			p.addGBufferTexture(name, texture++);
		});
	}


	OpenGLSystem::~OpenGLSystem() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glfwDestroyWindow(g_window);
		glfwTerminate();
	}

	void OpenGLSystem::onLoad(const char *) noexcept {
		if (!g_init)
			return;

		_em += [](Entity & e) { e += AdjustableComponent("[Render/Planes] Near", &g_params.nearPlane); };
		_em += [](Entity & e) { e += AdjustableComponent("[Render/Planes] Far", &g_params.farPlane); };

#ifndef KENGINE_OPENGL_NO_DEFAULT_SHADERS
		addShaders();
#endif

#ifndef KENGINE_NDEBUG
		_em += [](Entity & e) { e += AdjustableComponent("[ImGui] Scale", &g_dpiScale); };

		_em += Controllers::ShaderController(_em);
		_em += Controllers::GBufferDebugger(_em, _gBufferIterator);
#endif

		for (const auto &[e, depthMap] : _em.getEntities<CSMComponent>()) {
			glDeleteFramebuffers(1, &depthMap.fbo);
			depthMap.fbo = -1;
			glDeleteTextures(lengthof(depthMap.textures), depthMap.textures);
			for (auto & texture : depthMap.textures)
				texture = -1;
		}
		for (const auto &[e, depthMap] : _em.getEntities<DepthMapComponent>()) {
			glDeleteFramebuffers(1, &depthMap.fbo);
			depthMap.fbo = -1;
			glDeleteTextures(1, &depthMap.texture);
			depthMap.texture = -1;
		}
		for (const auto &[e, depthCube] : _em.getEntities<DepthCubeComponent>()) {
			glDeleteFramebuffers(1, &depthCube.fbo);
			depthCube.fbo = -1;
			glDeleteTextures(1, &depthCube.texture);
			depthCube.texture = -1;
		}

		for (const auto & [e, modelInfo] : _em.getEntities<OpenGLModelComponent>())
			for (auto & mesh : modelInfo.meshes) {
				glDeleteVertexArrays(1, &mesh.vertexArrayObject);
				mesh.vertexArrayObject = -1;
				glDeleteBuffers(1, &mesh.vertexBuffer);
				mesh.vertexBuffer = -1;
				glDeleteBuffers(1, &mesh.indexBuffer);
				mesh.indexBuffer = -1;
			}
	}

	void OpenGLSystem::createObject(Entity & e, const ModelLoaderComponent & modelLoader) {
		const auto modelData = modelLoader.load();

		auto & modelInfo = e.attach<OpenGLModelComponent>();
		modelInfo.meshes.clear();
		modelInfo.vertexRegisterFunc = modelLoader.vertexRegisterFunc;

		for (const auto & meshData : modelData.meshes) {
			OpenGLModelComponent::Mesh meshInfo;
			glGenVertexArrays(1, &meshInfo.vertexArrayObject);
			glBindVertexArray(meshInfo.vertexArrayObject);

			glGenBuffers(1, &meshInfo.vertexBuffer);
			glBindBuffer(GL_ARRAY_BUFFER, meshInfo.vertexBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshData.vertices.nbElements * meshData.vertices.elementSize, meshData.vertices.data, GL_STATIC_DRAW);

			glGenBuffers(1, &meshInfo.indexBuffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshInfo.indexBuffer);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData.indices.nbElements * meshData.indices.elementSize, meshData.indices.data, GL_STATIC_DRAW);

			modelLoader.vertexRegisterFunc();

			meshInfo.nbIndices = meshData.indices.nbElements;
			meshInfo.indexType = meshData.indexType;

			modelInfo.meshes.push_back(meshInfo);
		}

		modelLoader.free();
		e.detach<ModelLoaderComponent>();
	}

	static void loadTexture(Entity & e, TextureLoaderComponent & textureLoader) {
		if (*textureLoader.textureID == -1)
			glGenTextures(1, textureLoader.textureID);

		if (textureLoader.data != nullptr) {
			GLenum format;

			switch (textureLoader.components) {
			case 1:
				format = GL_RED;
				break;
			case 3:
				format = GL_RGB;
				break;
			case 4:
				format = GL_RGBA;
				break;
			default:
				assert(false);
			}

			glBindTexture(GL_TEXTURE_2D, *textureLoader.textureID);
			glTexImage2D(GL_TEXTURE_2D, 0, format, textureLoader.width, textureLoader.height, 0, format, GL_UNSIGNED_BYTE, textureLoader.data);
			glGenerateMipmap(GL_TEXTURE_2D);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			if (textureLoader.free != nullptr)
				textureLoader.free(textureLoader.data);
		}

		e.detach<TextureLoaderComponent>();
	}

	struct EntityTextureComponent {
		putils::Point2ui size;
		float * texture;
		bool upToDate;
	};

	void OpenGLSystem::execute() noexcept {
		static bool first = true;
		if (first) {
			init();
			onLoad("");

			send(packets::ImGuiScale{ g_dpiScale });
#ifndef KENGINE_NO_DEFAULT_GBUFFER
			initGBuffer<GBufferTextures>(_em);
#endif
			first = false;
		}

		for (const auto & [e, entityTexture] : _em.getEntities<EntityTextureComponent>())
			entityTexture.upToDate = false;

		glfwPollEvents();

		if (glfwGetWindowAttrib(g_window, GLFW_ICONIFIED)) {
			glfwSwapBuffers(g_window);
			return;
		}

		if (glfwWindowShouldClose(g_window)) {
			_em.running = false;
			return;
		}

		handleInput();

		for (auto &[e, meshLoader] : _em.getEntities<ModelLoaderComponent>()) {
			createObject(e, meshLoader);
			if (e.componentMask == 0)
				_em.removeEntity(e);
		}

		for (auto &[e, textureLoader] : _em.getEntities<TextureLoaderComponent>()) {
			loadTexture(e, textureLoader);
			if (e.componentMask == 0)
				_em.removeEntity(e);
		}

		doOpenGL();

#ifndef KENGINE_NDEBUG
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		for (const auto &[e, comp] : _em.getEntities<ImGuiComponent>())
			comp.display(GImGui);
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
#endif

		glfwSwapBuffers(g_window);
	}

	static void axisToQuaternion(float * quat, const glm::vec3 & axis, float angle) {
		angle = angle * 0.5f;
		const float sinus = std::sin(angle);
		quat[0] = sinus * axis[0];
		quat[1] = sinus * axis[1];
		quat[2] = sinus * axis[2];
		quat[3] = std::cos(angle);
	}

	inline void quaternionToMatrix(float * mat, const float *qs) {
		float q[4];
		float s, xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz, den;
		q[0] = -qs[0];
		q[1] = -qs[1];
		q[2] = -qs[2];
		q[3] = qs[3];

		den = (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);

		if (den == 0.0f)
			s = 1.0f;
		else
			s = 2.0f / den;

		xs = q[0] * s;   ys = q[1] * s;  zs = q[2] * s;
		wx = q[3] * xs;  wy = q[3] * ys; wz = q[3] * zs;
		xx = q[0] * xs;  xy = q[0] * ys; xz = q[0] * zs;
		yy = q[1] * ys;  yz = q[1] * zs; zz = q[2] * zs;

		mat[0] = 1.0f - (yy + zz);
		mat[3] = xy - wz;
		mat[6] = xz + wy;

		mat[1] = xy + wz;
		mat[4] = 1.0f - (xx + zz);
		mat[7] = yz - wx;

		mat[2] = xz - wy;
		mat[5] = yz + wx;
		mat[8] = 1.0f - (xx + yy);
	}

	static void rotate(glm::vec3 & dst, const glm::vec3 & axis, float angle) {
		float quat[4];
		axisToQuaternion(quat, axis, angle);
		float mat[9];
		quaternionToMatrix(mat, quat);

		float x, y, z;
		x = dst[0] * mat[0] + dst[1] * mat[3] + dst[2] * mat[6];
		y = dst[0] * mat[1] + dst[1] * mat[4] + dst[2] * mat[7];
		z = dst[0] * mat[2] + dst[1] * mat[5] + dst[2] * mat[8];
		dst[0] = x;
		dst[1] = y;
		dst[2] = z;
	}

	struct CameraFramebufferComponent {
		GLuint fbo = -1;
		GLuint depthTexture = -1;
		putils::Point2i resolution;
	};

	static void initFramebuffer(Entity & e) {
		auto & viewport = e.get<ViewportComponent>();
		if (viewport.resolution.x == 0 || viewport.resolution.y == 0)
			return;

		auto & fb = e.attach<CameraFramebufferComponent>();
		fb.resolution = viewport.resolution;

		if (fb.fbo == -1)
			glGenFramebuffers(1, &fb.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);

		GLuint texture = (GLuint)viewport.renderTexture;
		if (viewport.renderTexture == (ViewportComponent::RenderTexture)-1)
			glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, viewport.resolution.x, viewport.resolution.y, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

		if (fb.depthTexture == -1)
			glGenTextures(1, &fb.depthTexture);
		glBindTexture(GL_TEXTURE_2D, fb.depthTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, viewport.resolution.x, viewport.resolution.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fb.depthTexture, 0);

		assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		viewport.renderTexture = (ViewportComponent::RenderTexture)texture;
	}

	void OpenGLSystem::doOpenGL() noexcept {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		for (auto &[e, cam, viewport] : _em.getEntities<CameraComponent3f, ViewportComponent>()) {
			g_params.viewPort.size = viewport.resolution;

			putils::gl::setViewPort(g_params.viewPort);

			g_params.camPos = ShaderHelper::toVec(cam.frustum.position);
			g_params.camFOV = cam.frustum.size.y;

			g_params.view = [&] {
				const auto front = glm::normalize(glm::vec3{
					std::sin(cam.yaw) * std::cos(cam.pitch),
					std::sin(cam.pitch),
					std::cos(cam.yaw) * std::cos(cam.pitch)
				});
				const auto right = glm::normalize(glm::cross(front, { 0.f, 1.f, 0.f }));
				auto up = glm::normalize(glm::cross(right, front));
				rotate(up, front, cam.roll);

				return glm::lookAt(g_params.camPos, g_params.camPos + front, up);
			}();

			g_params.proj = glm::perspective(
				g_params.camFOV,
				(float)g_params.viewPort.size.x / (float)g_params.viewPort.size.y,
				g_params.nearPlane, g_params.farPlane
			);

			for (const auto &[e, depthMap] : _em.getEntities<DepthMapComponent>()) {
				ShaderHelper::BindFramebuffer b(depthMap.fbo);
				glClear(GL_DEPTH_BUFFER_BIT);
			}

			for (const auto &[e, depthMap] : _em.getEntities<CSMComponent>()) {
				ShaderHelper::BindFramebuffer b(depthMap.fbo);
				for (const auto texture : depthMap.textures) {
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
					glClear(GL_DEPTH_BUFFER_BIT);
				}
			}

			for (const auto &[e, depthCube] : _em.getEntities<DepthCubeComponent>()) {
				ShaderHelper::BindFramebuffer b(depthCube.fbo);
				glClear(GL_DEPTH_BUFFER_BIT);
			}

			static const auto runShaders = [](auto && shaders) {
				for (auto & [e, comp] : shaders)
					if (comp.enabled) {
#ifndef KENGINE_NDEBUG
						struct ShaderProfiler {
							ShaderProfiler(Entity & e) {
								if (!e.has<Controllers::ShaderProfileComponent>())
									_comp = &e.attach<Controllers::ShaderProfileComponent>();
								else
									_comp = &e.get<Controllers::ShaderProfileComponent>();
								_timer.restart();
							}

							~ShaderProfiler() {
								_comp->executionTime = _timer.getTimeSinceStart().count();
							}

							Controllers::ShaderProfileComponent * _comp;
							putils::Timer _timer;
						};
						ShaderProfiler _(e);
#endif
						comp.shader->run(g_params);
					}
			};

			if (!e.has<GBufferComponent>()) {
				auto & gbuffer = e.attach<GBufferComponent>();
				gbuffer.init(viewport.resolution.x, viewport.resolution.y, g_gBufferTextureCount);
				std::cout << gbuffer.textures.size() << '\n';
			}
			auto & gbuffer = e.get<GBufferComponent>();
			if (gbuffer.getSize() != viewport.resolution)
				gbuffer.resize(viewport.resolution.x, viewport.resolution.y);

			gbuffer.bindForWriting(); {
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				ShaderHelper::Enable depth(GL_DEPTH_TEST);
				runShaders(_em.getEntities<GBufferShaderComponent>());
			} gbuffer.bindForReading();

			if (!e.has<CameraFramebufferComponent>() || e.get<CameraFramebufferComponent>().resolution != viewport.resolution)
				initFramebuffer(e);
			auto & fb = e.get<CameraFramebufferComponent>();
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.fbo);
			glClear(GL_COLOR_BUFFER_BIT);

			glBlitFramebuffer(0, 0, (GLint)g_params.viewPort.size.x, (GLint)g_params.viewPort.size.y, 0, 0, (GLint)g_params.viewPort.size.x, (GLint)g_params.viewPort.size.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

			runShaders(_em.getEntities<LightingShaderComponent>());
			runShaders(_em.getEntities<PostLightingShaderComponent>());
			runShaders(_em.getEntities<PostProcessShaderComponent>());

			glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.fbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

			const auto destSizeX = (GLint)(viewport.boundingBox.size.x * g_screenSize.x);
			const auto destSizeY = (GLint)(viewport.boundingBox.size.y * g_screenSize.y);

			const auto destX = (GLint)(viewport.boundingBox.position.x * g_screenSize.x);
			// OpenGL coords have Y=0 at the bottom, I want Y=0 at the top
			const auto destY = (GLint)(g_screenSize.y - destSizeY - viewport.boundingBox.position.y * g_screenSize.y);

			glBlitFramebuffer(
				// src
				0, 0, fb.resolution.x, fb.resolution.y,
				// dest
				destX, destY, destX + destSizeX, destY + destSizeY,
				GL_COLOR_BUFFER_BIT, GL_LINEAR
			);

			if (!e.has<ImGuiComponent>())
				e += ImGuiComponent([&] {
					if (ImGui::Begin("FBO")) {
						const auto start = ImGui::GetWindowContentRegionMin();
						const auto end = ImGui::GetWindowContentRegionMax();
						ImGui::Image((ImTextureID)viewport.renderTexture, { end.x - start.x, end.y - start.y }, { 0, 1 }, { 1, 0 });
					}
					ImGui::End();
				});
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLSystem::handleInput() noexcept {
		for (const auto &[e, comp] : _em.getEntities<InputComponent>()) {
			if (!ImGui::GetIO().WantCaptureKeyboard)
				for (const auto & e : Input::keys)
					if (comp.onKey != nullptr)
						comp.onKey(e.key, e.pressed);

			if (!ImGui::GetIO().WantCaptureMouse) {
				if (comp.onMouseButton != nullptr)
					for (const auto & click : Input::clicks)
						 comp.onMouseButton(click.button, click.pos, click.pressed);

				if (comp.onMouseMove != nullptr)
					for (const auto & pos : Input::positions)
						comp.onMouseMove(pos.pos, pos.rel);
				if (comp.onMouseWheel != nullptr)
					for (const auto delta : Input::scrolls)
						comp.onMouseWheel(delta, {});
			}
		}
		Input::keys.clear();
		Input::clicks.clear();
		Input::positions.clear();
		Input::scrolls.clear();
	}

	void OpenGLSystem::handle(packets::GetEntityInPixel p) {
		static constexpr auto GBUFFER_TEXTURE_COMPONENTS = 4;
		static constexpr auto GBUFFER_ENTITY_LOCATION = 3;

		const auto viewportInfo = CameraHelper::getViewportForPixel(_em, p.pixel, g_screenSize);
		if (viewportInfo.camera == Entity::INVALID_ID) {
			p.id = Entity::INVALID_ID;
			return;
		}

		auto & camera = _em.getEntity(viewportInfo.camera);

		const auto & gbuffer = camera.get<GBufferComponent>();
		const putils::Point2ui gBufferSize = gbuffer.getSize();

		const auto pixelInGBuffer = putils::Point2i(viewportInfo.pixel * gBufferSize);

		if (pixelInGBuffer.x >= gBufferSize.x || pixelInGBuffer.y > gBufferSize.y || pixelInGBuffer.y == 0) {
			p.id = Entity::INVALID_ID;
			return;
		}

		if (!camera.has<EntityTextureComponent>()) {
			auto & entityTexture = camera.attach<EntityTextureComponent>();
			entityTexture.size = gBufferSize;
			entityTexture.texture = new float[gBufferSize.x * gBufferSize.y * 4];
			gbuffer.getTexture(GBUFFER_ENTITY_LOCATION, entityTexture.texture, gBufferSize.x * gBufferSize.y * GBUFFER_TEXTURE_COMPONENTS);
			entityTexture.upToDate = true;
		}

		auto & entityTexture = camera.get<EntityTextureComponent>();
		if (entityTexture.size != gBufferSize) {
			delete[] entityTexture.texture;
			entityTexture.texture = new float[gBufferSize.x * gBufferSize.y * 4];
			entityTexture.upToDate = false;
			entityTexture.size = gBufferSize;
		}

		if (!entityTexture.upToDate) {
			gbuffer.getTexture(GBUFFER_ENTITY_LOCATION, entityTexture.texture, gBufferSize.x * gBufferSize.y * GBUFFER_TEXTURE_COMPONENTS);
			entityTexture.upToDate = true;
		}

		const auto index = (pixelInGBuffer.x + (gBufferSize.y - pixelInGBuffer.y) * gBufferSize.x) * GBUFFER_TEXTURE_COMPONENTS;
		p.id = (Entity::ID)entityTexture.texture[index];
		if (p.id == 0)
			p.id = Entity::INVALID_ID;
	}
}
