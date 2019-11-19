#include <iostream>

#include "go_to_bin_dir.hpp"

#include "EntityManager.hpp"
#include "Entity.hpp"

#include "systems/LuaSystem.hpp"
#include "components/GraphicsComponent.hpp"
#include "components/TransformComponent.hpp"

// Simple system that outputs the transform and lua components of each entity that has them
class DebugSystem : public kengine::System<DebugSystem> {
public:
	DebugSystem(kengine::EntityManager & em) : System<DebugSystem>(em), _em(em) {}

	void execute() final {
		for (const auto & [e, transform, lua] : _em.getEntities<kengine::TransformComponent3f, kengine::LuaComponent>()) {
			std::cout << "Entity " << e.id << '\n';

			std::cout << "\tTransform: " 
				<< transform.boundingBox.position.x << " "
				<< transform.boundingBox.position.y << " "
				<< transform.boundingBox.position.z
				<< '\n';

			std::cout << "\tScripts:" << '\n';
			for (const auto & script : lua.getScripts())
				std::cout << "\t\t[" << script << "]\n";

			std::cout << '\n';
		}
	}

private:
	kengine::EntityManager & _em;
};

int main(int, char **av) {
    // Go to the executable's directory to be next to resources and scripts
    //putils::goToBinDir(av[0]);

    // Create an EntityManager
    kengine::EntityManager em; // Optionally, pass a number of threads as parameter (kengine::EntityManager em(4);)

    // Load the specified systems, and any plugin placed in the executable's directory
    //      If you specify 'KENGINE_SFML' as TRUE in your CMakeLists.txt, this will load the SfSystem
    em.loadSystems<DebugSystem, kengine::LuaSystem>(".");

    // To add a new system, simply add a DLL with a
    //      `ISystem *getSystem(kengine::EntityManager &em)`
    // To add a new system, simply add a DLL with a

    // Create an Entity and attach Components to it
    em += [](kengine::Entity e) {
		e += kengine::TransformComponent3f({ 42.f, 0.f, 42.f }); // Parameter is a Point3f for position
		auto c = kengine::LuaComponent({  }); // Parameter is a vector of scripts
		c.attachScript("scripts/unit.lua");
		e += c;
    };

	// Register types to be used in lua
    try {
        auto &lua = em.getSystem<kengine::LuaSystem>();
        lua.registerTypes<
                kengine::TransformComponent3f, putils::Point<float, 3>, putils::Rect<float, 3>,
                kengine::LuaComponent
        >();
    }
    catch (const std::out_of_range &) {} // If the LuaSystem wasn't found, ignore

    // Start game
    while (em.running)
        em.execute();

    return (EXIT_SUCCESS);
}
