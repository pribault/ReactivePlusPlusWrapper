#pragma once

// rpp
#include <rpp/disposables/composite_disposable.hpp>

namespace rppw
{
	/**
	 * @brief Copyable handle sharing the cancellation state of an RPP subscription.
	 * @details Copies share cancellation; destroying a handle does not explicitly
	 * request cancellation. An empty handle is considered disposed.
	 * @note dispose() and isDisposed() delegate to thread-safe RPP operations.
	 * This does not permit concurrent assignment to the same handle object.
	 *
	 * @include rppw/Disposable.h
	 */
	class Disposable
	{
	public:
		/**
		 * @brief Constructs an empty handle, already considered disposed.
		 */
		Disposable();

		/**
		 * @brief Adapts an RPP composite disposable by sharing its state.
		 * @param disposable RPP handle transferred into this object; an empty handle is accepted.
		 */
		explicit Disposable(rpp::composite_disposable_wrapper disposable);

		/**
		 * @brief Cancels the subscription and propagates cancellation to upstream resources.
		 * @note Idempotent operation with no effect on an empty or already disposed handle.
		 * A callback already in progress may finish; running threads and callables are not interrupted.
		 */
		void dispose() const noexcept;

		/**
		 * @brief Checks whether the handle is empty, cancelled or terminated.
		 * @return true if the subscription is no longer active, false otherwise.
		 */
		[[nodiscard]] bool isDisposed() const noexcept;

	private:
		rpp::composite_disposable_wrapper disposable_;
	};
} // namespace rppw
