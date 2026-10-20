#pragma once

// stl
#include <concepts>
#include <cstddef>
#include <exception>
#include <functional>
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
		 * @brief Subscribes to sources sequentially, preserving their order.
		 * @tparam R Input range of Observable<T>, Single<T> or Maybe<T>.
		 * @param sources Homogeneous range copied into owned storage immediately.
		 * @return Observable forwarding all values, completing when all sources complete.
		 * An empty range completes without values; empty Maybes are skipped.
		 * @note Subscription is deferred and repeated for each subscriber.
		 * An error or cancellation prevents subscription to remaining sources.
		 * Range traversal and allocation exceptions are synchronous.
		 */
		template <concepts::ObservableSourceRangeOf<T> R>
		static Observable concat(R &&sources);

		/**
		 * @brief Subscribes to sources without waiting for preceding sources to complete.
		 * @tparam R Input range of Observable<T>, Single<T> or Maybe<T>.
		 * @param sources Homogeneous range copied into owned storage immediately.
		 * @return Observable completing after all sources, or immediately for an empty range.
		 * Empty Maybes contribute no values.
		 * @note Each subscription restarts the sources. Their own value order is preserved;
		 * ordering between sources is unspecified. An error cancels all active sources.
		 * Cancellation is propagated to every active source. No scheduler is added.
		 * Range traversal and allocation exceptions are synchronous.
		 * @warning No concurrency limit is imposed.
		 */
		template <concepts::ObservableSourceRangeOf<T> R>
		static Observable merge(R &&sources);

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
		 * @brief Inspects an error before forwarding it unchanged.
		 * @tparam F Const-invocable callback accepting const std::exception_ptr&.
		 * @param action Callback stored by value and invoked on each subscription's error.
		 * @return Observable preserving all notifications.
		 * @pre The callback must not throw; an escaping exception invokes std::terminate.
		 */
		template <concepts::CallbackFor<const std::exception_ptr &> F>
		Observable doOnError(F action) const;

		/**
		 * @brief Executes an action before forwarding normal completion.
		 * @tparam F Const-invocable no-argument callback.
		 * @param action Callback stored by value; not invoked on error or cancellation.
		 * @return Observable preserving all notifications.
		 * @pre The callback must not throw; an escaping exception invokes std::terminate.
		 */
		template <concepts::CallbackFor F>
		Observable doOnComplete(F action) const;

		/**
		 * @brief Inspects each value before forwarding it unchanged.
		 * @tparam F Const-invocable callback accepting const T&.
		 * @param action Callback stored by value, invoked only when a value arrives.
		 * @return Observable preserving all notifications.
		 * @pre The callback must not throw; an escaping exception invokes std::terminate.
		 */
		template <concepts::CallbackFor<const T &> F>
		Observable doOnNext(F action) const;

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
		 * @brief Tests whether every value satisfies the predicate.
		 * @tparam F Const-invocable predicate accepting const T&.
		 * @param predicate Test applied until the first false result.
		 * @return Single emitting false at the first failure, or true at normal completion.
		 * An empty source produces true.
		 * @note A decisive result cancels upstream. Upstream errors and predicate exceptions
		 * before that result are forwarded as errors. Evaluation is deferred per subscription.
		 */
		template <concepts::PredicateFor<T> F>
		Single<bool> allOf(F predicate) const;

		/**
		 * @brief Tests whether at least one value satisfies the predicate.
		 * @tparam F Const-invocable predicate accepting const T&.
		 * @param predicate Test applied until the first true result.
		 * @return Single emitting true at the first match, or false at normal completion.
		 * An empty source produces false.
		 * @note A decisive result cancels upstream. Upstream errors and predicate exceptions
		 * before that result are forwarded as errors. Evaluation is deferred per subscription.
		 */
		template <concepts::PredicateFor<T> F>
		Single<bool> anyOf(F predicate) const;

		/**
		 * @brief Tests whether no value satisfies the predicate.
		 * @tparam F Const-invocable predicate accepting const T&.
		 * @param predicate Test applied until the first true result.
		 * @return Single emitting false at the first match, or true at normal completion.
		 * An empty source produces true.
		 * @note A decisive result cancels upstream. Upstream errors and predicate exceptions
		 * before that result are forwarded as errors. Evaluation is deferred per subscription.
		 */
		template <concepts::PredicateFor<T> F>
		Single<bool> noneOf(F predicate) const;

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
		 * @brief Emits values through the first value satisfying the predicate.
		 * @tparam F Const-invocable predicate accepting const T&.
		 * @param predicate Stop condition evaluated before forwarding each value.
		 * @return Source including the matching value, then completing and cancelling upstream.
		 * @note Without a match, all values are forwarded. Predicate exceptions signal onError.
		 */
		template <concepts::PredicateFor<T> F>
		Observable takeUntil(F predicate) const;

		/**
		 * @brief Emits values while the predicate holds.
		 * @tparam F Const-invocable predicate accepting const T&.
		 * @param predicate Condition evaluated before forwarding each value.
		 * @return Source excluding the first nonmatching value, then completing and cancelling upstream.
		 * @note Predicate exceptions signal onError.
		 */
		template <concepts::PredicateFor<T> F>
		Observable takeWhile(F predicate) const;

		/**
		 * @brief Discards the first count values.
		 * @param count Number of initial values to discard; zero preserves all values.
		 * @return Source forwarding remaining values and preserving upstream termination.
		 */
		Observable drop(std::size_t count) const;

		/**
		 * @brief Discards values until the predicate first holds.
		 * @tparam F Const-invocable predicate accepting const T&.
		 * @param predicate Start condition, no longer evaluated after its first match.
		 * @return Source forwarding the first matching value and all subsequent values.
		 * @note Without a match, no values are forwarded. Predicate exceptions signal onError.
		 */
		template <concepts::PredicateFor<T> F>
		Observable dropUntil(F predicate) const;

		/**
		 * @brief Discards values while the predicate holds.
		 * @tparam F Const-invocable predicate accepting const T&.
		 * @param predicate Condition, no longer evaluated after its first false result.
		 * @return Source forwarding the first nonmatching value and all subsequent values.
		 * @note Predicate exceptions signal onError. State is independent for each subscription.
		 */
		template <concepts::PredicateFor<T> F>
		Observable dropWhile(F predicate) const;

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
		 * @brief Selects the value at a zero-based index.
		 * @param index Number of values to skip before selecting one.
		 * @return Maybe containing the selected value, or empty if the source completes first.
		 * @note Selection cancels upstream immediately. Errors before selection are preserved.
		 */
		Maybe<T> elementAt(std::size_t index) const;

		/**
		 * @brief Selects the first value and cancels upstream immediately.
		 * @return Maybe containing the first value, or empty for an empty source.
		 * @note Errors before the first value are preserved.
		 */
		Maybe<T> first() const;

		/**
		 * @brief Selects the last value after normal completion.
		 * @return Maybe containing the last value, or empty for an empty source.
		 * @note Retains one value while waiting for completion. An upstream error discards it.
		 * No value is emitted if upstream never completes. Cancellation is propagated upstream.
		 */
		Maybe<T> last() const;

		/**
		 * @brief Counts all values until normal completion.
		 * @return Single containing the number of values, or zero for an empty source.
		 * @note Upstream errors discard the partial count. Counter overflow signals
		 * std::overflow_error through onError. Cancellation is propagated upstream.
		 */
		Single<std::size_t> count() const;

		/**
		 * @brief Selects the greatest value after normal completion.
		 * @tparam Compare Const-invocable strict weak ordering, defaulting to std::less<>.
		 * @param compare Comparator stored by value, returning true when its first argument is smaller.
		 * @return Maybe containing the greatest value, or empty for an empty source.
		 * Equivalent values retain the first occurrence.
		 * @note Retains one value. Upstream errors and comparator exceptions discard
		 * the partial result and signal onError. Cancellation is propagated upstream.
		 */
		template <typename Compare = std::less<>>
			requires std::strict_weak_order<const Compare &, const T &, const T &>
		Maybe<T> max(Compare compare = {}) const;

		/**
		 * @brief Selects the smallest value after normal completion.
		 * @tparam Compare Const-invocable strict weak ordering, defaulting to std::less<>.
		 * @param compare Comparator stored by value, returning true when its first argument is smaller.
		 * @return Maybe containing the smallest value, or empty for an empty source.
		 * Equivalent values retain the first occurrence.
		 * @note Retains one value. Upstream errors and comparator exceptions discard
		 * the partial result and signal onError. Cancellation is propagated upstream.
		 */
		template <typename Compare = std::less<>>
			requires std::strict_weak_order<const Compare &, const T &, const T &>
		Maybe<T> min(Compare compare = {}) const;

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
