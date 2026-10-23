#pragma once

// stl
#include <concepts>
#include <cstddef>
#include <exception>
#include <optional>
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
	 * @brief Cold source that succeeds with a value, completes empty or signals an error.
	 * @tparam T Unqualified value type satisfying std::copyable.
	 * @details Success and empty completion are distinct terminal outcomes:
	 * onSuccess is never followed by onComplete. Each subscription executes its own chain.
	 * Operators do not modify the source; their exceptions are forwarded to onError.
	 * Without a scheduler, synchronous sources run on the thread calling subscribe().
	 * @warning Reference captures must outlive processing.
	 * The caller must synchronize data shared between subscriptions.
	 *
	 * @include rppw/Maybe.h
	 */
	template <detail::Value T>
	class Maybe
	{
	public:
		/** @brief Type of the optional value. */
		using value_type = T;

		/**
		 * @brief Stores a value to emit as success for each subscription.
		 * @param value Value owned by the source.
		 * @return Present Maybe.
		 */
		static Maybe just(T value);

		/**
		 * @brief Invokes a producer with an RPP observer for each subscription.
		 * @param producer Const-invocable callable receiving rpp::dynamic_observer<T>.
		 * The observer may be retained for asynchronous delivery.
		 * @return Cold Maybe forwarding the first value as terminal success,
		 * empty completion, or an error, including producer exceptions.
		 * @pre Notifications must be serialized; errors must be non-null.
		 * @note Stop after termination or disposal. Use observer.set_upstream()
		 * for cleanup; captured resources must outlive asynchronous work.
		 */
		template <concepts::CallbackFor<rpp::dynamic_observer<T>> F>
		static Maybe create(F producer);

		/**
		 * @brief Creates a source that completes without a value.
		 * @return Maybe invoking only onComplete at subscription time.
		 */
		static Maybe empty();

		/**
		 * @brief Evaluates presence or absence for each subscription.
		 * @tparam F Const-invocable callable returning `std::optional<T>`.
		 * @param callable Computation stored by value and invoked only at subscription time.
		 * @return Present Maybe if the optional has a value, empty otherwise.
		 * @note Callable exceptions are forwarded to onError.
		 */
		template <concepts::FactoryFor<std::optional<T>> F>
		static Maybe fromCallable(F callable);

		/**
		 * @brief Creates a source that only forwards the supplied error.
		 * @param error Non-null exception retained by the source.
		 * @return Failing Maybe.
		 * @throws std::invalid_argument If error is null.
		 */
		static Maybe error(std::exception_ptr error);

		/**
		 * @brief Captures an error with std::make_exception_ptr and creates a failing source.
		 * @tparam E Copy-constructible type satisfying concepts::ExceptionValue.
		 * @param error Error copied into the exception pointer retained by the source.
		 * @return Maybe that forwards the captured error for each subscription.
		 * @note Capture happens immediately; only notification is deferred.
		 */
		template <concepts::ExceptionValue E>
		static Maybe error(E error);

		/**
		 * @brief Invokes a Maybe factory for each subscription.
		 * @tparam F Const-invocable callable returning `Maybe<T>`.
		 * @param factory Factory stored by value.
		 * @return Deferred source; factory exceptions are forwarded to onError.
		 */
		template <concepts::SourceFactoryFor<T, rppw::Maybe> F>
		static Maybe defer(F factory);

		/**
		 * @brief Starts processing, ignoring success and empty completion.
		 * @return Shared cancellation handle, disposed after termination.
		 * @note Unhandled errors are logged to stderr without stopping the program.
		 * Destroying the handle does not cancel the subscription.
		 */
		Disposable subscribe() const;

		/**
		 * @brief Starts processing with three mutually exclusive terminal outcomes.
		 * @tparam N Type of the callback receiving T.
		 * @tparam E Type of the callback receiving std::exception_ptr.
		 * @tparam C Type of the no-argument callback for an absent value.
		 * @param onSuccess Terminal callback when a value is received.
		 * @param onError Terminal error callback.
		 * @param onComplete Terminal callback when the source is empty.
		 * @return Shared cancellation handle, disposed after success, error or empty completion.
		 * @pre Callbacks must be const-invocable and must not throw.
		 * @warning An exception escaping a callback invokes std::terminate.
		 * @note Callbacks are retained throughout processing. Destroying the handle
		 * does not cancel the subscription. Cancellation does not interrupt a callback already in progress.
		 */
		template <concepts::CallbackFor<T> N, concepts::CallbackFor<std::exception_ptr &> E, concepts::CallbackFor C>
		Disposable subscribe(N onSuccess, E onError, C onComplete) const;

		/**
		 * @brief Inspects an error before forwarding it unchanged.
		 * @tparam F Const-invocable callback accepting const std::exception_ptr&.
		 * @param action Callback stored by value and invoked on each subscription's error.
		 * @return Maybe preserving all terminal outcomes.
		 * @pre The callback must not throw; an escaping exception invokes std::terminate.
		 */
		template <concepts::CallbackFor<const std::exception_ptr &> F>
		Maybe doOnError(F action) const;

		/**
		 * @brief Executes an action before success or empty completion.
		 * @tparam F Const-invocable no-argument callback.
		 * @param action Callback stored by value and invoked once per normal termination.
		 * Not invoked on error or cancellation.
		 * @return Maybe preserving the distinction between success and empty completion.
		 * @pre The callback must not throw; an escaping exception invokes std::terminate.
		 */
		template <concepts::CallbackFor F>
		Maybe doOnComplete(F action) const;

		/**
		 * @brief Inspects a present value before forwarding success unchanged.
		 * @tparam F Const-invocable callback accepting const T&.
		 * @param action Callback stored by value; not invoked when empty, on error or cancellation.
		 * @return Maybe preserving all terminal outcomes.
		 * @pre The callback must not throw; an escaping exception invokes std::terminate.
		 */
		template <concepts::CallbackFor<const T &> F>
		Maybe doOnSuccess(F action) const;

		/**
		 * @brief Transforms a present value without invoking the function when empty.
		 * @tparam F Const-invocable callable producing an admissible, non-reference value.
		 * @param function Value transformation.
		 * @return `Maybe<U>`, where U is the decayed result type.
		 * @note Exceptions from function are forwarded to onError.
		 */
		template <concepts::MapperFor<T> F>
		auto map(F function) const;

		/**
		 * @brief Chains a Maybe from a present value.
		 * @tparam F Const-invocable callable returning `Maybe<U>`.
		 * @param function Second-source factory, not invoked on empty completion or error.
		 * @return Maybe carrying the second source's result or upstream termination.
		 * @note Exceptions from function are forwarded to onError.
		 */
		template <concepts::FlatMapperFor<T, rppw::Maybe> F>
		auto flatMap(F function) const;

		/**
		 * @brief Keeps an accepted value or completes empty.
		 * @tparam F Const-invocable predicate accepting const T&.
		 * @param predicate Retention test, not invoked for an empty source.
		 * @return Filtered Maybe; upstream and predicate errors are propagated.
		 */
		template <concepts::PredicateFor<T> F>
		Maybe filter(F predicate) const;

		/**
		 * @brief Delays success, empty completion and errors on the supplied scheduler.
		 * @tparam S ReactivePlusPlus scheduler type.
		 * @param duration Nonnegative notification delay.
		 * @param scheduler Delivery context; upstream subscription is not delayed.
		 * @return Maybe preserving all terminal outcomes with delayed delivery.
		 * @throws std::invalid_argument If duration is negative.
		 * @note Cancellation discards queued notifications and cancels upstream.
		 * A blocking scheduler can block the subscribing thread.
		 */
		template <rpp::schedulers::constraint::scheduler S>
		Maybe delay(rpp::schedulers::duration duration, S scheduler) const;

		/**
		 * @brief Repeats success or empty completion up to count total subscriptions.
		 * @param count Total subscription count; zero completes without subscribing.
		 * @return Observable emitting present values and stopping on the first error.
		 * @note Empty completions consume repetitions. Counts reset per subscriber;
		 * cancellation prevents further subscriptions. No scheduler is added.
		 */
		Observable<T> repeat(std::size_t count) const;

		/**
		 * @brief Repeats success or empty completion until cancellation or error.
		 * @return Observable emitting present values from every repetition.
		 * @warning Synchronous repetition, including empty sources, can occupy the
		 * subscribing thread indefinitely. No scheduler is added.
		 */
		Observable<T> repeat() const;

		/**
		 * @brief Resubscribes after an error up to count additional attempts.
		 * @param count Maximum retries after the initial attempt; zero does not retry.
		 * @return Maybe succeeding or completing empty on recovery, or forwarding the last error.
		 * @note Empty completion is not retried. Counts reset per subscriber;
		 * cancellation prevents further attempts. No scheduler or backoff is added.
		 */
		Maybe retry(std::size_t count) const;

		/**
		 * @brief Retries errors until success, empty completion or cancellation.
		 * @return Maybe preserving success or empty completion on recovery.
		 * @warning Synchronous errors can occupy the subscribing thread indefinitely.
		 * No scheduler or backoff is added.
		 */
		Maybe retry() const;

		/**
		 * @brief Schedules the upstream subscription.
		 * @tparam S ReactivePlusPlus scheduler type.
		 * @param scheduler Subscription context, not necessarily the notification context.
		 * @return Maybe with a scheduled subscription.
		 */
		template <rpp::schedulers::constraint::scheduler S>
		Maybe subscribeOn(S scheduler) const;

		/**
		 * @brief Schedules delivery of success, empty completion and errors.
		 * @tparam S ReactivePlusPlus scheduler type.
		 * @param scheduler Downstream delivery context.
		 * @return Maybe whose notifications are scheduled on scheduler.
		 */
		template <rpp::schedulers::constraint::scheduler S>
		Maybe observeOn(S scheduler) const;

		/**
		 * @brief Exposes the source as zero or one value followed by completion.
		 * @return Observable preserving values, absence and errors.
		 */
		Observable<T> toObservable() const;

		/**
		 * @brief Requires a value before completion.
		 * @return Single carrying the value, or std::out_of_range if the source is empty.
		 * @note Source errors are preserved; absence errors use the reactive error channel.
		 */
		Single<T> toSingle() const;

		/**
		 * @brief Replaces an absent value without replacing errors.
		 * @param value Fallback value owned by the chain.
		 * @return Single carrying the present value or value after empty completion.
		 */
		Single<T> defaultIfEmpty(T value) const;

		/**
		 * @brief Subscribes to a fallback only after empty completion.
		 * @param fallback Maybe stored by value and subscribed only when this source is empty.
		 * @return Maybe preserving this source's success or error, otherwise forwarding fallback.
		 * @note Errors do not trigger fallback. Each subscription has independent state.
		 * Cancellation is propagated to the active source and prevents a pending fallback subscription.
		 */
		Maybe switchIfEmpty(Maybe fallback) const;

		/**
		 * @brief Discards the optional value and preserves termination.
		 * @return Completable that completes on success or empty completion, or forwards the error.
		 */
		Completable ignoreElement() const;

	private:
		explicit Maybe(Observable<T> source)
			: source_{std::move(source)}
		{
		}
		Observable<T> source_;
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
#include <rppw/Maybe.inl>
