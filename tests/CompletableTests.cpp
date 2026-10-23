// tests
#include <support/RunLoopTest.h>

// stl
#include <array>

namespace rppw::test
{
	namespace
	{
		using ::testing::InSequence;
		using ::testing::MockFunction;
		using ::testing::StrictMock;

		class CompletableTest : public ReactiveTest
		{
		};

		template <typename Case>
		class CompletableConversionTest : public RunLoopTest
		{
		protected:
			auto convert(const Completable &source)
			{
				if constexpr (Case::value)
					return source.toMaybe<int>();
				else
					return source.toObservable<int>();
			}
		};

		using CompletableConversionCases = ::testing::Types<std::false_type, std::true_type>;
		TYPED_TEST_SUITE(CompletableConversionTest, CompletableConversionCases);

		TYPED_TEST(CompletableConversionTest, CompletesWithoutEmittingValue)
		{
			EXPECT_CALL(this->observer, complete());
			EXPECT_TRUE(this->subscribe(this->convert(Completable::complete())).isDisposed());
		}

		TYPED_TEST(CompletableConversionTest, PreservesOriginalError)
		{
			EXPECT_CALL(this->observer, error(this->failure));
			EXPECT_TRUE(this->subscribe(this->convert(Completable::error(this->failure))).isDisposed());
		}

		TYPED_TEST(CompletableConversionTest, DefersActionForEverySubscription)
		{
			StrictMock<MockFunction<void()>> action;
			const auto						 source = this->convert(Completable::fromAction(action.AsStdFunction()));
			InSequence						 sequence;
			EXPECT_CALL(action, Call());
			EXPECT_CALL(this->observer, complete());
			EXPECT_CALL(action, Call());
			EXPECT_CALL(this->observer, complete());
			this->subscribe(source);
			this->subscribe(source);
		}

		TYPED_TEST(CompletableConversionTest, PropagatesActionException)
		{
			const auto source = this->convert(Completable::fromAction(
				[]
				{
					throw std::runtime_error{"action"};
				}));
			EXPECT_CALL(this->observer, error(ExceptionContaining<std::runtime_error>("action")));
			this->subscribe(source);
		}

		TYPED_TEST(CompletableConversionTest, PropagatesCancellationToScheduledAction)
		{
			StrictMock<MockFunction<void()>> action;
			const auto subscription = this->subscribe(this->convert(Completable::fromAction(action.AsStdFunction()).subscribeOn(this->loop)));
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(CompletableConversionTest, WaitsForScheduledCompletion)
		{
			const auto subscription = this->subscribe(this->convert(Completable::complete().observeOn(this->loop)));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(this->observer, complete());
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(CompletableConversionTest, PreservesScheduledError)
		{
			const auto subscription = this->subscribe(this->convert(Completable::error(this->failure).observeOn(this->loop)));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(this->observer, error(this->failure));
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		struct NonDefaultValue
		{
			explicit NonDefaultValue(int) {}
		};

		TEST_F(CompletableTest, ConvertsToObservableWithoutConstructingValue)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Completable::complete().toObservable<NonDefaultValue>().ignoreElements());
		}

		TEST_F(CompletableTest, ConvertsToMaybeWithoutConstructingValue)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Completable::complete().toMaybe<NonDefaultValue>().toObservable().ignoreElements());
		}

