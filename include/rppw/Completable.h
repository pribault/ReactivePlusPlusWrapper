#pragma once

// stl
#include <concepts>
#include <exception>
#include <type_traits>
#include <variant>

// rpp
#include <rpp/rpp.hpp>

// rppw
#include <rppw/Concepts.h>
#include <rppw/Disposable.h>

namespace rppw
{
	/**
	 * @brief Cold source that completes without a value or signals an error.
	 * @details Each subscription executes its own chain. Actions are deferred until
	 * subscription; their exceptions are forwarded through std::exception_ptr.
	 * Without a scheduler, synchronous actions run on the thread calling subscribe().
	 * @warning Reference captures must outlive processing.
	 * The caller must synchronize data shared between subscriptions.
	 *
	 * @include rppw/Completable.h
	 */
	class Completable
	{
	public:
		/** @brief Technical type without a domain value, used for generic composition. */
		using value_type = std::monostate;

		/**
		 * @brief Creates a source that completes without executing an action.
		 * @return Completable that completes normally for each subscription.
		 */
		static Completable complete();

		/**
		 * @brief Subscribes to Completables sequentially in range order.
		 * @tparam R Input range of Completable.
		 * @param sources Sources copied into owned storage immediately.
		 * @return Completable completing after all sources, including an empty range.
		 * @note Each subscription restarts the chain. An error or cancellation prevents
		 * subscription to remaining sources. Range traversal and allocation exceptions
		 * are synchronous.
		 */
		template <concepts::RangeOf<Completable> R>
		static Completable concat(R &&sources);

		/**
		 * @brief Subscribes to Completables without waiting for preceding completions.
		 * @tparam R Input range of Completable.
		 * @param sources Sources copied into owned storage immediately.
		 * @return Completable completing after all sources, including an empty range.
		 * @note Each subscription restarts the sources. An error or cancellation cancels
		 * all active sources. No scheduler is added. Range traversal and allocation
		 * exceptions are synchronous.
		 * @warning No concurrency limit is imposed.
		 */
		template <concepts::RangeOf<Completable> R>
		static Completable merge(R &&sources);

		/**
		 * @brief Creates a source that only forwards the supplied error.
		 * @param error Non-null exception retained by the source.
		 * @return Failing Completable.
		 * @throws std::invalid_argument If error is null.
		 */
		static Completable error(std::exception_ptr error);

		/**
		 * @brief Captures an error with std::make_exception_ptr and creates a failing source.
		 * @tparam E Copy-constructible type satisfying concepts::ExceptionValue.
		 * @param error Error copied into the exception pointer retained by the source.
		 * @return Completable that forwards the captured error for each subscription.
		 * @note Capture happens immediately; only notification is deferred.
		 */
		template <concepts::ExceptionValue E>
		static Completable error(E error);

		/**
		 * @brief Executes an action and completes independently for each subscription.
		 * @tparam F Const-invocable callable returning void.
		 * @param action Action stored by value, not executed during construction.
		 * @return Completable forwarding action exceptions to onError.
		 * @note Cancellation does not interrupt an action already in progress.
		 */
		template <concepts::FactoryFor<void> F>
		static Completable fromAction(F action);

		/**
		 * @brief Creates the effective source through a factory for each subscription.
		 * @tparam F Const-invocable callable returning Completable.
		 * @param factory Factory stored by value.
		 * @return Deferred source; factory exceptions are forwarded to onError.
		 */
		template <concepts::FactoryFor<Completable> F>
		static Completable defer(F factory);

		/**
		 * @brief Starts processing with terminal completion or an error.
		 * @tparam C Type of the no-argument completion callback.
		 * @tparam E Type of the callback receiving std::exception_ptr.
		 * @param onComplete Terminal success callback.
		 * @param onError Terminal error callback.
		 * @return Shared cancellation handle, already disposed if the source terminates synchronously.
		 * @pre Callbacks must be const-invocable and must not throw.
		 * @warning An exception escaping a callback invokes std::terminate.
		 * @note Callbacks are retained throughout processing. Destroying the handle
		 * does not cancel the subscription; cancellation does not interrupt a callback already in progress.
		 */
		template <concepts::CallbackFor C, concepts::CallbackFor<std::exception_ptr &> E>
		Disposable subscribe(C onComplete, E onError) const;

		/**
		 * @brief Inspects an error before forwarding it unchanged.
		 * @tparam F Const-invocable callback accepting const std::exception_ptr&.
		 * @param action Callback stored by value and invoked on each subscription's error.
		 * @return Completable preserving completion and errors.
		 * @pre The callback must not throw; an escaping exception invokes std::terminate.
		 */
		template <concepts::CallbackFor<const std::exception_ptr &> F>
		Completable doOnError(F action) const;

		/**
		 * @brief Executes an action before forwarding normal completion.
		 * @tparam F Const-invocable no-argument callback.
		 * @param action Callback stored by value; not invoked on error or cancellation.
		 * @return Completable preserving completion and errors.
		 * @pre The callback must not throw; an escaping exception invokes std::terminate.
		 */
		template <concepts::CallbackFor F>
		Completable doOnComplete(F action) const;

		/**
		 * @brief Subscribes to the second source only after this source completes normally.
		 * @tparam R `Observable<U>`, `Single<U>`, `Maybe<U>` or Completable.
		 * @param next Second source stored by value.
		 * @return Chain of type R forwarding notifications from next.
		 * @note An error or cancellation of the first stage prevents subscription
		 * to next. Cancellation is propagated to the active stage.
		 */
		template <concepts::ReactiveSource R>
		R andThen(R next) const;

		/**
		 * @brief Schedules the upstream subscription.
		 * @tparam S ReactivePlusPlus scheduler type.
		 * @param scheduler Subscription context, not necessarily the notification context.
		 * @return Completable with a scheduled subscription.
		 */
		template <rpp::schedulers::constraint::scheduler S>
		Completable subscribeOn(S scheduler) const;

		/**
		 * @brief Schedules completion and error delivery.
		 * @tparam S ReactivePlusPlus scheduler type.
		 * @param scheduler Downstream delivery context.
		 * @return Completable whose notifications are scheduled on scheduler.
		 */
		template <rpp::schedulers::constraint::scheduler S>
		Completable observeOn(S scheduler) const;

		/**
		 * @brief Exposes completion or an error as a stream without values.
		 * @tparam T Observable value type, defaulting to std::monostate; no T is constructed.
		 * @return Cold Observable forwarding termination without emitting a value.
		 * @note Each subscription executes this source anew. Cancellation is propagated upstream.
		 */
		template <detail::Value T = std::monostate>
		Observable<T> toObservable() const;

		/**
		 * @brief Exposes normal completion as an empty Maybe, preserving errors.
		 * @tparam T Maybe value type, defaulting to std::monostate; no T is constructed.
		 * @return Cold Maybe that completes empty or forwards this source's error.
		 * @note Each subscription executes this source anew. Cancellation is propagated upstream.
		 */
		template <detail::Value T = std::monostate>
		Maybe<T> toMaybe() const;

	private:
		explicit Completable(Observable<std::monostate> source);
		rpp::dynamic_observable<std::monostate> source_;
		template <detail::Value>
		friend class Observable;
	};
} // namespace rppw

// rppw
#include <rppw/Completable.inl>
