#pragma once

// stl
#include <concepts>
#include <type_traits>

namespace rppw
{
	namespace detail
	{
		/**
		 * @brief Requires an unqualified object value that is copyable and assignable.
		 * @tparam T Candidate value type for reactive sources.
		 * @internal
		 */
		template <typename T>
		concept Value = std::is_object_v<T> && std::same_as<T, std::decay_t<T>> && std::copyable<T>;
	} // namespace detail

	template <detail::Value T>
	class Observable;
	template <detail::Value T>
	class Single;
	template <detail::Value T>
	class Maybe;
	class Completable;
} // namespace rppw
