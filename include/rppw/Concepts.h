#pragma once

// stl
#include <concepts>
#include <exception>
#include <ranges>
#include <type_traits>

// rppw
#include <rppw/detail/Fwd.h>

namespace rppw::concepts
{
	/**
	 * @brief Copy-constructible error without conversion to std::exception_ptr.
	 * @tparam E Error type, not necessarily derived from std::exception.
	 * @note Exception pointers and nullptr remain handled by the dedicated overload.
	 */
	template <typename E>
	concept ExceptionValue = std::is_copy_constructible_v<E> && !std::convertible_to<E, std::exception_ptr>;

	/**
	 * @brief Const-invocable factory with an exact result type.
	 * @tparam F Callable type.
	 * @tparam Result Return type, including void for an action.
	 */
	template <typename F, typename Result>
	concept FactoryFor = std::invocable<const F &> && std::same_as<std::invoke_result_t<const F &>, Result>;

	/**
	 * @brief Const-invocable factory returning the requested reactive source specialization.
	 * @tparam F Callable type.
	 * @tparam T Source value type.
	 * @tparam Source Observable, Single or Maybe template expected as the result.
	 * @note The separate source template avoids self-type constraint mismatches in Clang 15.
	 */
	template <typename F, typename T, template <typename> typename Source>
	concept SourceFactoryFor = FactoryFor<F, Source<T>>;

	/**
	 * @brief Const-invocable callback accepting the specified arguments.
	 * @tparam F Callback type.
	 * @tparam Args Argument types; the callback's result is ignored.
	 * @note Does not check noexcept; subscription callbacks must not throw.
	 */
	template <typename F, typename... Args>
	concept CallbackFor = std::invocable<const F &, Args...>;

	/**
	 * @brief Const-invocable transformation producing an admissible owned value.
	 * @tparam F Callable type accepting T and const T& with the same result type.
	 * @tparam T Source value type.
	 * @note The result must satisfy detail::Value; void and references are excluded.
	 */
	template <typename F, typename T>
	concept MapperFor = std::invocable<const F &, T> && std::invocable<F, T> && detail::Value<std::invoke_result_t<const F &, T>> &&
						std::same_as<std::invoke_result_t<F, T>, std::invoke_result_t<const F &, T>> && requires(const F &function, const T &value) {
							{ function(value) } -> std::same_as<std::invoke_result_t<const F &, T>>;
						};

	/**
	 * @brief Const-invocable predicate accepting a borrowed value.
	 * @tparam F Predicate type.
	 * @tparam T Value type being tested.
	 */
	template <typename F, typename T>
	concept PredicateFor = std::predicate<const F &, const T &>;

	/**
	 * @brief Const-invocable factory returning exactly the expected reactive category.
	 * @tparam F Callable type.
	 * @tparam T Source value type.
	 * @tparam Source Observable, Single or Maybe template expected as the result.
	 */
	template <typename F, typename T, template <typename> typename Source>
	concept FlatMapperFor = std::invocable<const F &, T> && std::invocable<const F &, const T &> &&
							std::same_as<std::invoke_result_t<const F &, T>, std::invoke_result_t<const F &, const T &>> &&
							requires { typename std::invoke_result_t<const F &, T>::value_type; } &&
							std::same_as<std::invoke_result_t<const F &, T>, Source<typename std::invoke_result_t<const F &, T>::value_type>>;

	/**
	 * @brief Input range whose elements are convertible to the requested type.
	 * @tparam R Range type, possibly a reference.
	 * @tparam T Target value type.
	 */
	template <typename R, typename T>
	concept RangeOf = std::ranges::input_range<R> && std::convertible_to<std::ranges::range_reference_t<R>, T>;

	/**
	 * @brief One of the wrapper's four reactive categories, without implicit conversion.
	 * @tparam R Specialized Observable, Single or Maybe, or Completable.
	 */
	template <typename R>
	concept ReactiveSource =
		std::same_as<R, Completable> || (requires { typename R::value_type; } && detail::Value<typename R::value_type> &&
										 (std::same_as<R, Observable<typename R::value_type>> || std::same_as<R, Single<typename R::value_type>> ||
										  std::same_as<R, Maybe<typename R::value_type>>));
} // namespace rppw::concepts
