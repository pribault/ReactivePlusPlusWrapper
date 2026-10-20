#pragma once

// stl
#include <concepts>
#include <cstddef>
#include <exception>
#include <ranges>
#include <type_traits>
#include <utility>

// rpp
#include <rpp/rpp.hpp>

// rppw
#include <rppw/Concepts.h>
#include <rppw/Disposable.h>

namespace rppw
{
	/**
	 * @brief Cold source of zero or more values, followed by completion or an error.
	 * @tparam T Unqualified value type satisfying std::copyable.
	 * @details Each subscription executes its own chain. Composing operators does not
	 * invoke producers. Without an explicit scheduler, synchronous processing runs
	 * on the thread calling subscribe().
	 * Producer and operator exceptions are forwarded through std::exception_ptr.
	 * An error or completion terminates the subscription; no further events are delivered.
	 * Reactive objects are copyable and share their type-erased RPP representation,
	 * which may allocate. Operators return a new chain.
	 * @warning Reference captures must outlive their use, including asynchronous use.
	 * The caller must synchronize data shared between subscriptions.
	 * No backpressure mechanism is provided.
	 *
	 * @include rppw/Observable.h
	 */
	template <detail::Value T>
	class Observable
	{
	public:
		/** @brief Type of emitted values. */
		using value_type = T;

		/**
		 * @brief Creates a source that emits the stored value and then completes.
		 * @param value Value owned by the source and emitted for each subscription.
		 * @return Cold source containing one element.
		 */
		static Observable just(T value);

		/**
		 * @brief Copies range elements and defers their emission until subscription.
		 * @tparam R Input range whose elements are convertible to T.
		 * @param range Elements copied into an owned collection in traversal order.
		 * @return Source that emits the elements in order and completes, even when empty.
		 * @note Traversal, conversions and allocations happen immediately;
		 * their exceptions are synchronous. No reference to the range is retained.
		 */
		template <concepts::RangeOf<T> R>
		static Observable fromIterable(R &&range);

		/**
		 * @brief Creates a source that completes without emitting a value.
		 * @return Empty source.
		 */
		static Observable empty();

		/**
		 * @brief Creates a source that only signals the supplied error.
		 * @param error Non-null exception retained for each subscription.
		 * @return Failing source.
		 * @throws std::invalid_argument If error is null.
		 */
		static Observable error(std::exception_ptr error);

		/**
		 * @brief Captures an error with std::make_exception_ptr and creates a failing source.
		 * @tparam E Copy-constructible type satisfying concepts::ExceptionValue.
		 * @param error Error copied into the exception pointer retained by the source.
		 * @return Source that forwards the captured error for each subscription.
		 * @note Capture happens immediately; only notification is deferred.
		 */
		template <concepts::ExceptionValue E>
		static Observable error(E error);

		/**
		 * @brief Creates a new source through the factory for each subscription.
		 * @tparam F Const-invocable callable returning `Observable<T>`.
		 * @param factory Factory stored by value and invoked at subscription time.
		 * @return Deferred source; factory exceptions are forwarded to onError.
		 */
		template <concepts::SourceFactoryFor<T, rppw::Observable> F>
		static Observable defer(F factory);

		/**
		 * @brief Subscribes the three callbacks and starts processing.
		 * @tparam N Type of the callback receiving a value of type T.
		 * @tparam E Type of the callback receiving std::exception_ptr.
		 * @tparam C Type of the completion callback taking no arguments.
		 * @param onNext Callback invoked for each value.
		 * @param onError Terminal error callback.
		 * @param onComplete Terminal normal-completion callback.
		 * @return Shared cancellation handle, already disposed if the source terminates synchronously.
		 * @pre All three callbacks must be const-invocable and must not throw.
		 * @warning An exception escaping a callback invokes std::terminate.
		 * @note Callbacks are retained throughout processing. Destroying the returned
		 * handle does not cancel the subscription. Callbacks of one subscription are
		 * serialized; concurrent cancellation does not interrupt a callback already in progress.
		 */
		template <concepts::CallbackFor<T> N, concepts::CallbackFor<std::exception_ptr &> E, concepts::CallbackFor C>
		Disposable subscribe(N onNext, E onError, C onComplete) const;

