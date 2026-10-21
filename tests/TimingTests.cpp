// tests
#include <support/RunLoopTest.h>

// rpp
#include <rpp/schedulers/test_scheduler.hpp>

// stl
#include <chrono>
#include <limits>

namespace rppw::test
{
	namespace
	{
		using ::testing::InSequence;
		using ::testing::MockFunction;
		using ::testing::StrictMock;

		template <typename Source>
		class TimingTest : public RunLoopTest
		{
		protected:
			Source success()
			{
				if constexpr (std::same_as<Source, Completable>)
					return Source::complete();
				else
					return Source::just(7);
			}

			void expectValues(int count = 1)
			{
				if constexpr (!std::same_as<Source, Completable>)
					EXPECT_CALL(observer, next(7)).Times(count);
			}

			void expectSuccess()
			{
				if constexpr (std::same_as<Source, Completable>)
					EXPECT_CALL(observer, complete());
				else
					expectValues();
			}

			rpp::schedulers::test_scheduler clock;
			static constexpr auto			duration = std::chrono::milliseconds{10};
		};

		using TimingSources = ::testing::Types<Single<int>, Maybe<int>, Completable>;
		TYPED_TEST_SUITE(TimingTest, TimingSources);

		TYPED_TEST(TimingTest, DelayDefersSuccessUntilExactDeadline)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			const auto							  source = TypeParam::defer(factory.AsStdFunction()).delay(this->duration, this->clock);
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(this->success()));
			const auto subscription = this->subscribe(source);
			EXPECT_FALSE(subscription.isDisposed());
			this->clock.time_advance(this->duration - std::chrono::nanoseconds{1});
			this->expectSuccess();
			this->clock.time_advance(std::chrono::nanoseconds{1});
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(TimingTest, DelayPreservesOriginalErrorAtDeadline)
		{
			const auto subscription = this->subscribe(TypeParam::error(this->failure).delay(this->duration, this->clock));
			EXPECT_FALSE(subscription.isDisposed());
			this->clock.time_advance(this->duration - std::chrono::nanoseconds{1});
			EXPECT_CALL(this->observer, error(this->failure));
			this->clock.time_advance(std::chrono::nanoseconds{1});
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(TimingTest, DelayZeroPreservesSuccess)
		{
			this->expectSuccess();
			EXPECT_TRUE(this->subscribe(this->success().delay(std::chrono::nanoseconds{0}, this->clock)).isDisposed());
		}

		TYPED_TEST(TimingTest, DelayRejectsNegativeDurationBeforeSubscription)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_THROW(TypeParam::defer(factory.AsStdFunction()).delay(std::chrono::nanoseconds{-1}, this->clock), std::invalid_argument);
			EXPECT_TRUE(this->clock.get_schedulings().empty());
		}

		TYPED_TEST(TimingTest, DelayCancellationDiscardsQueuedSuccess)
		{
			const auto subscription = this->subscribe(this->success().delay(this->duration, this->clock));
			subscription.dispose();
			this->clock.time_advance(this->duration);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(TimingTest, DelayCancellationCancelsScheduledProducer)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			const auto							  source = TypeParam::defer(factory.AsStdFunction()).subscribeOn(this->loop).delay(this->duration, this->clock);
			const auto							  subscription = this->subscribe(source);
			subscription.dispose();
			this->drain();
			this->clock.time_advance(this->duration);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(TimingTest, DelaySchedulesSubscribersIndependently)
		{
			const auto source = this->success().delay(this->duration, this->clock);
			const auto first = this->subscribe(source);
			this->clock.time_advance(this->duration / 2);
			const auto second = this->subscribe(source);
			this->expectSuccess();
			this->clock.time_advance(this->duration / 2);
			EXPECT_TRUE(first.isDisposed());
			EXPECT_FALSE(second.isDisposed());
			this->expectSuccess();
			this->clock.time_advance(this->duration / 2);
			EXPECT_TRUE(second.isDisposed());
		}

		TYPED_TEST(TimingTest, RepeatZeroDoesNotSubscribe)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_CALL(this->observer, complete());
			EXPECT_TRUE(this->subscribe(TypeParam::defer(factory.AsStdFunction()).repeat(0)).isDisposed());
		}

