// tests
#include <support/ReactiveTest.h>

namespace rppw::test
{
	namespace
	{
		using ::testing::InSequence;
		using ::testing::MockFunction;
		using ::testing::StrictMock;

		class SingleTest : public ReactiveTest
		{
		};

		TEST_F(SingleTest, EmitsExactlyOneSuccess)
		{
			EXPECT_CALL(observer, next(7));
			EXPECT_TRUE(subscribe(Single<int>::just(7)).isDisposed());
		}

		TEST_F(SingleTest, ForwardsOriginalError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Single<int>::error(failure));
		}

		TEST_F(SingleTest, RejectsNullErrorSynchronously)
		{
			EXPECT_THROW(Single<int>::error({}), std::invalid_argument);
		}

		TEST_F(SingleTest, InvokesCallableLazilyForEverySubscription)
		{
			int		   calls = 0;
			const auto source = Single<int>::fromCallable(
				[&]
				{
					return ++calls;
				});
			EXPECT_EQ(calls, 0);
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			subscribe(source);
			subscribe(source);
			EXPECT_EQ(calls, 2);
		}

		TEST_F(SingleTest, RoutesCallableExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("callable")));
			EXPECT_NO_THROW(subscribe(Single<int>::fromCallable(
				[]() -> int
				{
					throw std::runtime_error{"callable"};
				})));
		}

		TEST_F(SingleTest, InvokesDeferredFactoryForEverySubscription)
		{
			int		   calls = 0;
			const auto source = Single<int>::defer(
				[&]
				{
					return Single<int>::just(++calls);
				});
			EXPECT_EQ(calls, 0);
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			subscribe(source);
			subscribe(source);
		}

		TEST_F(SingleTest, RoutesFactoryExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("factory")));
			subscribe(Single<int>::defer(
				[]() -> Single<int>
				{
					throw std::runtime_error{"factory"};
				}));
		}

		TEST_F(SingleTest, MapsSuccess)
		{
			EXPECT_CALL(observer, next(14));
			subscribe(Single<int>::just(7).map(
				[](int value)
				{
					return value * 2;
				}));
		}

		TEST_F(SingleTest, DoesNotInvokeMapperDuringComposition)
		{
			StrictMock<MockFunction<int(int)>> mapper;
			const auto						   source = Single<int>::just(7).map(mapper.AsStdFunction());
			EXPECT_CALL(mapper, Call(7)).WillOnce(::testing::Return(8));
			EXPECT_CALL(observer, next(8));
			subscribe(source);
		}

		TEST_F(SingleTest, RoutesMapperExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("map")));
			subscribe(Single<int>::just(7).map(
				[](int) -> int
				{
					throw std::runtime_error{"map"};
				}));
		}

		TEST_F(SingleTest, FlatMapsSuccess)
		{
			EXPECT_CALL(observer, next(8));
			subscribe(Single<int>::just(7).flatMap(
				[](int value)
				{
					return Single<int>::just(value + 1);
				}));
		}

		TEST_F(SingleTest, RoutesFlatMapFactoryExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("flatMap")));
			subscribe(Single<int>::just(7).flatMap(
				[](int) -> Single<int>
				{
					throw std::runtime_error{"flatMap"};
				}));
		}

		TEST_F(SingleTest, ForwardsFlatMapInnerError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Single<int>::just(7).flatMap(
				[this](int)
				{
					return Single<int>::error(failure);
				}));
		}

		TEST_F(SingleTest, FilterPreservesAcceptedSuccess)
		{
			EXPECT_CALL(observer, next(7));
			subscribe(Single<int>::just(7).filter(
				[](int value)
				{
					return value == 7;
				}));
		}

		TEST_F(SingleTest, FilterConvertsRejectedSuccessToEmptyMaybe)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Single<int>::just(7).filter(
				[](int)
				{
					return false;
				}));
		}

		TEST_F(SingleTest, RoutesPredicateExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("filter")));
			subscribe(Single<int>::just(7).filter(
				[](int) -> bool
				{
					throw std::runtime_error{"filter"};
				}));
		}

		TEST_F(SingleTest, ForwardsErrorWithoutInvokingOperators)
		{
			StrictMock<MockFunction<int(int)>>	mapper;
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(observer, error(failure));
			subscribe(Single<int>::error(failure)
						  .map(mapper.AsStdFunction())
						  .flatMap(
							  [](int n)
							  {
								  return Single<int>::just(n);
							  })
						  .filter(predicate.AsStdFunction()));
		}

		TEST_F(SingleTest, ConvertsSuccessToObservable)
		{
			InSequence sequence;
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(Single<int>::just(7).toObservable());
		}

		TEST_F(SingleTest, ConvertsSuccessToMaybeWithoutEmptyCompletion)
		{
			EXPECT_CALL(observer, next(7));
			subscribe(Single<int>::just(7).toMaybe());
		}

		TEST_F(SingleTest, ConvertsErrorToObservable)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Single<int>::error(failure).toObservable());
		}

		TEST_F(SingleTest, ConvertsErrorToMaybe)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Single<int>::error(failure).toMaybe());
		}

		TEST_F(SingleTest, IgnoresSuccessValue)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Single<int>::just(7).ignoreElement());
		}

		TEST_F(SingleTest, IgnoreElementPreservesError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Single<int>::error(failure).ignoreElement());
		}
	} // namespace
} // namespace rppw::test
