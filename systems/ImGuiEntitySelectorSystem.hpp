#pragma once

#include "EntityCreator.hpp"

namespace kengine {
	class EntityManager;

	EntityCreatorFunctor<64> ImGuiEntitySelectorSystem(EntityManager & em);
}