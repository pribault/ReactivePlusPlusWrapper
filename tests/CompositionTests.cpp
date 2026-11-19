// tests
#include <support/RunLoopTest.h>

// stl
#include <array>
#include <cstddef>
#include <latch>
#include <list>
#include <ranges>
#include <thread>
#include <vector>

namespace rppw::test
{
	namespace
	{
		template <typename S, bool Merge>
		struct CompositionCase
		{
			using Source = S;
			static constexpr bool merge = Merge;
		};

		template <typename Case>
		class CompositionTest : public RunLoopTest
		{
		protected:
			using Source = typename Case::Source;

			template <typename R>
			auto combine(R &&sources)
			{
				using Result = std::conditional_t<std::same_as<Source, Completable>, Completable, Observable<int>>;
				if constexpr (Case::merge)
					return Result::merge(std::forward<R>(sources));
				else
					return Result::concat(std::forward<R>(sources));
			}

			Source makeSource(int value)
			{
				return Source::defer(
					[this, value]
					{
						started.push_back(value);
						if constexpr (std::same_as<Source, Completable>)
							return Source::complete();
						else
							return Source::just(value);
					});
			}

			void expectValue(int value, int count = 1)
			{
				if constexpr (!std::same_as<Source, Completable>)
					EXPECT_CALL(observer, next(value)).Times(count);
			}

			std::vector<int> started;
		};

		using CompositionCases = ::testing::Types<CompositionCase<Observable<int>, false>, CompositionCase<Observable<int>, true>,
												  CompositionCase<Single<int>, false>, CompositionCase<Single<int>, true>, CompositionCase<Maybe<int>, false>,
												  CompositionCase<Maybe<int>, true>, CompositionCase<Completable, false>, CompositionCase<Completable, true>>;
		TYPED_TEST_SUITE(CompositionTest, CompositionCases);

		TYPED_TEST(CompositionTest, CombinesSourcesInSynchronousOrder)
		{
			const auto			  source = this->combine(std::array{this->makeSource(1), this->makeSource(2)});
			::testing::InSequence sequence;
			this->expectValue(1);
			this->expectValue(2);
			EXPECT_CALL(this->observer, complete());
			EXPECT_TRUE(this->subscribe(source).isDisposed());
			EXPECT_EQ(this->started, (std::vector{1, 2}));
		}

		TYPED_TEST(CompositionTest, CompletesEmptyRange)
		{
			EXPECT_CALL(this->observer, complete());
			EXPECT_TRUE(this->subscribe(this->combine(std::vector<typename TestFixture::Source>{})).isDisposed());
		}

		TYPED_TEST(CompositionTest, OwnsSourcesAfterRangeDestruction)
		{
			const auto source = [this]
			{
				const std::list sources{this->makeSource(1)};
				return this->combine(sources);
			}();
			this->expectValue(1);
			EXPECT_CALL(this->observer, complete());
			this->subscribe(source);
			EXPECT_EQ(this->started, (std::vector{1}));
		}

		TYPED_TEST(CompositionTest, CopiesSourcesBeforeRangeMutation)
		{
			using Source = typename TestFixture::Source;
			std::vector sources{this->makeSource(1)};
			const auto	source = this->combine(sources);
			sources.front() = Source::error(this->failure);
			this->expectValue(1);
			EXPECT_CALL(this->observer, complete());
			this->subscribe(source);
		}

		TYPED_TEST(CompositionTest, DefersSourcesUntilSubscription)
		{
			const auto source = this->combine(std::array{this->makeSource(1)});
			EXPECT_TRUE(this->started.empty());
			this->expectValue(1);
			EXPECT_CALL(this->observer, complete());
			this->subscribe(source);
			EXPECT_EQ(this->started, (std::vector{1}));
		}

		TYPED_TEST(CompositionTest, RestartsSourcesForEverySubscription)
		{
			const auto source = this->combine(std::array{this->makeSource(1), this->makeSource(2)});
			this->expectValue(1, 2);
			this->expectValue(2, 2);
			EXPECT_CALL(this->observer, complete()).Times(2);
			this->subscribe(source);
			this->subscribe(source);
			EXPECT_EQ(this->started, (std::vector{1, 2, 1, 2}));
		}

