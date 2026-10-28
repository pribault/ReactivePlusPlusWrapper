#pragma once

// stl
#include <memory>

// rppw
#include <rppw/Disposable.h>

namespace rppw
{
	/**
	 * @brief Copyable cancellation scope that disposes its attached children together.
	 * @details Copies share the scope. Destruction releases ownership without requesting
	 * cancellation of children. A moved-from scope is empty and considered disposed.
	 * @note add(), remove(), dispose() and isDisposed() use thread-safe RPP operations.
	 * Concurrent assignment or destruction of the same handle object is not supported.
	 * Concurrent remove() and dispose() may cancel the child if disposal wins the race.
	 *
	 * @include rppw/CompositeDisposable.h
	 */
	class CompositeDisposable
	{
	public:
		/**
		 * @brief Constructs a fresh, active cancellation scope.
		 */
		CompositeDisposable();

		/**
		 * @brief Retains a child for cancellation when the scope is disposed.
		 * @param disposable Child handle; empty or already disposed children are ignored.
		 * @note An active child added to a disposed or empty scope is disposed immediately.
		 * Allocation failures propagate without disposing the scope.
		 */
		void add(const Disposable &disposable) const;

		/**
		 * @brief Detaches all registrations of a child without cancelling it.
		 * @param disposable Child handle or a copy sharing the same cancellation state.
		 * @note A missing child or an empty or disposed scope has no effect.
		 * Removing completed children releases their retained handles.
		 */
		void remove(const Disposable &disposable) const;

		/**
		 * @brief Cancels all attached children and permanently closes the scope.
		 * @note Idempotent. Running callbacks may finish; work is not interrupted.
		 */
		void dispose() const noexcept;

		/**
		 * @brief Checks whether the scope is disposed or empty.
		 * @return true if the scope is closed or moved from, false if it is active.
		 */
		[[nodiscard]] bool isDisposed() const noexcept;

	private:
		std::shared_ptr<rpp::composite_disposable> disposable_;
	};
} // namespace rppw
