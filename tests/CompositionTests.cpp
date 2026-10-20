// tests
#include <support/RunLoopTest.h>

// stl
#include <array>
#include <list>
#include <ranges>
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
	} // namespace
} // namespace rppw::test
