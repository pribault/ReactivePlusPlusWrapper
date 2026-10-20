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
	Completable Completable::subscribeOn(S scheduler) const
	{
		return Completable{Observable<std::monostate>{source_}.subscribeOn(std::move(scheduler))};
	}

	template <rpp::schedulers::constraint::scheduler S>
	Completable Completable::observeOn(S scheduler) const
	{
		return Completable{Observable<std::monostate>{source_}.observeOn(std::move(scheduler))};
	}
} // namespace rppw