		TEST_F(CompletableTest, DefaultObservableConversionEmitsNoMonostate)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Completable::complete().toObservable().map(
				[](std::monostate)
				{
					return 1;
				}));
		}

		TEST_F(CompletableTest, DefaultMaybeConversionEmitsNoMonostate)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Completable::complete().toMaybe().map(
				[](std::monostate)
				{
					return 1;
				}));
		}

		TEST_F(CompletableTest, CompletesWithoutValues)
		{
			EXPECT_CALL(observer, complete());
			EXPECT_TRUE(subscribe(Completable::complete()).isDisposed());
		}

		TEST_F(CompletableTest, ForwardsOriginalError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Completable::error(failure));
		}

		TEST_F(CompletableTest, RejectsNullErrorSynchronously)
		{
			EXPECT_THROW(Completable::error({}), std::invalid_argument);
		}

		TEST_F(CompletableTest, ExecutesActionLazilyForEverySubscription)
		{
			StrictMock<MockFunction<void()>> action;
			const auto						 source = Completable::fromAction(action.AsStdFunction());
			InSequence						 sequence;
			EXPECT_CALL(action, Call());
			EXPECT_CALL(observer, complete());
			EXPECT_CALL(action, Call());
			EXPECT_CALL(observer, complete());
			subscribe(source);
			subscribe(source);
		}

		TEST_F(CompletableTest, RoutesActionExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("action")));
			EXPECT_NO_THROW(subscribe(Completable::fromAction(
				[]
				{
					throw std::runtime_error{"action"};
				})));
		}

		TEST_F(CompletableTest, InvokesFactoryLazilyForEverySubscription)
		{
			StrictMock<MockFunction<void()>> factory;
			const auto						 source = Completable::defer(
				  [&]
				  {
					  factory.Call();
					  return Completable::complete();
				  });
			InSequence sequence;
			EXPECT_CALL(factory, Call());
			EXPECT_CALL(observer, complete());
			EXPECT_CALL(factory, Call());
			EXPECT_CALL(observer, complete());
			subscribe(source);
			subscribe(source);
		}

		TEST_F(CompletableTest, RoutesFactoryExceptionToError)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("factory")));
			subscribe(Completable::defer(
				[]() -> Completable
				{
					throw std::runtime_error{"factory"};
				}));
		}

		TEST_F(CompletableTest, SubscribesObservableOnlyAfterFirstAction)
		{
			StrictMock<MockFunction<void()>> first;
			StrictMock<MockFunction<void()>> second;
			const auto						 source = Completable::fromAction(first.AsStdFunction())
									.andThen(Observable<int>::defer(
										[&]
										{
											second.Call();
											return Observable<int>::just(7);
										}));
			InSequence sequence;
			EXPECT_CALL(first, Call());
			EXPECT_CALL(second, Call());
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(source);
		}

		TEST_F(CompletableTest, SubscribesSingleOnlyAfterFirstAction)
		{
			StrictMock<MockFunction<void()>> first;
			StrictMock<MockFunction<int()>>	 second;
			const auto						 source = Completable::fromAction(first.AsStdFunction()).andThen(Single<int>::fromCallable(second.AsStdFunction()));
			InSequence						 sequence;
			EXPECT_CALL(first, Call());
			EXPECT_CALL(second, Call()).WillOnce(::testing::Return(7));
			EXPECT_CALL(observer, next(7));
			subscribe(source);
		}

		TEST_F(CompletableTest, SubscribesMaybeOnlyAfterFirstAction)
		{
			StrictMock<MockFunction<void()>>			   first;
			StrictMock<MockFunction<std::optional<int>()>> second;
			const auto source = Completable::fromAction(first.AsStdFunction()).andThen(Maybe<int>::fromCallable(second.AsStdFunction()));
			InSequence sequence;
			EXPECT_CALL(first, Call());
			EXPECT_CALL(second, Call()).WillOnce(::testing::Return(std::optional{7}));
			EXPECT_CALL(observer, next(7));
			subscribe(source);
		}

		TEST_F(CompletableTest, SubscribesCompletableOnlyAfterFirstAction)
		{
			StrictMock<MockFunction<void()>> first;
			StrictMock<MockFunction<void()>> second;
			const auto						 source = Completable::fromAction(first.AsStdFunction()).andThen(Completable::fromAction(second.AsStdFunction()));
			InSequence						 sequence;
			EXPECT_CALL(first, Call());
			EXPECT_CALL(second, Call());
			EXPECT_CALL(observer, complete());
			subscribe(source);
		}

		TEST_F(CompletableTest, PreservesAllObservableValues)
		{
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			subscribe(Completable::complete().andThen(Observable<int>::fromIterable(std::array{1, 2})));
		}

		TEST_F(CompletableTest, PreservesEmptyMaybeCompletion)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Completable::complete().andThen(Maybe<int>::empty()));
		}

		TEST_F(CompletableTest, ErrorPreventsObservableSubscription)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(observer, error(failure));
			subscribe(Completable::error(failure).andThen(Observable<int>::defer(factory.AsStdFunction())));
		}

		TEST_F(CompletableTest, ErrorPreventsSingleSubscription)
		{
			StrictMock<MockFunction<int()>> callable;
			EXPECT_CALL(observer, error(failure));
			subscribe(Completable::error(failure).andThen(Single<int>::fromCallable(callable.AsStdFunction())));
		}

		TEST_F(CompletableTest, ErrorPreventsMaybeSubscription)
		{
			StrictMock<MockFunction<std::optional<int>()>> callable;
			EXPECT_CALL(observer, error(failure));
			subscribe(Completable::error(failure).andThen(Maybe<int>::fromCallable(callable.AsStdFunction())));
		}

		TEST_F(CompletableTest, ErrorPreventsCompletableSubscription)
		{
			StrictMock<MockFunction<void()>> action;
			EXPECT_CALL(observer, error(failure));
			subscribe(Completable::error(failure).andThen(Completable::fromAction(action.AsStdFunction())));
		}

		TEST_F(CompletableTest, ForwardsSecondObservableError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Completable::complete().andThen(Observable<int>::error(failure)));
		}

		TEST_F(CompletableTest, ForwardsSecondSingleError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Completable::complete().andThen(Single<int>::error(failure)));
		}

		TEST_F(CompletableTest, ForwardsSecondMaybeError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Completable::complete().andThen(Maybe<int>::error(failure)));
		}

		TEST_F(CompletableTest, ForwardsSecondCompletableError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Completable::complete().andThen(Completable::error(failure)));
		}

		TEST_F(CompletableTest, RestartsBothActionsForEverySubscription)
		{
			StrictMock<MockFunction<void(int)>> action;
			const auto							source = Completable::fromAction(
									 [&]
									 {
										 action.Call(1);
									 })
									.andThen(Completable::fromAction(
										[&]
										{
											action.Call(2);
										}));
			InSequence sequence;
			EXPECT_CALL(action, Call(1));
			EXPECT_CALL(action, Call(2));
			EXPECT_CALL(observer, complete());
			EXPECT_CALL(action, Call(1));
			EXPECT_CALL(action, Call(2));
			EXPECT_CALL(observer, complete());
			subscribe(source);
			subscribe(source);
		}
	} // namespace
} // namespace rppw::test
