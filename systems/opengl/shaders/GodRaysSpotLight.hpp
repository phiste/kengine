#pragma once

#include "opengl/Program.hpp"
#include "GodRaysSrc.hpp"
#include "ShadowMapSrc.hpp"
#include "PointLightSrc.hpp"

namespace kengine {
	class EntityManager;
	struct SpotLightComponent;
	struct DepthMapComponent;
}

namespace kengine::Shaders {
	class GodRaysSpotLight : public putils::gl::Program,
		public src::GodRays::Frag::Uniforms,
		public src::ShadowMap::Frag::Uniforms,
		public src::PointLight::GetDirection::Uniforms
	{
	public:
		GodRaysSpotLight(EntityManager & em);

		void init(size_t firstTextureID) override;
		void run(const Parameters & params) override;
		
	private:
		void drawLight(const SpotLightComponent & light, const putils::Point3f & pos, const DepthMapComponent & depthMap, const Parameters & params);

	private:
		EntityManager & _em;
		GLuint _shadowMapTextureID;

	public:
		putils_reflection_parents(
			putils_reflection_type(src::GodRays::Frag::Uniforms),
			putils_reflection_type(src::ShadowMap::Frag::Uniforms),
			putils_reflection_type(src::PointLight::GetDirection::Uniforms)
		);
	};
}
