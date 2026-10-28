// tests
#include <support/RunLoopTest.h>

// stl
#include <array>
#include <memory>

namespace rppw::test
{
	namespace
	{
		using ::testing::InSequence;
		using ::testing::MockFunction;
		using ::testing::StrictMock;

		class DisposableTest : public RunLoopTest
		{
		};

		TEST_F(DisposableTest, DefaultHandleCanBeDisposedRepeatedly)
		{
			const Disposable subscription;
			EXPECT_NO_THROW(subscription.dispose());
			EXPECT_NO_THROW(subscription.dispose());
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(DisposableTest, ReportsTerminationAfterSuccess)
		{
			EXPECT_CALL(observer, next(7));
			const auto subscription = subscribe(Single<int>::just(7));
			EXPECT_TRUE(subscription.isDisposed());
			EXPECT_NO_THROW(subscription.dispose());
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(DisposableTest, CreatedTokenStaysActiveUntilDisposed)
		{
			const auto token = Disposable::create();
			EXPECT_FALSE(token.isDisposed());
			token.dispose();
			token.dispose();
			EXPECT_TRUE(token.isDisposed());
		}

		TEST_F(DisposableTest, CreatedTokensCancelIndependently)
		{
			const auto first = Disposable::create();
			const auto second = Disposable::create();
			first.dispose();
			EXPECT_TRUE(first.isDisposed());
			EXPECT_FALSE(second.isDisposed());
		}

		TEST_F(DisposableTest, CreatedTokenCopiesShareCancellation)
		{
			const auto token = Disposable::create();
			const auto copy = token;
			copy.dispose();
			EXPECT_TRUE(token.isDisposed());
		}

		TEST_F(DisposableTest, DestroyedTokenCopyDoesNotCancelToken)
		{
			const auto token = Disposable::create();
			{
				const auto copy = token;
			}
			EXPECT_FALSE(token.isDisposed());
		}

		TEST_F(DisposableTest, ReportsTerminationAfterError)
		{
			EXPECT_CALL(observer, error(failure));
			const auto subscription = subscribe(Single<int>::error(failure));
			EXPECT_TRUE(subscription.isDisposed());
			subscription.dispose();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(DisposableTest, ReportsTerminationAfterEmptyCompletion)
		{
			EXPECT_CALL(observer, complete());
			const auto subscription = subscribe(Maybe<int>::empty());
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(DisposableTest, CancelsBeforeSourceExecution)
		{
			StrictMock<MockFunction<int()>> callable;
			const auto						subscription = subscribe(Single<int>::fromCallable(callable.AsStdFunction()).subscribeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			subscription.dispose();
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(DisposableTest, CopiesShareCancellation)
		{
			const auto first = subscribe(Observable<int>::just(7).subscribeOn(loop));
			const auto second = first;
			second.dispose();
			EXPECT_TRUE(first.isDisposed());
			EXPECT_TRUE(second.isDisposed());
			drain();
		}

		TEST_F(DisposableTest, DistinctSubscriptionsCancelIndependently)
		{
			const auto source = Observable<int>::just(7).subscribeOn(loop);
			const auto first = subscribe(source);
			const auto second = subscribe(source);
			first.dispose();
			EXPECT_FALSE(second.isDisposed());
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			drain();
			EXPECT_TRUE(second.isDisposed());
		}

		TEST_F(DisposableTest, CancelsDuringSynchronousFlow)
		{
			Disposable subscription;
			subscription = Observable<int>::fromIterable(std::array{1, 2, 3})
							   .subscribeOn(loop)
							   .subscribe(
								   [&](int value)
								   {
									   observer.next(value);
									   subscription.dispose();
								   },
								   [this](std::exception_ptr error)
								   {
									   observer.error(error);
								   },
								   [this]
								   {
									   observer.complete();
								   });
			EXPECT_CALL(observer, next(1));
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(DisposableTest, DestroyedHandleDoesNotCancelSubscription)
		{
			{
				const auto subscription = subscribe(Observable<int>::just(7).subscribeOn(loop));
				EXPECT_FALSE(subscription.isDisposed());
			}
			InSequence sequence;
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			drain();
		}

		TEST_F(DisposableTest, DestroyingOneCopyDoesNotCancelOtherCopy)
		{
			const auto subscription = subscribe(Single<int>::just(7).subscribeOn(loop));
			{
				const auto copy = subscription;
				EXPECT_FALSE(copy.isDisposed());
			}
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(observer, next(7));
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(DisposableTest, RetainsCallbackCaptureUntilTermination)
		{
			auto					 resource = std::make_shared<int>(7);
			const std::weak_ptr<int> weak = resource;
			const auto				 subscription = Single<int>::just(1).subscribeOn(loop).subscribe(
				  [this, resource](int)
				  {
					  observer.next(*resource);
				  },
				  [this](std::exception_ptr error)
				  {
					  observer.error(error);
				  });
			resource.reset();
			EXPECT_FALSE(weak.expired());
			EXPECT_CALL(observer, next(7));
			drain();
			EXPECT_TRUE(weak.expired());
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(DisposableTest, ReleasesCallbackCaptureAfterCancellation)
		{
			auto					 resource = std::make_shared<int>(7);
			const std::weak_ptr<int> weak = resource;
			const auto				 subscription = Single<int>::just(1).subscribeOn(loop).subscribe(
				  [this, resource](int)
				  {
					  observer.next(*resource);
				  },
				  [this](std::exception_ptr error)
				  {
					  observer.error(error);
				  });
			resource.reset();
			EXPECT_FALSE(weak.expired());
			subscription.dispose();
			drain();
			EXPECT_TRUE(weak.expired());
		}

		TEST_F(DisposableTest, ReleasesCallbackCaptureAfterError)
		{
			auto					 resource = std::make_shared<int>(7);
			const std::weak_ptr<int> weak = resource;
			const auto				 subscription = Single<int>::error(failure).subscribeOn(loop).subscribe(
				  [this, resource](int)
				  {
					  observer.next(*resource);
				  },
				  [this](std::exception_ptr error)
				  {
					  observer.error(error);
				  });
			resource.reset();
			EXPECT_CALL(observer, error(failure));
			drain();
			EXPECT_TRUE(weak.expired());
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(DisposableTest, RetainsOwnedValueUntilSourceIsDestroyed)
		{
			auto					 resource = std::make_shared<int>(7);
			const std::weak_ptr<int> weak = resource;
			EXPECT_CALL(observer, next(7));
			{
				const auto source = Single<std::shared_ptr<int>>::just(resource);
				resource.reset();
				source.subscribe(
					[this](const std::shared_ptr<int> &value)
					{
						ASSERT_NE(value, nullptr);
						observer.next(*value);
					},
					[this](std::exception_ptr error)
					{
						observer.error(error);
					});
				EXPECT_FALSE(weak.expired());
			}
			EXPECT_TRUE(weak.expired());
		}
	} // namespace
} // namespace rppw::test
