// googletest
#include <gtest/gtest.h>

// rppw
#include <rppw/ReactivePlusPlusWrapper.h>

// rpp
#include <rpp/schedulers/run_loop.hpp>

// stl
#include <array>
#include <concepts>
#include <stdexcept>

// tests
#include <support/RunLoopTest.h>

namespace rppw::test
{
	namespace
	{
		template <typename Source>
		class ParameterlessSubscribeTest : public RunLoopTest
		{
		protected:
			static Source success()
			{
				if constexpr (std::same_as<Source, Completable>)
					return Completable::complete();
				else
					return Source::just(7);
			}
		};

		using SubscribeSources = ::testing::Types<Observable<int>, Single<int>, Maybe<int>, Completable>;
		TYPED_TEST_SUITE(ParameterlessSubscribeTest, SubscribeSources);

		TYPED_TEST(ParameterlessSubscribeTest, ExecutesSourceForEachSubscription)
		{
			int		   executions = 0;
			const auto source = TypeParam::defer(
				[&]
				{
					++executions;
					return TestFixture::success();
				});
			EXPECT_EQ(executions, 0);
			EXPECT_TRUE(source.subscribe().isDisposed());
			EXPECT_TRUE(source.subscribe().isDisposed());
			EXPECT_EQ(executions, 2);
		}

		TYPED_TEST(ParameterlessSubscribeTest, LogsStandardError)
		{
			::testing::internal::CaptureStderr();
			const auto subscription = TypeParam::error(std::runtime_error{"subscription failed"}).subscribe();
			const auto log = ::testing::internal::GetCapturedStderr();
			EXPECT_EQ(log, "rppw::subscribe(): unhandled error: subscription failed\n");
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ParameterlessSubscribeTest, LogsNonStandardError)
		{
			::testing::internal::CaptureStderr();
			const auto subscription = TypeParam::error(42).subscribe();
			const auto log = ::testing::internal::GetCapturedStderr();
			EXPECT_EQ(log, "rppw::subscribe(): unhandled error: non-standard exception\n");
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ParameterlessSubscribeTest, LogsAsynchronousError)
		{
			const auto source = TypeParam::error(std::runtime_error{"async failure"}).subscribeOn(this->loop);
			::testing::internal::CaptureStderr();
			const auto subscription = source.subscribe();
			EXPECT_FALSE(subscription.isDisposed());
			this->drain();
			const auto log = ::testing::internal::GetCapturedStderr();
			EXPECT_EQ(log, "rppw::subscribe(): unhandled error: async failure\n");
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ParameterlessSubscribeTest, CancelsBeforeSourceExecution)
		{
			int		   executions = 0;
			const auto source = TypeParam::defer(
									[&]
									{
										++executions;
										return TestFixture::success();
									})
									.subscribeOn(this->loop);
			const auto subscription = source.subscribe();
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			this->drain();
			EXPECT_EQ(executions, 0);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ParameterlessSubscribeTest, DestroyedHandleDoesNotCancelProcessing)
		{
			int		   completions = 0;
			const auto source = TestFixture::success()
									.doOnComplete(
										[&]
										{
											++completions;
										})
									.subscribeOn(this->loop);
			{
				const auto subscription = source.subscribe();
				EXPECT_FALSE(subscription.isDisposed());
			}
			this->drain();
			EXPECT_EQ(completions, 1);
		}

		class ParameterlessSubscribeEdgeTest : public ::testing::Test
		{
		};

		TEST_F(ParameterlessSubscribeEdgeTest, ConsumesEveryObservableValue)
		{
			int		   values = 0;
			const auto subscription = Observable<int>::fromIterable(std::array{1, 2, 3})
										  .doOnNext(
											  [&](int)
											  {
												  ++values;
											  })
										  .subscribe();
			EXPECT_EQ(values, 3);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ParameterlessSubscribeEdgeTest, CompletesEmptyObservable)
		{
			EXPECT_TRUE(Observable<int>::empty().subscribe().isDisposed());
		}

		TEST_F(ParameterlessSubscribeEdgeTest, CompletesEmptyMaybe)
		{
			EXPECT_TRUE(Maybe<int>::empty().subscribe().isDisposed());
		}
	} // namespace
} // namespace rppw::test
