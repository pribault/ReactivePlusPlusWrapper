// tests
#include <support/ReactiveTest.h>

// stl
#include <array>
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