		TYPED_TEST(CompositionTest, StopsOnFirstError)
		{
			using Source = typename TestFixture::Source;
			EXPECT_CALL(this->observer, error(this->failure));
			EXPECT_TRUE(this->subscribe(this->combine(std::array{Source::error(this->failure), this->makeSource(2)})).isDisposed());
			EXPECT_TRUE(this->started.empty());
		}

		TYPED_TEST(CompositionTest, PreservesValuesBeforeLaterError)
		{
			using Source = typename TestFixture::Source;
			::testing::InSequence sequence;
			this->expectValue(1);
			EXPECT_CALL(this->observer, error(this->failure));
			this->subscribe(this->combine(std::array{this->makeSource(1), Source::error(this->failure).observeOn(this->loop)}));
			this->drain();
			EXPECT_EQ(this->started, (std::vector{1}));
		}

		TYPED_TEST(CompositionTest, PropagatesRangeTraversalExceptionSynchronously)
		{
			const auto sources = std::views::iota(0, 2) | std::views::transform(
															  [](int) -> typename TestFixture::Source
															  {
																  throw std::runtime_error{"range traversal"};
															  });
			EXPECT_THROW(this->combine(sources), std::runtime_error);
		}

		TYPED_TEST(CompositionTest, RespectsSequentialOrOverlappingSubscriptions)
		{
			const auto source = this->combine(std::array{this->makeSource(1).observeOn(this->loop), this->makeSource(2).observeOn(this->loop)});
			const auto subscription = this->subscribe(source);
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_EQ(this->started, (TypeParam::merge ? std::vector{1, 2} : std::vector{1}));
			this->expectValue(1);
			this->expectValue(2);
			EXPECT_CALL(this->observer, complete());
			this->drain();
			EXPECT_EQ(this->started, (std::vector{1, 2}));
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(CompositionTest, CancelsPendingSubscriptions)
		{
			const auto source = this->combine(std::array{this->makeSource(1).subscribeOn(this->loop), this->makeSource(2).subscribeOn(this->loop)});
			const auto subscription = this->subscribe(source);
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(this->started.empty());
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(CompositionTest, CancelsActiveSourcesOnError)
		{
			using Source = typename TestFixture::Source;
			const auto source = this->combine(std::array{Source::error(this->failure).observeOn(this->loop), this->makeSource(2).subscribeOn(this->loop)});
			const auto subscription = this->subscribe(source);
			EXPECT_CALL(this->observer, error(this->failure));
			this->drain();
			EXPECT_TRUE(this->started.empty());
			EXPECT_TRUE(subscription.isDisposed());
		}

		class ObservableCompositionTest : public ReactiveTest
		{
		};

		TEST_F(ObservableCompositionTest, ConcatenatesAllValuesInSourceOrder)
		{
			::testing::InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(observer, next(4));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::concat(
				std::array{Observable<int>::fromIterable(std::array{1, 2}), Observable<int>::empty(), Observable<int>::fromIterable(std::array{3, 4})}));
		}

		TEST_F(ObservableCompositionTest, MergesAllValues)
		{
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(observer, next(4));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::merge(
				std::array{Observable<int>::fromIterable(std::array{1, 2}), Observable<int>::empty(), Observable<int>::fromIterable(std::array{3, 4})}));
		}

		TEST_F(ObservableCompositionTest, ConcatSkipsEmptyMaybes)
		{
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::concat(std::array{Maybe<int>::empty(), Maybe<int>::just(7), Maybe<int>::empty()}));
		}

