#pragma once

// stl
#include <functional>
#include <limits>
#include <memory>
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
	template <concepts::CallbackFor<const std::exception_ptr &> F>
	Observable<T> Observable<T>::doOnError(F action) const
	{
		return Observable{(source_ | rpp::operators::tap([](const T &) {},
														 [action = std::move(action)](const std::exception_ptr &error) noexcept
														 {
															 std::invoke(action, error);
														 },
														 [] {}))
							  .as_dynamic()};
	}

	template <detail::Value T>
	template <concepts::CallbackFor F>
	Observable<T> Observable<T>::doOnComplete(F action) const
	{
		return Observable{(source_ | rpp::operators::tap([](const T &) {}, [](const std::exception_ptr &) {},
														 [action = std::move(action)]() noexcept
														 {
															 std::invoke(action);
														 }))
							  .as_dynamic()};
	}

	template <detail::Value T>
	template <concepts::CallbackFor<const T &> F>
	Observable<T> Observable<T>::doOnNext(F action) const
	{
		return Observable{(source_ | rpp::operators::tap(
										 [action = std::move(action)](const T &value) noexcept
										 {
											 std::invoke(action, value);
										 },
										 [] {}))
							  .as_dynamic()};
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
	Observable<T> Observable<T>::switchIfEmpty(Observable fallback) const
	{
		return defer(
			[source = *this, fallback = std::move(fallback)]
			{
				const auto seen = std::make_shared<bool>(false);
				const auto first = source.doOnNext(
					[seen](const T &) noexcept
					{
						*seen = true;
					});
				const auto next = defer(
					[seen, fallback]
					{
						return *seen ? empty() : fallback;
					});
				return Observable{rpp::source::concat(first.source_, next.source_).as_dynamic()};
			});
	}

	template <detail::Value T>
	template <concepts::PredicateFor<T> F>
	Single<bool> Observable<T>::allOf(F predicate) const
	{
		return noneOf(
			[predicate = std::move(predicate)](const T &value)
			{
				return !std::invoke(predicate, value);
			});
	}

	template <detail::Value T>
	template <concepts::PredicateFor<T> F>
	Single<bool> Observable<T>::anyOf(F predicate) const
	{
		const auto firstMatch = filter(
									[predicate = std::move(predicate)](const T &value) -> bool
									{
										return std::invoke(predicate, value);
									})
									.take(1);
		return Single<bool>{Observable<bool>{(firstMatch.source_ | rpp::operators::reduce(false,
																						  [](bool, const T &)
																						  {
																							  return true;
																						  }))
												 .as_dynamic()}};
	}

	template <detail::Value T>
	template <concepts::PredicateFor<T> F>
	Single<bool> Observable<T>::noneOf(F predicate) const
	{
		return anyOf(std::move(predicate))
			.map(
				[](bool matched)
				{
					return !matched;
				});
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
	template <concepts::PredicateFor<T> F>
	Observable<T> Observable<T>::takeUntil(F predicate) const
	{
		return Observable{rpp::source::create<T>(
							  [source = source_, predicate = std::move(predicate)](rpp::dynamic_observer<T> observer)
							  {
								  const auto upstream = rpp::composite_disposable_wrapper::make();
								  observer.set_upstream(upstream);
								  source.subscribe(
									  upstream,
									  [observer, predicate](const T &value)
									  {
										  const bool stop = std::invoke(predicate, value);
										  observer.on_next(value);
										  if (stop)
											  observer.on_completed();
									  },
									  [observer](const std::exception_ptr &error)
									  {
										  observer.on_error(error);
									  },
									  [observer]
									  {
										  observer.on_completed();
									  });
							  })
							  .as_dynamic()};
	}

	template <detail::Value T>
	template <concepts::PredicateFor<T> F>
	Observable<T> Observable<T>::takeWhile(F predicate) const
	{
		return Observable{(source_ | rpp::operators::take_while(
										 [predicate = std::move(predicate)](const T &value) -> bool
										 {
											 return std::invoke(predicate, value);
										 }))
							  .as_dynamic()};
	}

	template <detail::Value T>
	Observable<T> Observable<T>::drop(std::size_t count) const
	{
		return Observable{(source_ | rpp::operators::skip(count)).as_dynamic()};
	}

	template <detail::Value T>
	template <concepts::PredicateFor<T> F>
	Observable<T> Observable<T>::dropUntil(F predicate) const
	{
		return dropWhile(
			[predicate = std::move(predicate)](const T &value)
			{
				return !std::invoke(predicate, value);
			});
	}

	template <detail::Value T>
	template <concepts::PredicateFor<T> F>
	Observable<T> Observable<T>::dropWhile(F predicate) const
	{
		struct KeepAfterPrefix
		{
			F			 predicate;
			mutable bool dropping = true;

			bool operator()(const T &value) const
			{
				if (dropping)
					dropping = std::invoke(predicate, value);
				return !dropping;
			}
		};
		return filter(KeepAfterPrefix{std::move(predicate)});
	}

	template <detail::Value T>
	template <rpp::schedulers::constraint::scheduler S>
	Observable<T> Observable<T>::delay(rpp::schedulers::duration duration, S scheduler) const
	{
		if (duration < rpp::schedulers::duration::zero())
			throw std::invalid_argument{"Observable::delay: negative duration"};
		return Observable{(source_ | rpp::operators::delay(duration, std::move(scheduler))).as_dynamic()};
	}

	template <detail::Value T>
	Observable<T> Observable<T>::repeat(std::size_t count) const
	{
		if (count == 0)
			return empty();
		return Observable{(source_ | rpp::operators::repeat(count)).as_dynamic()};
	}

	template <detail::Value T>
	Observable<T> Observable<T>::repeat() const
	{
		return Observable{(source_ | rpp::operators::repeat()).as_dynamic()};
	}

	template <detail::Value T>
	Observable<T> Observable<T>::retry(std::size_t count) const
	{
		return Observable{(source_ | rpp::operators::retry(count)).as_dynamic()};
	}

	template <detail::Value T>
	Observable<T> Observable<T>::retry() const
	{
		return Observable{(source_ | rpp::operators::retry()).as_dynamic()};
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
		return delay(rpp::schedulers::duration::zero(), std::move(scheduler));
	}

	template <detail::Value T>
	Maybe<T> Observable<T>::elementAt(std::size_t index) const
	{
		return drop(index).take(1).singleElement();
	}

	template <detail::Value T>
	Maybe<T> Observable<T>::first() const
	{
		return elementAt(0);
	}

	template <detail::Value T>
	Maybe<T> Observable<T>::last() const
	{
		return Observable{(source_ | rpp::operators::take_last(1)).as_dynamic()}.singleElement();
	}

	template <detail::Value T>
	Single<std::size_t> Observable<T>::count() const
	{
		return Single<std::size_t>{Observable<std::size_t>{(source_ | rpp::operators::reduce(std::size_t{0},
																							 [](std::size_t count, const T &)
																							 {
																								 if (count == std::numeric_limits<std::size_t>::max())
																									 throw std::overflow_error{
																										 "Observable::count: counter overflow"};
																								 return count + 1;
																							 }))
															   .as_dynamic()}};
	}

	template <detail::Value T>
	template <typename Compare>
		requires std::strict_weak_order<const Compare &, const T &, const T &>
	Maybe<T> Observable<T>::max(Compare compare) const
	{
		return min(
			[compare = std::move(compare)](const T &left, const T &right)
			{
				return std::invoke(compare, right, left);
			});
	}

	template <detail::Value T>
	template <typename Compare>
		requires std::strict_weak_order<const Compare &, const T &, const T &>
	Maybe<T> Observable<T>::min(Compare compare) const
	{
		return Maybe<T>{Observable{(source_ | rpp::operators::reduce(
												  [compare = std::move(compare)](T best, const T &value)
												  {
													  if (std::invoke(compare, value, std::as_const(best)))
														  return value;
													  return best;
												  }))
									   .as_dynamic()}};
	}

	template <detail::Value T>
	template <typename R, concepts::ReducerFor<R, T> F>
	Single<R> Observable<T>::reduce(R initial, F accumulator) const
	{
		return Single<R>{Observable<R>{(source_ | rpp::operators::reduce(std::move(initial),
																		 [accumulator = std::move(accumulator)](R acc, const T &value)
																		 {
																			 return std::invoke(accumulator, acc, value);
																		 }))
										   .as_dynamic()}};
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
