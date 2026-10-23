#pragma once

// stl
#include <functional>

// rppw
#include <rppw/Completable.h>
#include <rppw/Maybe.h>
#include <rppw/Observable.h>

namespace rppw
{
	template <detail::Value T>
	Single<T> Single<T>::just(T value)
	{
		return Single{Observable<T>::just(std::move(value))};
	}

	template <detail::Value T>
	template <concepts::CallbackFor<rpp::dynamic_observer<T>> F>
	Single<T> Single<T>::create(F producer)
	{
		return Single{Observable<T>::create(std::move(producer)).take(1)};
	}

	template <detail::Value T>
	template <concepts::FactoryFor<T> F>
	Single<T> Single<T>::fromCallable(F callable)
	{
		return defer(
			[callable = std::move(callable)]
			{
				return just(std::invoke(callable));
			});
	}

	template <detail::Value T>
	Single<T> Single<T>::error(std::exception_ptr error)
	{
		return Single{Observable<T>::error(std::move(error))};
	}

	template <detail::Value T>
	template <concepts::ExceptionValue E>
	Single<T> Single<T>::error(E error)
	{
		return Single::error(std::make_exception_ptr(error));
	}

	template <detail::Value T>
	template <concepts::SourceFactoryFor<T, rppw::Single> F>
	Single<T> Single<T>::defer(F factory)
	{
		return Single{Observable<T>::defer(
			[factory = std::move(factory)]
			{
				return std::invoke(factory).source_;
			})};
	}

	template <detail::Value T>
	Disposable Single<T>::subscribe() const
	{
		return source_.take(1).subscribe();
	}

	template <detail::Value T>
	template <concepts::CallbackFor<T> N, concepts::CallbackFor<std::exception_ptr &> E>
	Disposable Single<T>::subscribe(N onSuccess, E onError) const
	{
		return source_.take(1).subscribe(std::move(onSuccess), std::move(onError), [] {});
	}

	template <detail::Value T>
	template <concepts::CallbackFor<const std::exception_ptr &> F>
	Single<T> Single<T>::doOnError(F action) const
	{
		return Single{source_.doOnError(std::move(action))};
	}

	template <detail::Value T>
	template <concepts::CallbackFor F>
	Single<T> Single<T>::doOnComplete(F action) const
	{
		return doOnSuccess(
			[action = std::move(action)](const T &) noexcept
			{
				std::invoke(action);
			});
	}

	template <detail::Value T>
	template <concepts::CallbackFor<const T &> F>
	Single<T> Single<T>::doOnSuccess(F action) const
	{
		return Single{source_.doOnNext(std::move(action))};
	}

	template <detail::Value T>
	template <concepts::MapperFor<T> F>
	auto Single<T>::map(F function) const
	{
		using U = std::decay_t<std::invoke_result_t<const F &, T>>;
		return Single<U>{source_.map(std::move(function))};
	}

	template <detail::Value T>
	template <concepts::FlatMapperFor<T, rppw::Single> F>
	auto Single<T>::flatMap(F function) const
	{
		using R = std::invoke_result_t<const F &, T>;
		using U = typename R::value_type;
		return Single<U>{source_.flatMap(
			[function = std::move(function)](const T &value)
			{
				return std::invoke(function, value).source_;
			})};
	}

	template <detail::Value T>
	template <concepts::PredicateFor<T> F>
	Maybe<T> Single<T>::filter(F predicate) const
	{
		return Maybe<T>{source_.filter(std::move(predicate))};
	}

	template <detail::Value T>
	template <rpp::schedulers::constraint::scheduler S>
	Single<T> Single<T>::delay(rpp::schedulers::duration duration, S scheduler) const
	{
		return Single{source_.delay(duration, std::move(scheduler))};
	}

	template <detail::Value T>
	Observable<T> Single<T>::repeat(std::size_t count) const
	{
		return source_.repeat(count);
	}

	template <detail::Value T>
	Observable<T> Single<T>::repeat() const
	{
		return source_.repeat();
	}

	template <detail::Value T>
	Single<T> Single<T>::retry(std::size_t count) const
	{
		return Single{source_.retry(count)};
	}

	template <detail::Value T>
	Single<T> Single<T>::retry() const
	{
		return Single{source_.retry()};
	}

	template <detail::Value T>
	template <rpp::schedulers::constraint::scheduler S>
	Single<T> Single<T>::subscribeOn(S scheduler) const
	{
		return Single{source_.subscribeOn(std::move(scheduler))};
	}

	template <detail::Value T>
	template <rpp::schedulers::constraint::scheduler S>
	Single<T> Single<T>::observeOn(S scheduler) const
	{
		return Single{source_.observeOn(std::move(scheduler))};
	}

	template <detail::Value T>
	Observable<T> Single<T>::toObservable() const
	{
		return source_;
	}

	template <detail::Value T>
	Maybe<T> Single<T>::toMaybe() const
	{
		return Maybe<T>{source_};
	}

	template <detail::Value T>
	Completable Single<T>::ignoreElement() const
	{
		return source_.ignoreElements();
	}
} // namespace rppw