		TYPED_TEST(TimingTest, RepeatOneUsesSingleSubscription)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(this->success()));
			InSequence sequence;
			this->expectValues();
			EXPECT_CALL(this->observer, complete());
			this->subscribe(TypeParam::defer(factory.AsStdFunction()).repeat(1));
		}

		TYPED_TEST(TimingTest, RepeatDefersAndResetsForEverySubscriber)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			const auto							  source = TypeParam::defer(factory.AsStdFunction()).repeat(3);
			EXPECT_CALL(factory, Call()).Times(6).WillRepeatedly(::testing::Return(this->success()));
			this->expectValues(6);
			EXPECT_CALL(this->observer, complete()).Times(2);
			this->subscribe(source);
			this->subscribe(source);
		}

		TYPED_TEST(TimingTest, RepeatStopsOnFirstError)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(TypeParam::error(this->failure)));
			EXPECT_CALL(this->observer, error(this->failure));
			this->subscribe(TypeParam::defer(factory.AsStdFunction()).repeat(3));
		}

		TYPED_TEST(TimingTest, UnlimitedRepeatRemainsCancellable)
		{
			int		   attempts = 0;
			const auto source = TypeParam::defer(
									[&]
									{
										++attempts;
										return this->success();
									})
									.subscribeOn(this->loop)
									.repeat();
			this->expectValues(3);
			const auto subscription = this->subscribe(source);
			this->dispatchUntil(
				[&]
				{
					return attempts == 3;
				});
			subscription.dispose();
			this->drain();
			EXPECT_EQ(attempts, 3);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(TimingTest, RepeatCancellationPreventsInitialSubscription)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			const auto							  subscription = this->subscribe(TypeParam::defer(factory.AsStdFunction()).subscribeOn(this->loop).repeat(3));
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(TimingTest, RetryZeroUsesOnlyInitialAttempt)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(TypeParam::error(this->failure)));
			EXPECT_CALL(this->observer, error(this->failure));
			this->subscribe(TypeParam::defer(factory.AsStdFunction()).retry(0));
		}

		TYPED_TEST(TimingTest, RetryExhaustionForwardsLastError)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_CALL(factory, Call())
				.WillOnce(::testing::Return(TypeParam::error(std::runtime_error{"first"})))
				.WillOnce(::testing::Return(TypeParam::error(std::runtime_error{"second"})))
				.WillOnce(::testing::Return(TypeParam::error(this->failure)));
			EXPECT_CALL(this->observer, error(this->failure));
			EXPECT_TRUE(this->subscribe(TypeParam::defer(factory.AsStdFunction()).retry(2)).isDisposed());
		}

		TYPED_TEST(TimingTest, RetryRecoversOnLastAllowedAttempt)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_CALL(factory, Call())
				.WillOnce(::testing::Return(TypeParam::error(this->failure)))
				.WillOnce(::testing::Return(TypeParam::error(this->failure)))
				.WillOnce(::testing::Return(this->success()));
			this->expectSuccess();
			EXPECT_TRUE(this->subscribe(TypeParam::defer(factory.AsStdFunction()).retry(2)).isDisposed());
		}

		TYPED_TEST(TimingTest, RetryDoesNotRepeatSuccess)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(this->success()));
			this->expectSuccess();
			this->subscribe(TypeParam::defer(factory.AsStdFunction()).retry(3));
		}

		TYPED_TEST(TimingTest, RetryDefersAndResetsForEverySubscriber)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			const auto							  source = TypeParam::defer(factory.AsStdFunction()).retry(1);
			EXPECT_CALL(factory, Call()).Times(4).WillRepeatedly(::testing::Return(TypeParam::error(this->failure)));
			EXPECT_CALL(this->observer, error(this->failure)).Times(2);
			this->subscribe(source);
			this->subscribe(source);
		}

		TYPED_TEST(TimingTest, UnlimitedRetryRecoversFromFactoryExceptions)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_CALL(factory, Call())
				.WillOnce(::testing::Throw(std::runtime_error{"factory"}))
				.WillOnce(::testing::Return(TypeParam::error(this->failure)))
				.WillOnce(::testing::Return(this->success()));
			this->expectSuccess();
			this->subscribe(TypeParam::defer(factory.AsStdFunction()).retry());
		}

		TYPED_TEST(TimingTest, UnlimitedRetryRemainsCancellable)
		{
			int		   attempts = 0;
			const auto source = TypeParam::defer(
									[&]
									{
										++attempts;
										return TypeParam::error(this->failure);
									})
									.subscribeOn(this->loop)
									.retry();
			const auto subscription = this->subscribe(source);
			this->dispatchUntil(
				[&]
				{
					return attempts == 3;
				});
			subscription.dispose();
			this->drain();
			EXPECT_EQ(attempts, 3);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(TimingTest, RetryCancellationPreventsInitialSubscription)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			const auto							  subscription = this->subscribe(TypeParam::defer(factory.AsStdFunction()).subscribeOn(this->loop).retry(3));
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(TimingTest, MaximumRetryCountAllowsRecovery)
		{
			StrictMock<MockFunction<TypeParam()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(TypeParam::error(this->failure))).WillOnce(::testing::Return(this->success()));
			this->expectSuccess();
			this->subscribe(TypeParam::defer(factory.AsStdFunction()).retry(std::numeric_limits<std::size_t>::max()));
		}

		class MaybeTimingTest : public RunLoopTest
		{
		};

		TEST_F(MaybeTimingTest, DelaysEmptyCompletionUntilDeadline)
		{
			rpp::schedulers::test_scheduler clock;
			const auto						duration = std::chrono::milliseconds{10};
			const auto						subscription = subscribe(Maybe<int>::empty().delay(duration, clock));
			EXPECT_FALSE(subscription.isDisposed());
			clock.time_advance(duration - std::chrono::nanoseconds{1});
			EXPECT_CALL(observer, complete());
			clock.time_advance(std::chrono::nanoseconds{1});
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(MaybeTimingTest, RepeatConsumesEmptyCompletions)
		{
			StrictMock<MockFunction<Maybe<int>()>> factory;
			EXPECT_CALL(factory, Call())
				.WillOnce(::testing::Return(Maybe<int>::empty()))
				.WillOnce(::testing::Return(Maybe<int>::just(7)))
				.WillOnce(::testing::Return(Maybe<int>::empty()));
			InSequence sequence;
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::defer(factory.AsStdFunction()).repeat(3));
		}

		TEST_F(MaybeTimingTest, RetryRecoversWithEmptyCompletion)
		{
			StrictMock<MockFunction<Maybe<int>()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Maybe<int>::error(failure))).WillOnce(::testing::Return(Maybe<int>::empty()));
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::defer(factory.AsStdFunction()).retry(2));
		}

		TEST_F(MaybeTimingTest, UnlimitedRetryDoesNotRetryEmptyCompletion)
		{
			StrictMock<MockFunction<Maybe<int>()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Maybe<int>::empty()));
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::defer(factory.AsStdFunction()).retry());
		}
	} // namespace
} // namespace rppw::test
