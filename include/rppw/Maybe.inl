#pragma once

// stl
#include <functional>
#include <memory>

// rppw
#include <rppw/Completable.h>
#include <rppw/Observable.h>
#include <rppw/Single.h>
#include <rppw/detail/Operators.inl>

namespace rppw
{
	template <detail::Value T>
	Maybe<T> Maybe<T>::just(T value)
	{
		return Maybe{Observable<T>::just(std::move(value))};
	}

	template <detail::Value T>
	template <concepts::CallbackFor<rpp::dynamic_observer<T>> F>
	Maybe<T> Maybe<T>::create(F producer)
	{
		return Maybe{Observable<T>::create(std::move(producer)).take(1)};
	}

	template <detail::Value T>
	Maybe<T> Maybe<T>::empty()
	{
		return Maybe{Observable<T>::empty()};
	}

	template <detail::Value T>
	template <concepts::FactoryFor<std::optional<T>> F>
	Maybe<T> Maybe<T>::fromCallable(F callable)
	{
		return defer(
			[callable = std::move(callable)]
			{
				auto value = std::invoke(callable);
				return value ? just(std::move(*value)) : empty();
			});
	}

	template <detail::Value T>
	Maybe<T> Maybe<T>::error(std::exception_ptr error)
	{
		return Maybe{Observable<T>::error(std::move(error))};
	}

	template <detail::Value T>
	template <concepts::ExceptionValue E>
	Maybe<T> Maybe<T>::error(E error)
	{
		return Maybe::error(std::make_exception_ptr(error));
	}

	template <detail::Value T>
	template <concepts::ExceptionValue S>
		requires std::convertible_to<S, std::string_view>
	Maybe<T> Maybe<T>::error(S message)
	{
		return Maybe{Observable<T>::error(std::string_view{message})};
	}

	template <detail::Value T>
	template <concepts::SourceFactoryFor<T, rppw::Maybe> F>
	Maybe<T> Maybe<T>::defer(F factory)
	{
		return Maybe{Observable<T>::defer(
			[factory = std::move(factory)]
			{
				return std::invoke(factory).source_;
			})};
	}

	template <detail::Value T>
	Disposable Maybe<T>::subscribe() const
	{
		return source_.take(1).subscribe();
	}

	template <detail::Value T>
	template <concepts::CallbackFor<T> N, concepts::CallbackFor<std::exception_ptr &> E, concepts::CallbackFor C>
	Disposable Maybe<T>::subscribe(N onSuccess, E onError, C onComplete) const
	{
		auto seen = std::make_shared<bool>(false);
		return source_.take(1).subscribe(
			[seen, onSuccess = std::move(onSuccess)](T value)
			{
				*seen = true;
				std::invoke(onSuccess, std::move(value));
			},
			std::move(onError),
			[seen, onComplete = std::move(onComplete)]
			{
				if (!*seen)
					std::invoke(onComplete);
			});
	}

	template <detail::Value T>
	template <concepts::CallbackFor<const std::exception_ptr &> F>
	Maybe<T> Maybe<T>::doOnError(F action) const
	{
		return Maybe{source_.doOnError(std::move(action))};
	}

	template <detail::Value T>
	template <concepts::CallbackFor F>
	Maybe<T> Maybe<T>::doOnComplete(F action) const
	{
		return defer(
			[source = source_, action = std::move(action)]
			{
				const auto seen = std::make_shared<bool>(false);
				return Maybe{source
								 .doOnNext(
									 [seen, action](const T &) noexcept
									 {
										 *seen = true;
										 std::invoke(action);
									 })
								 .doOnComplete(
									 [seen, action]() noexcept
									 {
										 if (!*seen)
											 std::invoke(action);
									 })};
			});
	}

	template <detail::Value T>
	template <concepts::CallbackFor<const T &> F>
	Maybe<T> Maybe<T>::doOnSuccess(F action) const
	{
		return Maybe{source_.doOnNext(std::move(action))};
	}

	template <detail::Value T>
	template <concepts::MapperFor<T> F>
	auto Maybe<T>::map(F function) const
	{
		using U = std::decay_t<std::invoke_result_t<const F &, T>>;
		return Maybe<U>{source_.map(std::move(function))};
	}

	template <detail::Value T>
	template <concepts::FlatMapperFor<T, rppw::Maybe> F>
	auto Maybe<T>::flatMap(F function) const
	{
		using R = std::invoke_result_t<const F &, T>;
		using U = typename R::value_type;
		return Maybe<U>{source_.flatMap(
			[function = std::move(function)](const T &value)
			{
				return std::invoke(function, value).source_;
			})};
	}

	template <detail::Value T>
	template <concepts::PredicateFor<T> F>
	Maybe<T> Maybe<T>::filter(F predicate) const
	{
		return Maybe{source_.filter(std::move(predicate))};
	}

	template <detail::Value T>
	template <rpp::schedulers::constraint::scheduler S>
	Maybe<T> Maybe<T>::delay(rpp::schedulers::duration duration, S scheduler) const
	{
		return Maybe{source_.delay(duration, std::move(scheduler))};
	}

	template <detail::Value T>
	Observable<T> Maybe<T>::repeat(std::size_t count) const
	{
		return source_.repeat(count);
	}

	template <detail::Value T>
	Observable<T> Maybe<T>::repeat() const
	{
		return source_.repeat();
	}

	template <detail::Value T>
	Maybe<T> Maybe<T>::retry(std::size_t count) const
	{
		return Maybe{source_.retry(count)};
	}

	template <detail::Value T>
	Maybe<T> Maybe<T>::retry() const
	{
		return Maybe{source_.retry()};
	}

	template <detail::Value T>
	template <rpp::schedulers::constraint::scheduler S>
	Maybe<T> Maybe<T>::subscribeOn(S scheduler) const
	{
		return Maybe{source_.subscribeOn(std::move(scheduler))};
	}

	template <detail::Value T>
	template <rpp::schedulers::constraint::scheduler S>
	Maybe<T> Maybe<T>::observeOn(S scheduler) const
	{
		return Maybe{source_.observeOn(std::move(scheduler))};
	}

	template <detail::Value T>
	Observable<T> Maybe<T>::toObservable() const
	{
		return source_;
	}

	template <detail::Value T>
	Single<T> Maybe<T>::toSingle() const
	{
		return Single<T>{Observable<T>{detail::cardinality(source_.source_, true, "toSingle")}};
	}

	template <detail::Value T>
	Single<T> Maybe<T>::defaultIfEmpty(T value) const
	{
		return Single<T>{Observable<T>{(source_.source_ |
										rpp::operators::reduce(std::optional<T>{},
															   [](std::optional<T>, const T &item)
															   {
																   return std::optional<T>{item};
															   }) |
										rpp::operators::map(
											[value = std::move(value)](const std::optional<T> &item)
											{
												return item.value_or(value);
											}))
										   .as_dynamic()}};
	}

	template <detail::Value T>
	Maybe<T> Maybe<T>::switchIfEmpty(Maybe fallback) const
	{
		return Maybe{source_.switchIfEmpty(std::move(fallback.source_))};
	}

	template <detail::Value T>
	Completable Maybe<T>::ignoreElement() const
	{
		return source_.ignoreElements();
	}
} // namespace rppw
