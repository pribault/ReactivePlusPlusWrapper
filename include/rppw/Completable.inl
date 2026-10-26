#pragma once

// stl
#include <functional>
#include <utility>

// rppw
#include <rppw/Maybe.h>
#include <rppw/Observable.h>
#include <rppw/Single.h>

namespace rppw
{
	template <concepts::CallbackFor<rpp::dynamic_observer<std::monostate>> F>
	Completable Completable::create(F producer)
	{
		return Completable{Observable<std::monostate>::create(std::move(producer))};
	}

	template <concepts::RangeOf<Completable> R>
	Completable Completable::concat(R &&sources)
	{
		const auto inner = Observable<Completable>::fromIterable(std::forward<R>(sources))
							   .map(
								   [](const Completable &source)
								   {
									   return source.source_;
								   });
		return Completable{Observable<std::monostate>{(inner.source_ | rpp::operators::concat()).as_dynamic()}};
	}

	template <concepts::RangeOf<Completable> R>
	Completable Completable::merge(R &&sources)
	{
		const auto inner = Observable<Completable>::fromIterable(std::forward<R>(sources))
							   .map(
								   [](const Completable &source)
								   {
									   return source.source_;
								   });
		return Completable{Observable<std::monostate>{(inner.source_ | rpp::operators::merge()).as_dynamic()}};
	}

	template <concepts::ExceptionValue E>
	Completable Completable::error(E error)
	{
		return Completable::error(std::make_exception_ptr(error));
	}

	template <concepts::ExceptionValue S>
		requires std::convertible_to<S, std::string_view>
	Completable Completable::error(S message)
	{
		return Completable{Observable<std::monostate>::error(std::string_view{message})};
	}

	template <concepts::FactoryFor<void> F>
	Completable Completable::fromAction(F action)
	{
		return defer(
			[action = std::move(action)]
			{
				std::invoke(action);
				return complete();
			});
	}

	template <concepts::FactoryFor<Completable> F>
	Completable Completable::defer(F factory)
	{
		return Completable{Observable<std::monostate>::defer(
			[factory = std::move(factory)]
			{
				return Observable<std::monostate>{std::invoke(factory).source_};
			})};
	}

	template <concepts::CallbackFor C, concepts::CallbackFor<std::exception_ptr &> E>
	Disposable Completable::subscribe(C onComplete, E onError) const
	{
		return Observable<std::monostate>{source_}.subscribe([](std::monostate) {}, std::move(onError), std::move(onComplete));
	}

	template <concepts::CallbackFor<const std::exception_ptr &> F>
	Completable Completable::doOnError(F action) const
	{
		return Completable{Observable<std::monostate>{source_}.doOnError(std::move(action))};
	}

	template <concepts::CallbackFor F>
	Completable Completable::doOnComplete(F action) const
	{
		return Completable{Observable<std::monostate>{source_}.doOnComplete(std::move(action))};
	}

	template <concepts::ReactiveSource R>
	R Completable::andThen(R next) const
	{
		using U = typename R::value_type;
		auto first = (source_ | rpp::operators::flat_map(
									[](std::monostate)
									{
										return rpp::source::empty<U>();
									}))
						 .as_dynamic();
		if constexpr (std::same_as<R, Observable<U>>)
			return R{rpp::source::concat(std::move(first), next.source_).as_dynamic()};
		else if constexpr (std::same_as<R, Completable>)
			return R{Observable<U>{rpp::source::concat(std::move(first), next.source_).as_dynamic()}};
		else
			return R{Observable<U>{rpp::source::concat(std::move(first), next.source_.source_).as_dynamic()}};
	}

	template <rpp::schedulers::constraint::scheduler S>
	Completable Completable::delay(rpp::schedulers::duration duration, S scheduler) const
	{
		return Completable{Observable<std::monostate>{source_}.delay(duration, std::move(scheduler))};
	}

	template <rpp::schedulers::constraint::scheduler S>
	Completable Completable::subscribeOn(S scheduler) const
	{
		return Completable{Observable<std::monostate>{source_}.subscribeOn(std::move(scheduler))};
	}

	template <rpp::schedulers::constraint::scheduler S>
	Completable Completable::observeOn(S scheduler) const
	{
		return Completable{Observable<std::monostate>{source_}.observeOn(std::move(scheduler))};
	}

	template <detail::Value T>
	Observable<T> Completable::toObservable() const
	{
		return andThen(Observable<T>::empty());
	}

	template <detail::Value T>
	Maybe<T> Completable::toMaybe() const
	{
		return Maybe<T>{toObservable<T>()};
	}
} // namespace rppw
