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

		template <typename Source>
		class FallbackTest : public RunLoopTest
		{
		protected:
			void expectValue(int value)
			{
				EXPECT_CALL(observer, next(value));
				if constexpr (std::same_as<Source, Observable<int>>)
					EXPECT_CALL(observer, complete());
			}
		};

		using FallbackSources = ::testing::Types<Observable<int>, Maybe<int>>;
		TYPED_TEST_SUITE(FallbackTest, FallbackSources);

		TYPED_TEST(FallbackTest, SwitchesToFallbackOnEmptyCompletion)
		{
			this->expectValue(7);
			EXPECT_TRUE(this->subscribe(TypeParam::empty().switchIfEmpty(TypeParam::just(7))).isDisposed());
		}

		TYPED_TEST(FallbackTest, SkipsFallbackAfterValue)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			this->expectValue(1);
			this->subscribe(TypeParam::just(1).switchIfEmpty(TypeParam::defer(factory.AsStdFunction())));
		}

		TYPED_TEST(FallbackTest, SkipsFallbackOnError)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_CALL(this->observer, error(this->failure));
			this->subscribe(TypeParam::error(this->failure).switchIfEmpty(TypeParam::defer(factory.AsStdFunction())));
		}

		TYPED_TEST(FallbackTest, ForwardsFallbackError)
		{
			EXPECT_CALL(this->observer, error(this->failure));
			EXPECT_TRUE(this->subscribe(TypeParam::empty().switchIfEmpty(TypeParam::error(this->failure))).isDisposed());
		}

		TYPED_TEST(FallbackTest, CompletesWhenBothSourcesAreEmpty)
		{
			EXPECT_CALL(this->observer, complete());
			EXPECT_TRUE(this->subscribe(TypeParam::empty().switchIfEmpty(TypeParam::empty())).isDisposed());
		}

		TYPED_TEST(FallbackTest, DefersFactoriesUntilSubscription)
		{
			StrictMock<MockFunction<TypeParam()>> first;
			StrictMock<MockFunction<TypeParam()>> fallback;
			const auto							  source = TypeParam::defer(first.AsStdFunction()).switchIfEmpty(TypeParam::defer(fallback.AsStdFunction()));
			InSequence							  sequence;
			EXPECT_CALL(first, Call()).WillOnce(::testing::Return(TypeParam::empty()));
			EXPECT_CALL(fallback, Call()).WillOnce(::testing::Return(TypeParam::just(7)));
			this->expectValue(7);
			this->subscribe(source);
		}

		TYPED_TEST(FallbackTest, WaitsForEmptyCompletionBeforeSubscribingFallback)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			const auto							  source = TypeParam::empty().observeOn(this->loop).switchIfEmpty(TypeParam::defer(factory.AsStdFunction()));
			const auto							  subscription = this->subscribe(source);
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(TypeParam::just(7)));
			this->expectValue(7);
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(FallbackTest, ResetsStateAfterPresentSubscription)
		{
			StrictMock<MockFunction<TypeParam()>> first;
			StrictMock<MockFunction<TypeParam()>> fallback;
			const auto							  source = TypeParam::defer(first.AsStdFunction()).switchIfEmpty(TypeParam::defer(fallback.AsStdFunction()));
			InSequence							  sequence;
			EXPECT_CALL(first, Call()).WillOnce(::testing::Return(TypeParam::just(1)));
			this->expectValue(1);
			EXPECT_CALL(first, Call()).WillOnce(::testing::Return(TypeParam::empty()));
			EXPECT_CALL(fallback, Call()).WillOnce(::testing::Return(TypeParam::just(7)));
			this->expectValue(7);
			this->subscribe(source);
			this->subscribe(source);
		}

		TYPED_TEST(FallbackTest, RestartsFallbackForEveryEmptySubscription)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			const auto							  source = TypeParam::empty().switchIfEmpty(TypeParam::defer(factory.AsStdFunction()));
			InSequence							  sequence;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(TypeParam::just(1)));
			this->expectValue(1);
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(TypeParam::just(7)));
			this->expectValue(7);
			this->subscribe(source);
			this->subscribe(source);
		}

		TYPED_TEST(FallbackTest, PropagatesFallbackFactoryException)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Throw(std::runtime_error{"fallback"}));
			EXPECT_CALL(this->observer, error(ExceptionContaining<std::runtime_error>("fallback")));
			this->subscribe(TypeParam::empty().switchIfEmpty(TypeParam::defer(factory.AsStdFunction())));
		}

		TYPED_TEST(FallbackTest, CancellationPreventsInitialAndFallbackSubscriptions)
		{
			StrictMock<MockFunction<TypeParam()>> first;
			StrictMock<MockFunction<TypeParam()>> fallback;
			const auto source = TypeParam::defer(first.AsStdFunction()).subscribeOn(this->loop).switchIfEmpty(TypeParam::defer(fallback.AsStdFunction()));
			const auto subscription = this->subscribe(source);
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(FallbackTest, CancellationPreventsSwitchAfterQueuedCompletion)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			const auto							  source = TypeParam::empty().observeOn(this->loop).switchIfEmpty(TypeParam::defer(factory.AsStdFunction()));
			const auto							  subscription = this->subscribe(source);
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(FallbackTest, CancellationCancelsScheduledFallback)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			const auto							  fallback = TypeParam::defer(factory.AsStdFunction()).subscribeOn(this->loop);
			const auto							  subscription = this->subscribe(TypeParam::empty().switchIfEmpty(fallback));
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		class ObservableFallbackTest : public RunLoopTest
		{
		};

		TEST_F(ObservableFallbackTest, PreservesAllSourceValuesInOrder)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			InSequence									sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}).switchIfEmpty(Observable<int>::defer(factory.AsStdFunction())));
		}

		TEST_F(ObservableFallbackTest, PreservesAllFallbackValuesInOrder)
		{
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::empty().switchIfEmpty(Observable<int>::fromIterable(std::array{1, 2, 3})));
		}

		TEST_F(ObservableFallbackTest, ForwardsValueWithoutWaitingForCompletion)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto first = Observable<int>::concat(std::array{Observable<int>::just(1), Observable<int>::empty().subscribeOn(loop)});
			EXPECT_CALL(observer, next(1));
			const auto subscription = subscribe(first.switchIfEmpty(Observable<int>::defer(factory.AsStdFunction())));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(observer, complete());
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableFallbackTest, DoesNotSwitchAfterValueThenError)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto									first = Observable<int>::concat(std::array{Observable<int>::just(1), Observable<int>::error(failure)});
			InSequence									sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, error(failure));
			subscribe(first.switchIfEmpty(Observable<int>::defer(factory.AsStdFunction())));
		}

		TEST_F(ObservableFallbackTest, DownstreamCancellationStopsFallbackProduction)
		{
			StrictMock<MockFunction<void(int)>> produced;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, complete());
			const auto fallback = Observable<int>::fromIterable(std::array{1, 2, 3}).doOnNext(produced.AsStdFunction());
			subscribe(Observable<int>::empty().switchIfEmpty(fallback).take(1));
		}

		TEST_F(ObservableFallbackTest, DownstreamCancellationPreventsSwitch)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			StrictMock<MockFunction<void(int)>>			produced;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, complete());
			const auto source = Observable<int>::fromIterable(std::array{1, 2, 3}).doOnNext(produced.AsStdFunction());
			subscribe(source.switchIfEmpty(Observable<int>::defer(factory.AsStdFunction())).take(1));
		}
	} // namespace
} // namespace rppw::test
