#pragma once

// stl
#include <functional>
#include <stdexcept>
#include <variant>
#include <vector>

// rppw
#include <rppw/Completable.h>
#include <rppw/Maybe.h>
#include <rppw/Single.h>
#include <rppw/detail/Operators.inl>

namespace rppw
{
	template <detail::Value T>
	Observable<T> Observable<T>::just(T value)
	{
		return Observable{rpp::source::just(std::move(value)).as_dynamic()};
	}

	template <detail::Value T>
	template <concepts::RangeOf<T> R>
	Observable<T> Observable<T>::fromIterable(R &&range)
	{
		std::vector<T> values;
		if constexpr (std::ranges::sized_range<R>)
			values.reserve(std::ranges::size(range));
		for (auto &&value : range)
			values.emplace_back(value);
		return Observable{rpp::source::from_iterable(std::move(values), rpp::schedulers::immediate{}).as_dynamic()};
	}

	template <detail::Value T>
	template <concepts::ObservableSourceRangeOf<T> R>
	Observable<T> Observable<T>::concat(R &&sources)
	{
		const auto inner = Observable<std::ranges::range_value_t<R>>::fromIterable(std::forward<R>(sources))
							   .map(
								   [](const auto &source)
								   {
									   if constexpr (std::same_as<std::remove_cvref_t<decltype(source)>, Observable<T>>)
										   return source.source_;
									   else
										   return source.toObservable().source_;
								   });
		return Observable{(inner.source_ | rpp::operators::concat()).as_dynamic()};
	}

	template <detail::Value T>
	template <concepts::ObservableSourceRangeOf<T> R>
	Observable<T> Observable<T>::merge(R &&sources)
	{
		const auto inner = Observable<std::ranges::range_value_t<R>>::fromIterable(std::forward<R>(sources))
							   .map(
								   [](const auto &source)
								   {
									   if constexpr (std::same_as<std::remove_cvref_t<decltype(source)>, Observable<T>>)
										   return source.source_;
									   else
										   return source.toObservable().source_;
								   });
		return Observable{(inner.source_ | rpp::operators::merge()).as_dynamic()};
	}

	template <detail::Value T>
	Observable<T> Observable<T>::empty()
	{
		return Observable{rpp::source::empty<T>().as_dynamic()};
	}

	template <detail::Value T>
	Observable<T> Observable<T>::error(std::exception_ptr error)
	{
		if (!error)
			throw std::invalid_argument{"Observable::error: null exception_ptr"};
		return Observable{rpp::source::error<T>(std::move(error)).as_dynamic()};
	}

	template <detail::Value T>
	template <concepts::ExceptionValue E>
	Observable<T> Observable<T>::error(E error)
	{
		return Observable::error(std::make_exception_ptr(error));
	}

	template <detail::Value T>
	template <concepts::SourceFactoryFor<T, rppw::Observable> F>
	Observable<T> Observable<T>::defer(F factory)
	{
		return Observable{detail::deferred<T>(
			[factory = std::move(factory)]
			{
				return std::invoke(factory).source_;
			})};
	}

	template <detail::Value T>
	template <concepts::CallbackFor<T> N, concepts::CallbackFor<std::exception_ptr &> E, concepts::CallbackFor C>
	Disposable Observable<T>::subscribe(N onNext, E onError, C onComplete) const
	{
		return Disposable{source_.subscribe_with_disposable(
			[onNext = std::move(onNext)](T value) noexcept
			{
				std::invoke(onNext, std::move(value));
			},
			[onError = std::move(onError)](std::exception_ptr error) noexcept
			{
				std::invoke(onError, error);
			},
			[onComplete = std::move(onComplete)]() noexcept
			{
				std::invoke(onComplete);
			})};
	}

	template <detail::Value T>
	template <concepts::MapperFor<T> F>
	auto Observable<T>::map(F function) const
	{
		using U = std::decay_t<std::invoke_result_t<const F &, T>>;
		return Observable<U>{(source_ | rpp::operators::map(std::move(function))).as_dynamic()};
	}

	template <detail::Value T>
	template <concepts::PredicateFor<T> F>
	Observable<T> Observable<T>::filter(F predicate) const
	{
		return Observable{(source_ | rpp::operators::filter(std::move(predicate))).as_dynamic()};
	}

	template <detail::Value T>
	template <concepts::FlatMapperFor<T, rppw::Observable> F>
	auto Observable<T>::flatMap(F function) const
	{
		using R = std::invoke_result_t<const F &, T>;
		using U = typename R::value_type;
		return Observable<U>{(source_ | rpp::operators::flat_map(
											[function = std::move(function)](const T &value)
											{
												return std::invoke(function, value).source_;
											}))
								 .as_dynamic()};
	}

	template <detail::Value T>
	Observable<T> Observable<T>::take(std::size_t count) const
	{
		if (count == 0)
			return empty();
		return Observable{(source_ | rpp::operators::take(count)).as_dynamic()};
	}

	template <detail::Value T>
	template <rpp::schedulers::constraint::scheduler S>
	Observable<T> Observable<T>::subscribeOn(S scheduler) const
	{
		return Observable{(source_ | rpp::operators::subscribe_on(std::move(scheduler))).as_dynamic()};
	}

	template <detail::Value T>
	template <rpp::schedulers::constraint::scheduler S>
	Observable<T> Observable<T>::observeOn(S scheduler) const
	{
		// RPP observe_on forwards errors immediately; zero-delay also schedules errors.
		return Observable{(source_ | rpp::operators::delay(rpp::schedulers::duration::zero(), std::move(scheduler))).as_dynamic()};
	}

	template <detail::Value T>
	Single<T> Observable<T>::singleOrError() const
	{
		return Single<T>{Observable{detail::cardinality(source_, true, "singleOrError")}};
	}

	template <detail::Value T>
	Maybe<T> Observable<T>::singleElement() const
	{
		return Maybe<T>{Observable{detail::cardinality(source_, false, "singleElement")}};
	}

	template <detail::Value T>
	Completable Observable<T>::ignoreElements() const
	{
		return Completable{Observable<std::monostate>{(source_ |
													   rpp::operators::filter(
														   [](const T &)
														   {
															   return false;
														   }) |
													   rpp::operators::map(
														   [](const T &)
														   {
															   return std::monostate{};
														   }))
														  .as_dynamic()}};
	}
} // namespace rppw
