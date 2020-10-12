#include "ImGuiEntitySelectorSystem.hpp"

#include "data/ImGuiToolComponent.hpp"
#include "data/NameComponent.hpp"
#include "data/SelectedComponent.hpp"
#include "functions/Execute.hpp"

#include "helpers/typeHelper.hpp"
#include "helpers/sortHelper.hpp"
#include "meta/Has.hpp"
#include "meta/MatchString.hpp"

#include "helpers/imguiHelper.hpp"
#include "imgui.h"

#include "string.hpp"
#include "to_string.hpp"

namespace kengine::imgui_entity_selector {
	struct impl {
		static inline EntityManager * em;
		static inline bool * enabled;

		static void init(Entity & e) {
			e += NameComponent{ "Entity selector" };
			auto & tool = e.attach<ImGuiToolComponent>();
			tool.enabled = true;
			enabled = &tool.enabled;

			e += functions::Execute{ execute };
		}

		static void execute(float deltaTime) {
			if (!*enabled)
				return;

			if (ImGui::Begin("Entity selector", enabled)) {
				static char nameSearch[1024];
				ImGui::InputText("Search", nameSearch, sizeof(nameSearch));

				ImGui::Separator();

				ImGui::BeginChild("child");
				for (auto & e : em->getEntities())
					if (matches(e, nameSearch, *em)) {
						if (e.has<SelectedComponent>())
							e.detach<SelectedComponent>();
						else
							e.attach<SelectedComponent>();
					}
				ImGui::EndChild();
			}
			ImGui::End();
		}

		static bool matches(const Entity & e, const char * str, EntityManager & em) {
			putils::string<1024> displayText("[%d]", e.id);;
			const auto name = e.tryGet<NameComponent>();
			if (name) {
				displayText += " ";
				displayText += name->name;
			}


			if (strlen(str) != 0) {
				displayText += " -- ";

				bool matches = false;
				if (str[0] >= '0' && str[0] <= '9' && putils::parse<Entity::ID>(str) == e.id) {
					matches = true;
					displayText += "ID";
				}
				else {
					const auto types = sortHelper::getNameSortedEntities<KENGINE_COMPONENT_COUNT,
						meta::Has, meta::MatchString
					>(em);

					for (const auto & [_, type, has, matchFunc] : types) {
						if (!has->call(e) || !matchFunc->call(e, str))
							continue;

						if (displayText.size() + type->name.size() + 2 < decltype(displayText)::max_size) {
							if (matches) // Isn't the first time
								displayText += ", ";
							displayText += type->name;
						}
						matches = true;
					}
				}

				if (!matches)
					return false;
			}

			bool ret = false;
			const auto openTreeNode = ImGui::TreeNode(displayText + "##" + e.id);
			if (ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem("Select"))
					ret = true;
				if (ImGui::MenuItem("Remove")) {
					em.removeEntity(e);
					return false;
				}
				ImGui::EndPopup();
			}
			if (openTreeNode) {
				imguiHelper::displayEntity(em, e);
				ImGui::TreePop();
			}

			return ret;
		}
	};
}

namespace kengine {
	EntityCreatorFunctor<64> ImGuiEntitySelectorSystem(EntityManager & em) {
		imgui_entity_selector::impl::em = &em;
		return [&](Entity & e) {
			imgui_entity_selector::impl::init(e);
		};
	}
}