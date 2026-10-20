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

	private:
		explicit Completable(Observable<std::monostate> source);
		rpp::dynamic_observable<std::monostate> source_;
		template <detail::Value>
		friend class Observable;
	};
} // namespace rppw

// rppw
#include <rppw/Completable.inl>