		/**
		 * @brief Transforms each value without modifying the source.
		 * @tparam F Const-invocable callable producing an admissible, non-reference value.
		 * @param function Transformation applied to each element.
		 * @return `Observable<U>`, where U is the decayed result type.
		 * @note An exception from function terminates the chain through onError.
		 */
		template <concepts::MapperFor<T> F>
		auto map(F function) const;

		/**
		 * @brief Keeps only values accepted by the predicate.
		 * @tparam F Const-invocable predicate accepting const T&.
		 * @param predicate Test determining whether to keep a value.
		 * @return Filtered source, which may be empty.
		 * @note A predicate exception is forwarded to onError.
		 */
		template <concepts::PredicateFor<T> F>
		Observable filter(F predicate) const;

		/**
		 * @brief Merges the sources produced for each value.
		 * @tparam F Const-invocable callable returning `Observable<U>`.
		 * @param function Inner-source factory.
		 * @return Merged source; an error cancels upstream and all inner sources.
		 * @note Ordering between concurrent sources is not guaranteed. Each source
		 * preserves its own order. Exceptions from function are forwarded to onError.
		 * @warning No concurrency or memory limit is imposed.
		 */
		template <concepts::FlatMapperFor<T, rppw::Observable> F>
		auto flatMap(F function) const;

		/**
		 * @brief Completes after at most count values and cancels upstream.
		 * @param count Maximum number of values to forward.
		 * @return Limited source; a zero count completes without subscribing upstream.
		 */
		Observable take(std::size_t count) const;

		/**
		 * @brief Schedules the upstream subscription on the supplied scheduler.
		 * @tparam S ReactivePlusPlus scheduler type.
		 * @param scheduler Subscription execution context.
		 * @return Chain of the same type with a scheduled subscription.
		 * @note Does not guarantee the notification context of an asynchronous source.
		 */
		template <rpp::schedulers::constraint::scheduler S>
		Observable subscribeOn(S scheduler) const;

		/**
		 * @brief Schedules values and terminal notifications on the supplied scheduler.
		 * @tparam S ReactivePlusPlus scheduler type.
		 * @param scheduler Downstream notification delivery context.
		 * @return Chain of the same type preserving notification order, including errors.
		 * @note Delegates to the RPP delay operator with zero delay to schedule errors as well.
		 * @warning The queue is unbounded if the consumer is slower than the source.
		 */
		template <rpp::schedulers::constraint::scheduler S>
		Observable observeOn(S scheduler) const;

		/**
		 * @brief Requires exactly one value before upstream completion.
		 * @return Single that succeeds at completion if exactly one value was received.
		 * @note Zero or two values produce std::out_of_range through the reactive
		 * error channel; the second value immediately cancels upstream.
		 * An upstream error is preserved even after a first value.
		 */
		Single<T> singleOrError() const;

		/**
		 * @brief Accepts at most one value and waits for upstream completion.
		 * @return Empty Maybe or one containing the unique value.
		 * @note The second value produces std::out_of_range through the reactive
		 * error channel and cancels upstream. An upstream error is not replaced with success.
		 */
		Maybe<T> singleElement() const;

		/**
		 * @brief Ignores values while preserving upstream termination.
		 * @return Completable that completes or forwards this source's error.
		 */
		Completable ignoreElements() const;

	private:
		explicit Observable(rpp::dynamic_observable<T> source)
			: source_{std::move(source)}
		{
		}
		rpp::dynamic_observable<T> source_;
		template <detail::Value>
		friend class Observable;
		template <detail::Value>
		friend class Single;
		template <detail::Value>
		friend class Maybe;
		friend class Completable;
	};
} // namespace rppw

// rppw
#include <rppw/Observable.inl>
