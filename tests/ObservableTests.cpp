// tests
#include <support/RunLoopTest.h>

// stl
#include <array>
#include <limits>
#include <memory>
#include <ranges>
#include <vector>

namespace rppw::test
{
	namespace
	{
		using ::testing::_;
		using ::testing::InSequence;
		using ::testing::MockFunction;
		using ::testing::StrictMock;

		class ObservableTest : public ReactiveTest
		{
		};

		template <typename Case>
		class ObservablePrefixTest : public RunLoopTest
		{
		protected:
			template <typename F>
			Observable<int> apply(const Observable<int> &source, F predicate)
			{
				if constexpr (Case::value == 0)
					return source.takeUntil(std::move(predicate));
				else if constexpr (Case::value == 1)
					return source.takeWhile(
						[predicate = std::move(predicate)](const int &value)
						{
							return !std::invoke(predicate, value);
						});
				else if constexpr (Case::value == 2)
					return source.dropUntil(std::move(predicate));
				else
					return source.dropWhile(
						[predicate = std::move(predicate)](const int &value)
						{
							return !std::invoke(predicate, value);
						});
			}

			Disposable collect(const Observable<int> &source)
			{
				return source.subscribe(
					[this](int value)
					{
						values.push_back(value);
					},
					[this](std::exception_ptr error)
					{
						observer.error(error);
					},
					[this]
					{
						observer.complete();
					});
			}

			std::vector<int> expected()
			{
				if constexpr (Case::value == 0)
					return {1, 2};
				else if constexpr (Case::value == 1)
					return {1};
				else
					return {2, 3, 1};
			}

			std::vector<int> values;
		};

		using PrefixCases =
			::testing::Types<std::integral_constant<int, 0>, std::integral_constant<int, 1>, std::integral_constant<int, 2>, std::integral_constant<int, 3>>;
		TYPED_TEST_SUITE(ObservablePrefixTest, PrefixCases);

