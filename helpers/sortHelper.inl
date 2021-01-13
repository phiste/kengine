#include "sortHelper.hpp"

#include "kengine.hpp"
#include "data/NameComponent.hpp"
#include "vector.hpp"
#include "fwd.hpp"

namespace kengine::sortHelper {
	namespace detail {
		template<typename PointerTuple, typename RefTuple>
		void setImpl(PointerTuple & p, const RefTuple & r, std::index_sequence<>) noexcept {}

		template<typename PointerTuple, typename RefTuple, size_t I, size_t ...Is>
		void setImpl(PointerTuple & p, const RefTuple & r, std::index_sequence<I, Is...>) noexcept {
			std::get<I + 1>(p) = &std::get<I + 1>(r);
			setImpl(p, r, std::index_sequence<Is...>());
		}

		template<typename PointerTuple, typename RefTuple, size_t ...Is>
		void set(PointerTuple & p, const RefTuple & r, std::index_sequence<Is...> is) noexcept {
			std::get<0>(p) = std::get<0>(r); // 0 is Entity
			setImpl(p, r, is);
		}
	}

	template<size_t MaxCount, typename ... Comps, typename Pred>
	auto getSortedEntities(Pred && pred) noexcept { 
		using Type = std::tuple<Entity, Comps *...>;

		using Ret = std::conditional_t<
			MaxCount == 0,
			std::vector<Type>,
			putils::vector<Type, MaxCount>
		>;
		Ret ret;

		for (const auto & t : entities.with<Comps...>()) {
			if constexpr (putils::is_vector<Ret>::value)
				if (ret.full())
					break;
			ret.emplace_back();
			detail::set(ret.back(), t, std::make_index_sequence<sizeof...(Comps)>());
		}
		std::sort(ret.begin(), ret.end(), FWD(pred));

		return ret;
	}

	template<typename ... Comps, typename Pred>
	auto getSortedEntities(Pred && pred) noexcept {
		return getSortedEntities<0, Comps...>(FWD(pred));
	}

	template<size_t MaxCount, typename ... Comps>
	auto getNameSortedEntities() noexcept {
		return getSortedEntities<MaxCount, NameComponent, Comps...>(
			[](const auto & lhs, const auto & rhs) noexcept {
				return strcmp(std::get<1>(lhs)->name, std::get<1>(rhs)->name) < 0;
			}
		);
	}

	template<typename ... Comps>
	auto getNameSortedEntities() noexcept {
		return getNameSortedEntities<0, Comps...>();
	}
}