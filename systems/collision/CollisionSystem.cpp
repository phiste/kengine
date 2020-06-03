#include "CollisionSystem.hpp"
#include "EntityManager.hpp"

#include "functions/OnCollision.hpp"
#include "data/CollisionComponent.hpp"

namespace kengine {
#pragma region declarations
	static void onCollision(Entity & first, Entity & second);
#pragma endregion
	EntityCreator * CollisionSystem(EntityManager & em) {
		return [](Entity & e) {
			e += functions::OnCollision{ onCollision };
		};
	}

#pragma region onCollision
#pragma region declarations
	static void trigger(Entity & first, Entity & second);
#pragma endregion
	static void onCollision(Entity & first, Entity & second) {
		trigger(first, second);
		trigger(second, first);
	}

	static void trigger(Entity & first, Entity & second) {
		if (!first.has<CollisionComponent>())
			return;

		const auto & comp = first.get<CollisionComponent>();
		if (comp.onCollide != nullptr)
			comp.onCollide(first, second);
	}
#pragma endregion onCollision
}