		TYPED_TEST(ObservablePrefixTest, SelectsPrefixBoundaryExactlyOnce)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(predicate, Call(1)).WillOnce(::testing::Return(false));
			EXPECT_CALL(predicate, Call(2)).WillOnce(::testing::Return(true));
			EXPECT_CALL(this->observer, complete());
			const auto subscription = this->collect(this->apply(Observable<int>::fromIterable(std::array{1, 2, 3, 1}), predicate.AsStdFunction()));
			EXPECT_EQ(this->values, this->expected());
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ObservablePrefixTest, CompletesEmptySourceWithoutEvaluatingPredicate)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(this->observer, complete());
			this->collect(this->apply(Observable<int>::empty(), predicate.AsStdFunction()));
			EXPECT_TRUE(this->values.empty());
		}

		TYPED_TEST(ObservablePrefixTest, PreservesUpstreamError)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(this->observer, error(this->failure));
			this->collect(this->apply(Observable<int>::error(this->failure), predicate.AsStdFunction()));
			EXPECT_TRUE(this->values.empty());
		}

		TYPED_TEST(ObservablePrefixTest, PropagatesPredicateExceptionWithoutForwardingValue)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(predicate, Call(1)).WillOnce(::testing::Throw(std::runtime_error{"predicate"}));
			EXPECT_CALL(this->observer, error(ExceptionContaining<std::runtime_error>("predicate")));
			const auto subscription = this->collect(this->apply(Observable<int>::fromIterable(std::array{1, 2}), predicate.AsStdFunction()));
			EXPECT_TRUE(this->values.empty());
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ObservablePrefixTest, HandlesBoundaryAtFirstValue)
		{
			EXPECT_CALL(this->observer, complete());
			this->collect(this->apply(Observable<int>::fromIterable(std::array{1, 2, 3}),
									  [](int)
									  {
										  return true;
									  }));
			if constexpr (TypeParam::value == 0)
				EXPECT_EQ(this->values, (std::vector{1}));
			else if constexpr (TypeParam::value == 1)
				EXPECT_TRUE(this->values.empty());
			else
				EXPECT_EQ(this->values, (std::vector{1, 2, 3}));
		}

		TYPED_TEST(ObservablePrefixTest, HandlesSourceWithoutBoundary)
		{
			EXPECT_CALL(this->observer, complete());
			this->collect(this->apply(Observable<int>::fromIterable(std::array{1, 2, 3}),
									  [](int)
									  {
										  return false;
									  }));
			if constexpr (TypeParam::value < 2)
				EXPECT_EQ(this->values, (std::vector{1, 2, 3}));
			else
				EXPECT_TRUE(this->values.empty());
		}

		TYPED_TEST(ObservablePrefixTest, ResetsStateForEachSubscription)
		{
			const auto source = this->apply(Observable<int>::fromIterable(std::array{1, 2, 3, 1}),
											[](int value)
											{
												return value == 2;
											});
			EXPECT_CALL(this->observer, complete()).Times(2);
			this->collect(source);
			EXPECT_EQ(this->values, this->expected());
			this->values.clear();
			this->collect(source);
			EXPECT_EQ(this->values, this->expected());
		}

		TYPED_TEST(ObservablePrefixTest, CancelsScheduledUpstream)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			StrictMock<MockFunction<bool(int)>>			predicate;
			const auto									subscription =
				this->collect(this->apply(Observable<int>::defer(factory.AsStdFunction()).subscribeOn(this->loop), predicate.AsStdFunction()));
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
			EXPECT_TRUE(this->values.empty());
		}

		TYPED_TEST(ObservablePrefixTest, DoesNotEvaluatePredicateDuringComposition)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			const auto							source = this->apply(Observable<int>::just(1), predicate.AsStdFunction());
			EXPECT_CALL(predicate, Call(1)).WillOnce(::testing::Return(false));
			EXPECT_CALL(this->observer, complete());
			this->collect(source);
		}

		TYPED_TEST(ObservablePrefixTest, PreservesErrorBeforeBoundary)
		{
			const auto source = Observable<int>::concat(std::array{Observable<int>::just(1), Observable<int>::error(this->failure)});
			EXPECT_CALL(this->observer, error(this->failure));
			this->collect(this->apply(source,
									  [](int)
									  {
										  return false;
									  }));
			if constexpr (TypeParam::value < 2)
				EXPECT_EQ(this->values, (std::vector{1}));
			else
				EXPECT_TRUE(this->values.empty());
		}

		TEST_F(ObservableTest, EmitsJustValueBeforeCompletion)
		{
			InSequence sequence;
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			EXPECT_TRUE(subscribe(Observable<int>::just(7)).isDisposed());
		}

		TEST_F(ObservableTest, CompletesEmptySource)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::empty());
		}

		TEST_F(ObservableTest, EmitsIterableInOrder)
		{
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}));
		}

		TEST_F(ObservableTest, CompletesEmptyIterable)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::vector<int>{}));
		}

		TEST_F(ObservableTest, OwnsMaterializedRangeAfterOriginalIsDestroyed)
		{
			const auto source = []
			{
				const std::array values{2, 4, 6};
				return Observable<int>::fromIterable(values | std::views::filter(
																  [](int n)
																  {
																	  return n == 4;
																  }));
			}();
			EXPECT_CALL(observer, next(4));
			EXPECT_CALL(observer, complete());
			subscribe(source);
		}

		TEST_F(ObservableTest, CopiesRangeBeforeOriginalMutation)
		{
			std::vector values{7};
			const auto	source = Observable<int>::fromIterable(values);
			values.front() = 99;
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(source);
		}

		TEST_F(ObservableTest, PropagatesRangeTraversalExceptionSynchronously)
		{
			const auto range = std::views::iota(0, 3) | std::views::transform(
															[](int) -> int
															{
																throw std::runtime_error{"range traversal"};
															});
			EXPECT_THROW(Observable<int>::fromIterable(range), std::runtime_error);
		}

		TEST_F(ObservableTest, ForwardsOriginalErrorWithoutCompletion)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::error(failure));
		}

		TEST_F(ObservableTest, RejectsNullErrorSynchronously)
		{
			EXPECT_THROW(Observable<int>::error({}), std::invalid_argument);
		}

		TEST_F(ObservableTest, DefersFactoryUntilEverySubscription)
		{
			int		   invocations = 0;
			const auto source = Observable<int>::defer(
				[&]
				{
					return Observable<int>::just(++invocations);
				});
			EXPECT_EQ(invocations, 0);
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, complete());
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			subscribe(source);
			subscribe(source);
			EXPECT_EQ(invocations, 2);
		}

		TEST_F(ObservableTest, RoutesFactoryExceptionToError)
		{
			const auto source = Observable<int>::defer(
				[]() -> Observable<int>
				{
					throw std::runtime_error{"factory"};
				});
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("factory")));
			EXPECT_NO_THROW(subscribe(source));
		}

		TEST_F(ObservableTest, MapsWithoutMutatingSource)
		{
			const auto source = Observable<int>::just(3);
			InSequence sequence;
			EXPECT_CALL(observer, next(6));
			EXPECT_CALL(observer, complete());
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(observer, complete());
			subscribe(source.map(
				[](int n)
				{
					return n * 2;
				}));
			subscribe(source);
		}

		TEST_F(ObservableTest, DoesNotInvokeOperatorDuringComposition)
		{
			StrictMock<MockFunction<int(int)>> mapper;
			const auto						   source = Observable<int>::just(3).map(mapper.AsStdFunction());
			EXPECT_CALL(mapper, Call(3)).WillOnce(::testing::Return(6));
			EXPECT_CALL(observer, next(6));
			EXPECT_CALL(observer, complete());
			subscribe(source);
		}

		TEST_F(ObservableTest, StopsAfterMapperThrows)
		{
			StrictMock<MockFunction<int(int)>> mapper;
			EXPECT_CALL(mapper, Call(1)).WillOnce(::testing::Throw(std::runtime_error{"map"}));
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("map")));
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}).map(mapper.AsStdFunction()));
		}

		TEST_F(ObservableTest, FiltersRejectedValues)
		{
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3})
						  .filter(
							  [](int n)
							  {
								  return n % 2 == 0;
							  }));
		}

		TEST_F(ObservableTest, CompletesWhenAllValuesAreRejected)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::just(1).filter(
				[](int)
				{
					return false;
				}));
		}

		TEST_F(ObservableTest, RoutesPredicateExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("predicate")));
			subscribe(Observable<int>::just(1).filter(
				[](int) -> bool
				{
					throw std::runtime_error{"predicate"};
				}));
		}

		TEST_F(ObservableTest, FlatMapsInnerValues)
		{
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(11));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, next(12));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2})
						  .flatMap(
							  [](int n)
							  {
								  return Observable<int>::fromIterable(std::array{n, n + 10});
							  }));
		}

		TEST_F(ObservableTest, FlatMapCompletesEmptyInnerSource)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::just(1).flatMap(
				[](int)
				{
					return Observable<int>::empty();
				}));
		}

		TEST_F(ObservableTest, RoutesFlatMapFactoryExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("flatMap")));
			subscribe(Observable<int>::just(1).flatMap(
				[](int) -> Observable<int>
				{
					throw std::runtime_error{"flatMap"};
				}));
		}

		TEST_F(ObservableTest, ForwardsFlatMapInnerError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::just(1).flatMap(
				[this](int)
				{
					return Observable<int>::error(failure);
				}));
		}

		TEST_F(ObservableTest, ForwardsErrorThroughComposedOperators)
		{
			StrictMock<MockFunction<int(int)>>	mapper;
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::error(failure)
						  .map(mapper.AsStdFunction())
						  .filter(predicate.AsStdFunction())
						  .flatMap(
							  [](int n)
							  {
								  return Observable<int>::just(n);
							  })
						  .take(1));
		}

		TEST_F(ObservableTest, TakeZeroDoesNotSubscribeUpstream)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::defer(factory.AsStdFunction()).take(0));
		}

		TEST_F(ObservableTest, TakeCancelsBeforeProducingNextValue)
		{
			StrictMock<MockFunction<int(int)>> mapper;
			EXPECT_CALL(mapper, Call(1)).WillOnce(::testing::Return(1));
			EXPECT_CALL(mapper, Call(2)).WillOnce(::testing::Return(2));
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}).map(mapper.AsStdFunction()).take(2));
		}

		TEST_F(ObservableTest, TakeLargerThanSourceCompletesNormally)
		{
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::just(1).take(5));
		}

		TEST_F(ObservableTest, TakeUntilCancelsBeforeProducingNextValue)
		{
			StrictMock<MockFunction<void(int)>> produced;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(produced, Call(2));
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3})
						  .doOnNext(produced.AsStdFunction())
						  .takeUntil(
							  [](int value)
							  {
								  return value == 2;
							  }));
		}

		TEST_F(ObservableTest, TakeWhileCancelsBeforeProducingFollowingValue)
		{
			StrictMock<MockFunction<void(int)>> produced;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(produced, Call(2));
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3})
						  .doOnNext(produced.AsStdFunction())
						  .takeWhile(
							  [](int value)
							  {
								  return value < 2;
							  }));
		}

		TEST_F(ObservableTest, TakeUntilPropagatesDownstreamCancellation)
		{
			StrictMock<MockFunction<void(int)>> produced;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3})
						  .doOnNext(produced.AsStdFunction())
						  .takeUntil(
							  [](int)
							  {
								  return false;
							  })
						  .take(1));
		}

		TEST_F(ObservableTest, DropPropagatesDownstreamCancellation)
		{
			StrictMock<MockFunction<void(int)>> produced;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(produced, Call(2));
			EXPECT_CALL(produced, Call(3));
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3, 4}).doOnNext(produced.AsStdFunction()).drop(2).take(1));
		}

		TEST_F(ObservableTest, DropZeroPreservesAllValues)
		{
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2}).drop(0));
		}

		TEST_F(ObservableTest, DropDiscardsOnlyInitialCount)
		{
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}).drop(2));
		}

		TEST_F(ObservableTest, DropCountBeyondSourceCompletesWithoutValues)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::just(1).drop(std::numeric_limits<std::size_t>::max()));
		}

		TEST_F(ObservableTest, DropCountEqualToSourceCompletesWithoutValues)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2}).drop(2));
		}

		TEST_F(ObservableTest, DropPreservesEmptyCompletion)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::empty().drop(2));
		}

		TEST_F(ObservableTest, DropPreservesUpstreamError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::error(failure).drop(2));
		}

		TEST_F(ObservableTest, DropResetsCountForEachSubscription)
		{
			const auto source = Observable<int>::fromIterable(std::array{1, 2}).drop(1);
			EXPECT_CALL(observer, next(2)).Times(2);
			EXPECT_CALL(observer, complete()).Times(2);
			subscribe(source);
			subscribe(source);
		}

		TEST_F(ObservableTest, ConvertsOneValueToSingle)
		{
			EXPECT_CALL(observer, next(7));
			subscribe(Observable<int>::just(7).singleOrError());
		}

		TEST_F(ObservableTest, ConvertsOneValueToMaybe)
		{
			EXPECT_CALL(observer, next(7));
			subscribe(Observable<int>::just(7).singleElement());
		}

		TEST_F(ObservableTest, ConvertsEmptySourceToEmptyMaybe)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::empty().singleElement());
		}

		TEST_F(ObservableTest, BuffersSingleValueUntilUpstreamCompletes)
		{
			StrictMock<MockFunction<void(int)>> produced;
			InSequence							sequence;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(produced, Call(2));
			EXPECT_CALL(observer, next(1));
			subscribe(Observable<int>::fromIterable(std::array{1, 2})
						  .map(
							  [&](int n)
							  {
								  produced.Call(n);
								  return n;
							  })
						  .filter(
							  [](int n)
							  {
								  return n == 1;
							  })
						  .singleOrError());
		}

		TEST_F(ObservableTest, BuffersMaybeValueUntilUpstreamCompletes)
		{
			StrictMock<MockFunction<void(int)>> produced;
			InSequence							sequence;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(produced, Call(2));
			EXPECT_CALL(observer, next(1));
			subscribe(Observable<int>::fromIterable(std::array{1, 2})
						  .map(
							  [&](int n)
							  {
								  produced.Call(n);
								  return n;
							  })
						  .filter(
							  [](int n)
							  {
								  return n == 1;
							  })
						  .singleElement());
		}

		TEST_F(ObservableTest, PrioritizesErrorOverBufferedSingleValue)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("producer failed")));
			subscribe(Observable<int>::fromIterable(std::array{1, 2})
						  .map(
							  [this](int n)
							  {
								  if (n == 2)
									  std::rethrow_exception(failure);
								  return n;
							  })
						  .singleOrError());
		}

		TEST_F(ObservableTest, PrioritizesErrorOverBufferedMaybeValue)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("producer failed")));
			subscribe(Observable<int>::fromIterable(std::array{1, 2})
						  .map(
							  [this](int n)
							  {
								  if (n == 2)
									  std::rethrow_exception(failure);
								  return n;
							  })
						  .singleElement());
		}

		TEST_F(ObservableTest, SingleCardinalityFailureCancelsAtSecondValue)
		{
			StrictMock<MockFunction<int(int)>> mapper;
			EXPECT_CALL(mapper, Call(1)).WillOnce(::testing::Return(1));
			EXPECT_CALL(mapper, Call(2)).WillOnce(::testing::Return(2));
			EXPECT_CALL(observer, error(ExceptionContaining<std::out_of_range>("singleOrError")));
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}).map(mapper.AsStdFunction()).singleOrError());
		}

		TEST_F(ObservableTest, MaybeCardinalityFailureCancelsAtSecondValue)
		{
			StrictMock<MockFunction<int(int)>> mapper;
			EXPECT_CALL(mapper, Call(1)).WillOnce(::testing::Return(1));
			EXPECT_CALL(mapper, Call(2)).WillOnce(::testing::Return(2));
			EXPECT_CALL(observer, error(ExceptionContaining<std::out_of_range>("singleElement")));
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}).map(mapper.AsStdFunction()).singleElement());
		}

		TEST_F(ObservableTest, IgnoresValuesWhilePreservingCompletion)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}).ignoreElements());
		}

		TEST_F(ObservableTest, IgnoreElementsPreservesError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::error(failure).ignoreElements());
		}

		class InvalidSingleCardinalityTest : public ReactiveTest, public ::testing::WithParamInterface<std::vector<int>>
		{
		};

		TEST_P(InvalidSingleCardinalityTest, RejectsInvalidValueCount)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::out_of_range>("singleOrError: expected")));
			subscribe(Observable<int>::fromIterable(GetParam()).singleOrError());
		}

		INSTANTIATE_TEST_SUITE_P(ValueCounts, InvalidSingleCardinalityTest, ::testing::Values(std::vector<int>{}, std::vector{1, 2}, std::vector{1, 2, 3, 4}));

		class InvalidMaybeCardinalityTest : public ReactiveTest, public ::testing::WithParamInterface<std::vector<int>>
		{
		};

		TEST_P(InvalidMaybeCardinalityTest, RejectsMultipleValues)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::out_of_range>("singleElement: expected")));
			subscribe(Observable<int>::fromIterable(GetParam()).singleElement());
		}

		INSTANTIATE_TEST_SUITE_P(ValueCounts, InvalidMaybeCardinalityTest, ::testing::Values(std::vector{1, 2}, std::vector{1, 2, 3, 4}));
	} // namespace
} // namespace rppw::test
