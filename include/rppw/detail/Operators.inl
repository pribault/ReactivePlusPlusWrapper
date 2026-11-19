#pragma once

// stl
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// rpp
#include <rpp/rpp.hpp>

// rppw
#include <rppw/detail/Fwd.h>

namespace rppw::detail
{
	template <Value T, typename F>
	rpp::dynamic_observable<T> deferred(F factory)
	{
		return rpp::source::create<T>(
				   [factory = std::move(factory)](rpp::dynamic_observer<T> observer)
				   {
					   try
					   {
						   std::invoke(factory).subscribe(observer);
					   }
					   catch (...)
					   {
						   observer.on_error(std::current_exception());
					   }
				   })
			.as_dynamic();
	}

	/**
	 * @brief Subscription state of Observable::merge(sources, maxConcurrency).
	 * @tparam T Emitted value type.
	 * @details Notifications are serialized without blocking: the caller that finds no delivery
	 * in progress delivers directly; notifications raised meanwhile, from any thread, are queued
	 * and delivered in order by that caller before it returns. Slots freed during a delivery are
	 * refilled once it ends, so refilled synchronous sources are delivered as they emit rather
	 * than buffered. Refills raised while refilling are looped over instead of nested.
	 * RPP notifications are noexcept.
	 */
	template <Value T>
	class BoundedMerge : public std::enable_shared_from_this<BoundedMerge<T>>
	{
	public:
		using Sources = std::vector<rpp::dynamic_observable<T>>;

		BoundedMerge(rpp::dynamic_observer<T> observer, std::shared_ptr<const Sources> sources)
			: observer_{std::move(observer)}
			, sources_{std::move(sources)}
			, remaining_{sources_->size()}
		{
		}

		void start(std::size_t maxConcurrency)
		{
			observer_.set_upstream(disposables_);
			if (remaining_ == 0)
			{
				observer_.on_completed();
				return;
			}
			refills_.store(std::min(maxConcurrency, remaining_), std::memory_order_relaxed);
			subscribeRefills();
		}

	private:
		// Indices: value, error, completion of the source subscribed with this disposable.
		using Event = std::variant<T, std::exception_ptr, rpp::composite_disposable_wrapper>;

		// refillWip_ counts the refill loop in progress plus requests that arrived meanwhile.
		void subscribeRefills()
		{
			if (refillWip_.fetch_add(1, std::memory_order_acq_rel) != 0)
				return;
			std::size_t missed = 1;
			do
			{
				for (auto count = refills_.exchange(0, std::memory_order_relaxed); count != 0; --count)
					subscribeNext();
				missed = refillWip_.fetch_sub(missed, std::memory_order_acq_rel) - missed;
			} while (missed != 0);
		}

		void subscribeNext()
		{
			auto index = next_.load(std::memory_order_relaxed);
			do
			{
				if (index >= sources_->size() || disposables_.is_disposed())
					return;
			} while (!next_.compare_exchange_weak(index, index + 1, std::memory_order_relaxed));

			const auto child = rpp::composite_disposable_wrapper::make();
			disposables_.add(child);
			const auto self = this->shared_from_this();
			try
			{
				(*sources_)[index].subscribe(
					child,
					[self](const T &value)
					{
						self->emit(
							[&]
							{
								self->observer_.on_next(value);
							},
							[&]
							{
								return Event{std::in_place_index<0>, value};
							});
					},
					[self](const std::exception_ptr &error)
					{
						self->fail(error);
					},
					[self, child]
					{
						// User cleanup may cancel the merge; run it before the slot is refilled.
						child.dispose();
						self->emit(Event{std::in_place_index<2>, child});
					});
			}
			catch (...)
			{
				fail(std::current_exception());
			}
		}

		void fail(const std::exception_ptr &error)
		{
			// Stops admissions even while the error waits for delivery.
			next_.store(sources_->size(), std::memory_order_relaxed);
			emit(Event{std::in_place_index<1>, error});
		}

		void handle(Event &event)
		{
			switch (event.index())
			{
			case 0:
				observer_.on_next(std::move(std::get<0>(event)));
				break;
			case 1:
				observer_.on_error(std::get<1>(event));
				disposables_.dispose();
				break;
			default:
				disposables_.remove(std::get<2>(event));
				if (--remaining_ == 0)
					observer_.on_completed();
				else
					refills_.fetch_add(1, std::memory_order_relaxed);
			}
		}

		void emit(Event event)
		{
			emit(
				[&]
				{
					handle(event);
				},
				[&]
				{
					return std::move(event);
				});
		}

		// wip_ counts the delivery in progress plus queued events not yet accounted.
		template <typename Deliver, typename Make>
		void emit(Deliver &&deliver, Make &&make)
		{
			std::size_t missed = 1;
			if (std::size_t idle = 0; wip_.compare_exchange_strong(idle, 1, std::memory_order_acquire))
			{
				std::forward<Deliver>(deliver)();
				missed = wip_.fetch_sub(1, std::memory_order_acq_rel) - 1;
			}
			else
			{
				{
					const std::lock_guard lock{mutex_};
					queue_.push_back(std::forward<Make>(make)());
				}
				if (wip_.fetch_add(1, std::memory_order_acq_rel) != 0)
					return;
			}
			for (; missed != 0; missed = wip_.fetch_sub(missed, std::memory_order_acq_rel) - missed)
				while (auto event = pop())
					handle(*event);
			if (refills_.load(std::memory_order_relaxed) != 0)
				subscribeRefills();
		}

		std::optional<Event> pop()
		{
			const std::lock_guard lock{mutex_};
			if (queue_.empty())
				return std::nullopt;
			std::optional<Event> event{std::move(queue_.front())};
			queue_.pop_front();
			return event;
		}

		rpp::dynamic_observer<T>		  observer_;
		std::shared_ptr<const Sources>	  sources_;
		rpp::composite_disposable_wrapper disposables_ = rpp::composite_disposable_wrapper::make();
		std::atomic_size_t				  next_{0};
		std::size_t						  remaining_; // Accessed only before subscribing and while delivering.
		std::atomic_size_t				  wip_{0};
		std::mutex						  mutex_;
		std::deque<Event>				  queue_;
		std::atomic_size_t				  refills_{0};
		std::atomic_size_t				  refillWip_{0};
	};

	template <Value T>
	rpp::dynamic_observable<T> cardinality(rpp::dynamic_observable<T> source, bool required, const char *operation)
	{
		return (source |
				rpp::operators::reduce(std::optional<T>{},
									   [operation](std::optional<T> value, const T &next)
									   {
										   if (value)
											   throw std::out_of_range{std::string{operation} + ": expected at most one value"};
										   return std::optional<T>{next};
									   }) |
				rpp::operators::flat_map(
					[required, operation](const std::optional<T> &value) -> rpp::dynamic_observable<T>
					{
						if (value)
							return rpp::source::just(*value).as_dynamic();
						if (required)
							return rpp::source::error<T>(
									   std::make_exception_ptr(std::out_of_range{std::string{operation} + ": expected one value, received zero"}))
								.as_dynamic();
						return rpp::source::empty<T>().as_dynamic();
					}))
			.as_dynamic();
	}
} // namespace rppw::detail
