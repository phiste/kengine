#include <GLFW/glfw3.h>

#include "GLFWSystem.hpp"
#include "kengine.hpp"

#include "data/InputBufferComponent.hpp"
#include "data/GLFWWindowComponent.hpp"
#include "data/WindowComponent.hpp"

#include "functions/OnTerminate.hpp"
#include "functions/OnMouseCaptured.hpp"
#include "functions/Execute.hpp"

#include "imgui.h"
#include "with.hpp"

namespace kengine::glfw {
	namespace Input {
		static InputBufferComponent * g_buffer;

		static void onKey(GLFWwindow * window, int key, int scancode, int action, int mods) noexcept {
			if (g_buffer == nullptr || (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard))
				return;

			const auto id = (EntityID)glfwGetWindowUserPointer(window);

			if (action == GLFW_PRESS)
				g_buffer->keys.push_back(InputBufferComponent::KeyEvent{ id, key, true });
			else if (action == GLFW_RELEASE)
				g_buffer->keys.push_back(InputBufferComponent::KeyEvent{ id, key, false });
		}

		static putils::Point2f lastPos{ FLT_MAX, FLT_MAX };

		static void onClick(GLFWwindow * window, int button, int action, int mods) noexcept {
			if (g_buffer == nullptr || (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse))
				return;

			const auto id = (EntityID)glfwGetWindowUserPointer(window);

			if (action == GLFW_PRESS)
				g_buffer->clicks.push_back(InputBufferComponent::ClickEvent{ id, lastPos, button, true });
			else if (action == GLFW_RELEASE)
				g_buffer->clicks.push_back(InputBufferComponent::ClickEvent{ id, lastPos, button, false });
		}

		static void onMouseMove(GLFWwindow * window, double xpos, double ypos) noexcept {
			if (lastPos.x == FLT_MAX) {
				lastPos.x = (float)xpos;
				lastPos.y = (float)ypos;
			}

			if (g_buffer == nullptr || (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse))
				return;

			const auto id = (EntityID)glfwGetWindowUserPointer(window);

			InputBufferComponent::MouseMoveEvent info;
			info.window = id;
			info.pos = { (float)xpos, (float)ypos };
			info.rel = { (float)xpos - lastPos.x, (float)ypos - lastPos.y };
			lastPos = info.pos;

			g_buffer->moves.push_back(info);
		}

		static void onScroll(GLFWwindow * window, double xoffset, double yoffset) noexcept {
			if (g_buffer == nullptr || (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse))
				return;

			const auto id = (EntityID)glfwGetWindowUserPointer(window);
			g_buffer->scrolls.push_back(InputBufferComponent::MouseScrollEvent{ id, (float)xoffset, (float)yoffset, lastPos });
		}
	}

	struct impl {
		static void init(Entity & e) noexcept {
			e += functions::Execute{ glfw::impl::execute };
			e += functions::OnEntityCreated{ glfw::impl::onEntityCreated };
			e += functions::OnTerminate{ glfw::impl::terminate };
			e += functions::OnMouseCaptured{ glfw::impl::onMouseCaptured };
		
			for (const auto & [e, buffer] : entities.with<InputBufferComponent>()) {
				Input::g_buffer = &buffer;
				break;
			}

			glfwInit();
			execute(0.f); // init already existing windows
		}

		static void terminate() noexcept {
			glfwTerminate();
		}

		static void onMouseCaptured(EntityID window, bool captured) noexcept {
			const auto inputMode = captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;

			if (captured)
				ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
			else
				ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

			if (window == INVALID_ID) {
				for (const auto & [e, glfw] : entities.with<GLFWWindowComponent>())
					glfwSetInputMode(glfw.window.get(), GLFW_CURSOR, inputMode);
				return;
			}

			const auto glfw = entities.get(window).tryGet<GLFWWindowComponent>();
			if (glfw == nullptr)
				return;

			glfwSetInputMode(glfw->window.get(), GLFW_CURSOR, inputMode);
		}

		static void onEntityCreated(Entity & e) noexcept {
			const auto window = e.tryGet<WindowComponent>();
			if (!window)
				return;

			const auto initGlfw = e.tryGet<GLFWWindowInitComponent>();
			if (!initGlfw)
				return;

			createWindow(e, *window, *initGlfw);
		}

		static void execute(float deltaTime) noexcept {
			for (const auto & [e, window, glfw] : entities.with<WindowComponent, GLFWWindowComponent>())
				if (glfwWindowShouldClose(glfw.window.get())) {
					if (window.shutdownOnClose)
						stopRunning();
					else
						entities.remove(e.id);
				}

			for (auto [e, window, initGlfw, noGLFW] : entities.with<WindowComponent, GLFWWindowInitComponent, no<GLFWWindowComponent>>()) {
				createWindow(e, window, initGlfw);
				e.detach<GLFWWindowInitComponent>();
			}
		}

		static void createWindow(Entity & e, WindowComponent & window, const GLFWWindowInitComponent & initGlfw) noexcept {
			auto & glfwComp = e.attach<GLFWWindowComponent>();

			if (initGlfw.setHints)
				initGlfw.setHints();

			// TODO: depend on g_windowComponent->fullscreen
			glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

			glfwComp.window = glfwCreateWindow((int)window.size.x, (int)window.size.y, window.name, nullptr, nullptr);
			// Desired size may not have been available, update to actual size
			int width, height;
			glfwGetWindowSize(glfwComp.window.get(), &width, &height);
			window.size = { (unsigned int)width, (unsigned int)height };
			glfwSetWindowAspectRatio(glfwComp.window.get(), window.size.x, window.size.y);

			glfwMakeContextCurrent(glfwComp.window.get());
			glfwSetWindowSizeCallback(glfwComp.window.get(), [](auto window, int width, int height) noexcept {
				const auto id = (EntityID)glfwGetWindowUserPointer(window);
				auto & comp = entities.get(id).get<WindowComponent>();
				comp.size = { (unsigned int)width, (unsigned int)height };
			});

			glfwSetMouseButtonCallback(glfwComp.window.get(), Input::onClick);
			glfwSetCursorPosCallback(glfwComp.window.get(), Input::onMouseMove);
			glfwSetScrollCallback(glfwComp.window.get(), Input::onScroll);
			glfwSetKeyCallback(glfwComp.window.get(), Input::onKey);

			glfwSetWindowUserPointer(glfwComp.window.get(), (void *)e.id);

			if (initGlfw.onWindowCreated)
				initGlfw.onWindowCreated();
		}
	};
}

namespace kengine {
	EntityCreator * GLFWSystem() noexcept {
		return [](Entity & e) noexcept {
			glfw::impl::init(e);
		};
	}
}