		TEST_F(ObservableCompositionTest, MergeSkipsEmptyMaybes)
		{
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::merge(std::array{Maybe<int>::empty(), Maybe<int>::just(7), Maybe<int>::empty()}));
		}

		TEST_F(ObservableCompositionTest, BoundedMergeRefillsSlotsIndividually)
		{
			std::vector<rpp::dynamic_observer<int>> active;
			const auto								source = Observable<int>::create(
				 [&active](rpp::dynamic_observer<int> inner)
				 {
					 active.push_back(std::move(inner));
				 });
			const auto merged = Observable<int>::merge(std::vector(6, source), 4);
			EXPECT_TRUE(active.empty());

			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete()).Times(2);
			const auto first = subscribe(merged);
			ASSERT_EQ(active.size(), 4);
			auto finish = [&active](std::size_t index)
			{
				auto inner = active.at(index);
				inner.on_completed();
			};
			finish(2);
			ASSERT_EQ(active.size(), 5);
			finish(4);
			ASSERT_EQ(active.size(), 6);
			active[5].on_next(7);
			for (auto index : {0, 1, 3, 5})
				finish(index);
			EXPECT_TRUE(first.isDisposed());

			active.clear();
			const auto second = subscribe(merged);
			ASSERT_EQ(active.size(), 4);
			for (std::size_t index = 0; index < 6; ++index)
				finish(index);
			EXPECT_TRUE(second.isDisposed());
		}

		TEST_F(ObservableCompositionTest, BoundedMergeStopsOnErrorOrCancellation)
		{
			std::vector<rpp::dynamic_observer<int>> active;
			const auto								source = Observable<int>::create(
				 [&active](rpp::dynamic_observer<int> inner)
				 {
					 active.push_back(std::move(inner));
				 });
			const auto merged = Observable<int>::merge(std::vector(5, source), 4);

			EXPECT_CALL(observer, error(failure));
			const auto failed = subscribe(merged);
			ASSERT_EQ(active.size(), 4);
			active.front().on_error(failure);
			EXPECT_TRUE(failed.isDisposed());
			ASSERT_EQ(active.size(), 4);
			for (const auto &inner : active)
				EXPECT_TRUE(inner.is_disposed());

			active.clear();
			const auto cancelled = subscribe(merged);
			ASSERT_EQ(active.size(), 4);
			cancelled.dispose();
			ASSERT_EQ(active.size(), 4);
			for (const auto &inner : active)
				EXPECT_TRUE(inner.is_disposed());
		}

		TEST_F(ObservableCompositionTest, BoundedMergeClaimsEachSourceOnceOnConcurrentCompletions)
		{
			std::vector<rpp::dynamic_observer<int>> active;
			const auto								held = Observable<int>::create(
				 [&active](rpp::dynamic_observer<int> inner)
				 {
					 active.push_back(std::move(inner));
				 });
			std::vector sources(4, held);
			for (int value = 0; value < 256; ++value)
			{
				sources.push_back(Observable<int>::just(value));
				EXPECT_CALL(observer, next(value));
			}
			EXPECT_CALL(observer, complete());
			const auto subscription = subscribe(Observable<int>::merge(sources, 4));
			ASSERT_EQ(active.size(), 4);

			std::latch				 start{static_cast<std::ptrdiff_t>(active.size())};
			std::vector<std::thread> workers;
			for (const auto &inner : active)
				workers.emplace_back(
					[inner, &start]
					{
						start.arrive_and_wait();
						inner.on_completed();
					});
			for (auto &worker : workers)
				worker.join();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableCompositionTest, BoundedMergeDoesNotRefillWhileReportingError)
		{
			std::vector<rpp::dynamic_observer<int>> active;
			const auto								hold = Observable<int>::create(
				 [&active](rpp::dynamic_observer<int> inner)
				 {
					 active.push_back(std::move(inner));
				 });
			bool	   started = false;
			const auto pending = Observable<int>::defer(
				[&started]
				{
					started = true;
					return Observable<int>::empty();
				});
			const auto subscription = Observable<int>::merge(std::array{hold, hold, pending}, 2)
										  .subscribe([](int) {},
													 [&](std::exception_ptr error)
													 {
														 EXPECT_EQ(error, failure);
														 active[1].on_completed();
														 EXPECT_FALSE(started);
													 },
													 []
													 {
														 ADD_FAILURE() << "Unexpected completion";
													 });
			ASSERT_EQ(active.size(), 2);
			active[0].on_error(failure);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableCompositionTest, BoundedMergeRefillsAfterReentrantCompletion)
		{
			std::vector<rpp::dynamic_observer<int>> active;
			const auto								hold = Observable<int>::create(
				 [&active](rpp::dynamic_observer<int> inner)
				 {
					 active.push_back(std::move(inner));
				 });
			const auto refill = Observable<int>::create(
				[](rpp::dynamic_observer<int> inner)
				{
					inner.on_next(2);
					inner.on_completed();
				});
			std::vector<int> values;
			bool			 complete = false;
			const auto		 subscription = Observable<int>::merge(std::array{hold, hold, refill}, 2)
										  .subscribe(
											  [&](int value)
											  {
												  values.push_back(value);
												  if (value == 1)
												  {
													  active[1].on_completed();
													  EXPECT_EQ(values, std::vector{1});
												  }
											  },
											  [](std::exception_ptr)
											  {
												  ADD_FAILURE() << "Unexpected error";
											  },
											  [&complete]
											  {
												  complete = true;
											  });
			ASSERT_EQ(active.size(), 2);
			active[0].on_next(1);
			EXPECT_EQ(values, (std::vector{1, 2}));
			active[0].on_completed();
			EXPECT_TRUE(complete);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableCompositionTest, BoundedMergeAllowsSourceCleanupToCancel)
		{
			auto									subscription = Disposable::create();
			std::vector<rpp::dynamic_observer<int>> active;
			const auto								hold = Observable<int>::create(
				 [&](rpp::dynamic_observer<int> inner)
				 {
					 inner.set_upstream(rpp::make_callback_disposable(
						 [&subscription]() noexcept
						 {
							 subscription.dispose();
						 }));
					 active.push_back(std::move(inner));
				 });
			subscription = subscribe(Observable<int>::merge(std::vector(3, hold), 2));
			ASSERT_EQ(active.size(), 2);
			active[0].on_completed();
			EXPECT_EQ(active.size(), 2);
			EXPECT_TRUE(active[1].is_disposed());
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableCompositionTest, BoundedMergeSerializesReentrantNotificationsFromOnNext)
		{
			std::vector<rpp::dynamic_observer<int>> active;
			const auto								hold = Observable<int>::create(
				 [&active](rpp::dynamic_observer<int> inner)
				 {
					 active.push_back(std::move(inner));
				 });
			std::vector<int>   values;
			std::exception_ptr reported;
			const auto		   subscription = Observable<int>::merge(std::array{hold, hold}, 2)
										  .subscribe(
											  [&](int value)
											  {
												  values.push_back(value);
												  if (value == 1)
												  {
													  active[1].on_next(2);
													  active[1].on_error(failure);
													  EXPECT_EQ(values, std::vector{1});
													  EXPECT_FALSE(reported);
												  }
											  },
											  [&reported](std::exception_ptr error)
											  {
												  reported = error;
											  },
											  []
											  {
												  ADD_FAILURE() << "Unexpected completion";
											  });
			ASSERT_EQ(active.size(), 2);
			active[0].on_next(1);
			EXPECT_EQ(values, (std::vector{1, 2}));
			EXPECT_EQ(reported, failure);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableCompositionTest, BoundedMergeKeepsQueuedValueWhenSourceCompletesOnAnotherThread)
		{
			std::vector<rpp::dynamic_observer<int>> active;
			const auto								hold = Observable<int>::create(
				 [&active](rpp::dynamic_observer<int> inner)
				 {
					 active.push_back(std::move(inner));
				 });
			std::vector<int> values;
			bool			 complete = false;
			const auto		 subscription = Observable<int>::merge(std::array{hold, hold}, 2)
										  .subscribe(
											  [&](int value)
											  {
												  values.push_back(value);
												  if (value == 1)
												  {
													  active[1].on_next(2);
													  std::thread{[source = active[1]]
																  {
																	  source.on_completed();
																  }}
														  .join();
												  }
											  },
											  [](std::exception_ptr)
											  {
												  ADD_FAILURE() << "Unexpected error";
											  },
											  [&complete]
											  {
												  complete = true;
											  });
			ASSERT_EQ(active.size(), 2);
			active[0].on_next(1);
			EXPECT_EQ(values, (std::vector{1, 2}));
			active[0].on_completed();
			EXPECT_TRUE(complete);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableCompositionTest, BoundedMergeWithLimitOneSubscribesSequentially)
		{
			std::vector<rpp::dynamic_observer<int>> active;
			const auto								hold = Observable<int>::create(
				 [&active](rpp::dynamic_observer<int> inner)
				 {
					 active.push_back(std::move(inner));
				 });
			::testing::InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			const auto subscription = subscribe(Observable<int>::merge(std::array{hold, hold}, 1));
			ASSERT_EQ(active.size(), 1);
			active[0].on_next(1);
			auto first = active[0];
			first.on_completed();
			ASSERT_EQ(active.size(), 2);
			active[1].on_next(2);
			auto second = active[1];
			second.on_completed();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableCompositionTest, BoundedMergeDeliversRefilledSourceWithoutBuffering)
		{
			std::vector<rpp::dynamic_observer<int>> active;
			const auto								hold = Observable<int>::create(
				 [&active](rpp::dynamic_observer<int> inner)
				 {
					 active.push_back(std::move(inner));
				 });
			std::vector<int> values;
			const auto		 refill = Observable<int>::create(
				  [&values](rpp::dynamic_observer<int> inner)
				  {
					  inner.on_next(1);
					  EXPECT_EQ(values, std::vector{1});
					  inner.on_next(2);
					  EXPECT_EQ(values, (std::vector{1, 2}));
					  inner.on_completed();
				  });
			bool	   complete = false;
			const auto subscription = Observable<int>::merge(std::array{hold, refill}, 1)
										  .subscribe(
											  [&values](int value)
											  {
												  values.push_back(value);
											  },
											  [](std::exception_ptr)
											  {
												  ADD_FAILURE() << "Unexpected error";
											  },
											  [&complete]
											  {
												  complete = true;
											  });
			ASSERT_EQ(active.size(), 1);
			active[0].on_completed();
			EXPECT_EQ(values, (std::vector{1, 2}));
			EXPECT_TRUE(complete);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableCompositionTest, BoundedMergeRefillsManySynchronousSourcesWithoutRecursion)
		{
			std::vector<rpp::dynamic_observer<int>> active;
			std::vector								sources(1, Observable<int>::create(
									   [&active](rpp::dynamic_observer<int> inner)
									   {
										   active.push_back(std::move(inner));
									   }));
			sources.insert(sources.end(), 10'000, Observable<int>::just(1));
			int		   sum = 0;
			bool	   complete = false;
			const auto subscription = Observable<int>::merge(sources, 1)
										  .subscribe(
											  [&sum](int value)
											  {
												  sum += value;
											  },
											  [](std::exception_ptr)
											  {
												  ADD_FAILURE() << "Unexpected error";
											  },
											  [&complete]
											  {
												  complete = true;
											  });
			ASSERT_EQ(active.size(), 1);
			active[0].on_completed();
			EXPECT_EQ(sum, 10'000);
			EXPECT_TRUE(complete);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableCompositionTest, BoundedMergeDoesNotDeferNestedWorkInOnNext)
		{
			std::vector<rpp::dynamic_observer<int>> active;
			const auto								hold = Observable<int>::create(
				 [&active](rpp::dynamic_observer<int> inner)
				 {
					 active.push_back(std::move(inner));
				 });
			int		   nested = 0;
			const auto subscription = Observable<int>::merge(std::array{hold}, 1)
										  .subscribe(
											  [&nested](int)
											  {
												  Observable<int>::just(5).subscribe(
													  [&nested](int value)
													  {
														  nested = value;
													  },
													  [](std::exception_ptr) {}, [] {});
												  EXPECT_EQ(nested, 5);
											  },
											  [](std::exception_ptr)
											  {
												  ADD_FAILURE() << "Unexpected error";
											  },
											  [] {});
			ASSERT_EQ(active.size(), 1);
			active[0].on_next(1);
			EXPECT_EQ(nested, 5);
			subscription.dispose();
		}

		TEST_F(ObservableCompositionTest, BoundedMergeRejectsZeroAndSupportsEmptySources)
		{
			const std::array<Maybe<int>, 0> empty;
			EXPECT_THROW(Observable<int>::merge(empty, 0), std::invalid_argument);

			::testing::InSequence sequence;
			EXPECT_CALL(observer, complete());
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			EXPECT_TRUE(subscribe(Observable<int>::merge(empty, 4)).isDisposed());
			EXPECT_TRUE(subscribe(Observable<int>::merge(std::array{Maybe<int>::empty(), Maybe<int>::just(7)}, 2)).isDisposed());
		}
	} // namespace
} // namespace rppw::test
