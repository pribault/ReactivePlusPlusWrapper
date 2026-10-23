#pragma once

// stl
#include <concepts>
#include <cstddef>
#include <exception>
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
	 * @brief Cold source whose success produces exactly one value.
	 * @tparam T Unqualified value type satisfying std::copyable.
	 * @details Each subscription receives success or an error, unless cancelled
	 * or the source never terminates. Success is terminal, with no completion callback.
	 * Operators compose a new chain; producer exceptions are forwarded through
	 * std::exception_ptr. Without a scheduler, synchronous sources run during
	 * subscribe() on the calling thread.
	 * @warning Reference captures must outlive processing.
	 * The caller must synchronize data shared between subscriptions.
	 *
	 * @include rppw/Single.h
	 */
	template <detail::Value T>
	class Single
	{
	public:
		/** @brief Type of the success value. */
		using value_type = T;

		/**
		 * @brief Stores a value to forward for each subscription.
		 * @param value Value owned by the source.
		 * @return Single that succeeds with value.
		 */
		static Single just(T value);

		/**
		 * @brief Computes a value independently for each subscription.
		 * @tparam F Const-invocable callable returning exactly T.
		 * @param callable Computation stored by value, not executed during construction.
		 * @return Deferred source forwarding the callable's result or exception.
		 */
		template <concepts::FactoryFor<T> F>
		static Single fromCallable(F callable);

		/**
		 * @brief Creates a source that only forwards the supplied error.
		 * @param error Non-null exception retained by the source.
		 * @return Failing Single.
		 * @throws std::invalid_argument If error is null.
		 */
		static Single error(std::exception_ptr error);

		/**
		 * @brief Captures an error with std::make_exception_ptr and creates a failing source.
		 * @tparam E Copy-constructible type satisfying concepts::ExceptionValue.
		 * @param error Error copied into the exception pointer retained by the source.
		 * @return Single that forwards the captured error for each subscription.
		 * @note Capture happens immediately; only notification is deferred.
		 */
		template <concepts::ExceptionValue E>
		static Single error(E error);

		/**
		 * @brief Invokes a Single factory for each subscription.
		 * @tparam F Const-invocable callable returning `Single<T>`.
		 * @param factory Factory stored by value.
		 * @return Deferred source; factory exceptions are forwarded to onError.
		 */
		template <concepts::SourceFactoryFor<T, rppw::Single> F>
		static Single defer(F factory);

		/**
		 * @brief Starts processing, ignoring the success value.
		 * @return Shared cancellation handle; upstream is cancelled after success.
		 * @note Unhandled errors are logged to stderr without stopping the program.
		 * Destroying the handle does not cancel the subscription.
		 */
		Disposable subscribe() const;

		/**
		 * @brief Starts processing and receives a single success or an error.
		 * @tparam N Type of the callback receiving T.
		 * @tparam E Type of the callback receiving std::exception_ptr.
		 * @param onSuccess Terminal success callback.
		 * @param onError Terminal error callback.
		 * @return Shared cancellation handle; upstream is cancelled after success.
		 * @pre Callbacks must be const-invocable and must not throw.
		 * @warning An exception escaping a callback invokes std::terminate.
		 * @note Callbacks are retained throughout processing. Destroying the handle
		 * does not cancel the subscription; cancellation does not interrupt a callback already in progress.
		 */
		template <concepts::CallbackFor<T> N, concepts::CallbackFor<std::exception_ptr &> E>
		Disposable subscribe(N onSuccess, E onError) const;

		/**
		 * @brief Inspects an error before forwarding it unchanged.
		 * @tparam F Const-invocable callback accepting const std::exception_ptr&.
		 * @param action Callback stored by value and invoked on each subscription's error.
		 * @return Single preserving success and errors.
		 * @pre The callback must not throw; an escaping exception invokes std::terminate.
		 */
		template <concepts::CallbackFor<const std::exception_ptr &> F>
		Single doOnError(F action) const;

		/**
		 * @brief Executes an action before forwarding successful termination.
		 * @tparam F Const-invocable no-argument callback.
		 * @param action Callback stored by value; not invoked on error or cancellation.
		 * @return Single preserving success and errors, without adding a completion signal.
		 * @pre The callback must not throw; an escaping exception invokes std::terminate.
		 */
		template <concepts::CallbackFor F>
		Single doOnComplete(F action) const;

		/**
		 * @brief Inspects the success value before forwarding it unchanged.
		 * @tparam F Const-invocable callback accepting const T&.
		 * @param action Callback stored by value; not invoked on error or cancellation.
		 * @return Single preserving success and errors.
		 * @pre The callback must not throw; an escaping exception invokes std::terminate.
		 */
		template <concepts::CallbackFor<const T &> F>
		Single doOnSuccess(F action) const;

		/**
		 * @brief Transforms the success value.
		 * @tparam F Const-invocable callable producing an admissible, non-reference value.
		 * @param function Value transformation.
		 * @return `Single<U>`, where U is the decayed result type.
		 * @note Exceptions from function are forwarded to onError.
		 */
		template <concepts::MapperFor<T> F>
		auto map(F function) const;

		/**
		 * @brief Chains a Single computed from the success value.
		 * @tparam F Const-invocable callable returning `Single<U>`.
		 * @param function Factory invoked on upstream success.
		 * @return Single carrying the second source's result or error.
		 * @note An upstream error prevents invocation; exceptions from function are propagated.
		 */
		template <concepts::FlatMapperFor<T, rppw::Single> F>
		auto flatMap(F function) const;

		/**
		 * @brief Converts a rejected success into an absent value.
		 * @tparam F Const-invocable predicate accepting const T&.
		 * @param predicate Test determining whether to keep the value.
		 * @return Present Maybe if accepted, empty otherwise; errors are preserved.
		 * @note A predicate exception is forwarded to onError.
		 */
		template <concepts::PredicateFor<T> F>
		Maybe<T> filter(F predicate) const;

		/**
		 * @brief Delays success and errors on the supplied scheduler.
		 * @tparam S ReactivePlusPlus scheduler type.
		 * @param duration Nonnegative notification delay.
		 * @param scheduler Delivery context; upstream subscription is not delayed.
		 * @return Single preserving success and errors with delayed delivery.
		 * @throws std::invalid_argument If duration is negative.
		 * @note Cancellation discards queued notifications and cancels upstream.
		 * A blocking scheduler can block the subscribing thread.
		 */
		template <rpp::schedulers::constraint::scheduler S>
		Single delay(rpp::schedulers::duration duration, S scheduler) const;

		/**
		 * @brief Repeats successful subscriptions up to count times in total.
		 * @param count Total subscription count; zero completes without subscribing.
		 * @return Observable emitting each success and stopping on the first error.
		 * @note Counts reset per subscriber; cancellation prevents further subscriptions.
		 * No scheduler is added.
		 */
		Observable<T> repeat(std::size_t count) const;

		/**
		 * @brief Repeats successful subscriptions until cancellation or error.
		 * @return Observable emitting each success.
		 * @warning Synchronous repetition can occupy the subscribing thread indefinitely.
		 * No scheduler is added.
		 */
		Observable<T> repeat() const;

		/**
		 * @brief Resubscribes after an error up to count additional attempts.
		 * @param count Maximum retries after the initial attempt; zero does not retry.
		 * @return Single succeeding on recovery or forwarding the last error on exhaustion.
		 * @note Counts reset per subscriber; cancellation prevents further attempts.
		 * No scheduler or backoff is added.
		 */
		Single retry(std::size_t count) const;

		/**
		 * @brief Retries errors until success or cancellation.
		 * @return Single preserving the eventual success.
		 * @warning Synchronous errors can occupy the subscribing thread indefinitely.
		 * No scheduler or backoff is added.
		 */
		Single retry() const;

		/**
		 * @brief Schedules the upstream subscription without changing the reactive type.
		 * @tparam S ReactivePlusPlus scheduler type.
		 * @param scheduler Subscription context, not necessarily the notification context.
		 * @return Single with a scheduled subscription.
		 */
		template <rpp::schedulers::constraint::scheduler S>
		Single subscribeOn(S scheduler) const;

		/**
		 * @brief Schedules success and error delivery.
		 * @tparam S ReactivePlusPlus scheduler type.
		 * @param scheduler Downstream delivery context.
		 * @return Single whose notifications are scheduled on scheduler.
		 */
		template <rpp::schedulers::constraint::scheduler S>
		Single observeOn(S scheduler) const;

		/**
		 * @brief Exposes success as a one-value stream followed by completion.
		 * @return Observable preserving the value and errors.
		 */
		Observable<T> toObservable() const;

		/**
		 * @brief Exposes success as a present Maybe.
		 * @return Maybe preserving the value and errors without introducing an empty case.
		 */
		Maybe<T> toMaybe() const;

		/**
		 * @brief Discards the success value while waiting for termination.
		 * @return Completable that completes after success or forwards the error.
		 */
		Completable ignoreElement() const;

	private:
		explicit Single(Observable<T> source)
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
#include <rppw/Single.inl>
