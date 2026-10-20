// tests
#include <support/ReactiveTest.h>

// stl
#include <optional>

namespace rppw::test
{
	namespace
	{
		using ::testing::InSequence;
		using ::testing::MockFunction;
		using ::testing::StrictMock;

		class MaybeTest : public ReactiveTest
		{
		};

		TEST_F(MaybeTest, EmitsSuccessWithoutEmptyCompletion)
		{
			EXPECT_CALL(observer, next(7));
			EXPECT_TRUE(subscribe(Maybe<int>::just(7)).isDisposed());
		}

		TEST_F(MaybeTest, CompletesWithoutSuccessWhenEmpty)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::empty());
		}

		TEST_F(MaybeTest, ForwardsOriginalError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Maybe<int>::error(failure));
		}

		TEST_F(MaybeTest, RejectsNullErrorSynchronously)
		{
			EXPECT_THROW(Maybe<int>::error({}), std::invalid_argument);
		}

		TEST_F(MaybeTest, InvokesCallableLazilyForEverySubscription)
		{
			int		   calls = 0;
			const auto source = Maybe<int>::fromCallable(
				[&]
				{
					return std::optional{++calls};
				});
			EXPECT_EQ(calls, 0);
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			subscribe(source);
			subscribe(source);
		}

		TEST_F(MaybeTest, CompletesWhenCallableReturnsNullopt)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::fromCallable(
				[]
				{
					return std::optional<int>{};
				}));
		}

		TEST_F(MaybeTest, RoutesCallableExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("callable")));
			EXPECT_NO_THROW(subscribe(Maybe<int>::fromCallable(
				[]() -> std::optional<int>
				{
					throw std::runtime_error{"callable"};
				})));
		}

		TEST_F(MaybeTest, InvokesFactoryLazilyForEverySubscription)
		{
			int		   calls = 0;
			const auto source = Maybe<int>::defer(
				[&]
				{
					return Maybe<int>::just(++calls);
				});
			EXPECT_EQ(calls, 0);
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			subscribe(source);
			subscribe(source);
		}

		TEST_F(MaybeTest, RoutesFactoryExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("factory")));
			subscribe(Maybe<int>::defer(
				[]() -> Maybe<int>
				{
					throw std::runtime_error{"factory"};
				}));
		}

		TEST_F(MaybeTest, MapsSuccess)
		{
			EXPECT_CALL(observer, next(14));
			subscribe(Maybe<int>::just(7).map(
				[](int value)
				{
					return value * 2;
				}));
		}

		TEST_F(MaybeTest, SkipsOperatorsForEmptySource)
		{
			StrictMock<MockFunction<int(int)>>		  mapper;
			StrictMock<MockFunction<Maybe<int>(int)>> flatMapper;
			StrictMock<MockFunction<bool(int)>>		  predicate;
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::empty().map(mapper.AsStdFunction()).flatMap(flatMapper.AsStdFunction()).filter(predicate.AsStdFunction()));
		}

		TEST_F(MaybeTest, RoutesMapperExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("map")));
			subscribe(Maybe<int>::just(7).map(
				[](int) -> int
				{
					throw std::runtime_error{"map"};
				}));
		}

		TEST_F(MaybeTest, FlatMapsSuccess)
		{
			EXPECT_CALL(observer, next(8));
			subscribe(Maybe<int>::just(7).flatMap(
				[](int value)
				{
					return Maybe<int>::just(value + 1);
				}));
		}

		TEST_F(MaybeTest, FlatMapsSuccessToEmpty)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::just(7).flatMap(
				[](int)
				{
					return Maybe<int>::empty();
				}));
		}

		TEST_F(MaybeTest, RoutesFlatMapFactoryExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("flatMap")));
			subscribe(Maybe<int>::just(7).flatMap(
				[](int) -> Maybe<int>
				{
					throw std::runtime_error{"flatMap"};
				}));
		}

		TEST_F(MaybeTest, ForwardsFlatMapInnerError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Maybe<int>::just(7).flatMap(
				[this](int)
				{
					return Maybe<int>::error(failure);
				}));
		}

		TEST_F(MaybeTest, PreservesAcceptedValue)
		{
			EXPECT_CALL(observer, next(7));
			subscribe(Maybe<int>::just(7).filter(
				[](int value)
				{
					return value == 7;
				}));
		}

		TEST_F(MaybeTest, CompletesWhenValueIsRejected)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::just(7).filter(
				[](int)
				{
					return false;
				}));
		}

		TEST_F(MaybeTest, RoutesPredicateExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("filter")));
			subscribe(Maybe<int>::just(7).filter(
				[](int) -> bool
				{
					throw std::runtime_error{"filter"};
				}));
		}

		TEST_F(MaybeTest, ForwardsErrorWithoutInvokingOperators)
		{
			StrictMock<MockFunction<int(int)>>		  mapper;
			StrictMock<MockFunction<Maybe<int>(int)>> flatMapper;
			StrictMock<MockFunction<bool(int)>>		  predicate;
			EXPECT_CALL(observer, error(failure));
			subscribe(Maybe<int>::error(failure).map(mapper.AsStdFunction()).flatMap(flatMapper.AsStdFunction()).filter(predicate.AsStdFunction()));
		}

		TEST_F(MaybeTest, ConvertsSuccessToObservable)
		{
			InSequence sequence;
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::just(7).toObservable());
		}

		TEST_F(MaybeTest, ConvertsEmptyToObservable)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::empty().toObservable());
		}

		TEST_F(MaybeTest, ConvertsErrorToObservable)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Maybe<int>::error(failure).toObservable());
		}

		TEST_F(MaybeTest, ConvertsSuccessToSingle)
		{
			EXPECT_CALL(observer, next(7));
			subscribe(Maybe<int>::just(7).toSingle());
		}

		TEST_F(MaybeTest, RejectsEmptyConversionToSingle)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::out_of_range>("toSingle: expected")));
			subscribe(Maybe<int>::empty().toSingle());
		}

		TEST_F(MaybeTest, PreservesErrorInSingleConversion)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Maybe<int>::error(failure).toSingle());
		}

		TEST_F(MaybeTest, SuppliesDefaultForEmptySource)
		{
			EXPECT_CALL(observer, next(42));
			subscribe(Maybe<int>::empty().defaultIfEmpty(42));
		}

		TEST_F(MaybeTest, PreservesPresentValueInsteadOfDefault)
		{
			EXPECT_CALL(observer, next(7));
			subscribe(Maybe<int>::just(7).defaultIfEmpty(42));
		}

		TEST_F(MaybeTest, DoesNotReplaceErrorWithDefault)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Maybe<int>::error(failure).defaultIfEmpty(42));
		}

		TEST_F(MaybeTest, IgnoresSuccessValue)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::just(7).ignoreElement());
		}

		TEST_F(MaybeTest, IgnoresEmptySource)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::empty().ignoreElement());
		}

		TEST_F(MaybeTest, IgnoreElementPreservesError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Maybe<int>::error(failure).ignoreElement());
		}
	} // namespace
} // namespace rppw::test
