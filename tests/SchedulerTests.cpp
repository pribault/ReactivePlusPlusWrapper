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

		class SchedulerTest : public RunLoopTest
		{
		};

		TEST_F(SchedulerTest, SubscribeOnDefersObservableFactory)
		{
			StrictMock<MockFunction<void()>> factory;
			const auto						 subscription = subscribe(Observable<int>::defer(
													  [&]
													  {
														  factory.Call();
														  return Observable<int>::just(7);
													  })
																		  .subscribeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			InSequence sequence;
			EXPECT_CALL(factory, Call());
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, SubscribeOnDefersSingleCallable)
		{
			StrictMock<MockFunction<int()>> callable;
			const auto						subscription = subscribe(Single<int>::fromCallable(callable.AsStdFunction()).subscribeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(callable, Call()).WillOnce(::testing::Return(7));
			EXPECT_CALL(observer, next(7));
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, SubscribeOnDefersMaybeCallable)
		{
			StrictMock<MockFunction<std::optional<int>()>> callable;
			const auto									   subscription = subscribe(Maybe<int>::fromCallable(callable.AsStdFunction()).subscribeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(callable, Call()).WillOnce(::testing::Return(std::optional{7}));
			EXPECT_CALL(observer, next(7));
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, SubscribeOnDefersCompletableAction)
		{
			StrictMock<MockFunction<void()>> action;
			const auto						 subscription = subscribe(Completable::fromAction(action.AsStdFunction()).subscribeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			InSequence sequence;
			EXPECT_CALL(action, Call());
			EXPECT_CALL(observer, complete());
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, ObserveOnExecutesProducerBeforeDeliveringObservableValues)
		{
			StrictMock<MockFunction<void()>> producer;
			EXPECT_CALL(producer, Call());
			const auto subscription = subscribe(Observable<int>::defer(
													[&]
													{
														producer.Call();
														return Observable<int>::fromIterable(std::array{1, 2, 3});
													})
													.observeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(observer, complete());
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, ObserveOnDefersSingleSuccess)
		{
			StrictMock<MockFunction<int()>> producer;
			EXPECT_CALL(producer, Call()).WillOnce(::testing::Return(7));
			const auto subscription = subscribe(Single<int>::fromCallable(producer.AsStdFunction()).observeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(observer, next(7));
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, ObserveOnDefersMaybeSuccess)
		{
			const auto subscription = subscribe(Maybe<int>::just(7).observeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(observer, next(7));
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, DisposesSingleAfterOneScheduledSuccess)
		{
			bool	   succeeded = false;
			const auto subscription = subscribe(Single<int>::just(7).observeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(observer, next(7))
				.WillOnce(
					[&](int)
					{
						succeeded = true;
					});
			dispatchUntil(
				[&]
				{
					return succeeded;
				});
			EXPECT_TRUE(subscription.isDisposed());
			drain();
		}

		TEST_F(SchedulerTest, DisposesMaybeAfterOneScheduledSuccess)
		{
			bool	   succeeded = false;
			const auto subscription = subscribe(Maybe<int>::just(7).observeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(observer, next(7))
				.WillOnce(
					[&](int)
					{
						succeeded = true;
					});
			dispatchUntil(
				[&]
				{
					return succeeded;
				});
			EXPECT_TRUE(subscription.isDisposed());
			drain();
		}

		TEST_F(SchedulerTest, ObserveOnDefersEmptyMaybeCompletion)
		{
			subscribe(Maybe<int>::empty().observeOn(loop));
			EXPECT_CALL(observer, complete());
			drain();
		}

		TEST_F(SchedulerTest, ObserveOnDefersCompletableCompletion)
		{
			StrictMock<MockFunction<void()>> action;
			EXPECT_CALL(action, Call());
			const auto subscription = subscribe(Completable::fromAction(action.AsStdFunction()).observeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(observer, complete());
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, ObserveOnDefersObservableError)
		{
			subscribe(Observable<int>::error(failure).observeOn(loop));
			EXPECT_CALL(observer, error(failure));
			drain();
		}

		TEST_F(SchedulerTest, ObserveOnDefersSingleError)
		{
			subscribe(Single<int>::error(failure).observeOn(loop));
			EXPECT_CALL(observer, error(failure));
			drain();
		}

		TEST_F(SchedulerTest, ObserveOnDefersMaybeError)
		{
			subscribe(Maybe<int>::error(failure).observeOn(loop));
			EXPECT_CALL(observer, error(failure));
			drain();
		}

		TEST_F(SchedulerTest, ObserveOnDefersCompletableError)
		{
			subscribe(Completable::error(failure).observeOn(loop));
			EXPECT_CALL(observer, error(failure));
			drain();
		}

		TEST_F(SchedulerTest, ObserveOnDeliversQueuedValueBeforeError)
		{
			const auto subscription = subscribe(Observable<int>::fromIterable(std::array{1, 2})
													.map(
														[this](int value)
														{
															if (value == 2)
																std::rethrow_exception(failure);
															return value;
														})
													.observeOn(loop));
			EXPECT_FALSE(subscription.isDisposed());
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("producer failed")));
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, SubscribeOnForwardsDeferredFactoryException)
		{
			subscribe(Single<int>::fromCallable(
						  []() -> int
						  {
							  throw std::runtime_error{"scheduled"};
						  })
						  .subscribeOn(loop));
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("scheduled")));
			drain();
		}

		TEST_F(SchedulerTest, SeparatesSubscriptionFromNotificationQueues)
		{
			rpp::schedulers::run_loop		notifications;
			StrictMock<MockFunction<int()>> callable;
			const auto subscription = subscribe(Single<int>::fromCallable(callable.AsStdFunction()).subscribeOn(loop).observeOn(notifications));
			EXPECT_CALL(callable, Call()).WillOnce(::testing::Return(7));
			drain();
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(observer, next(7));
			drain(notifications);
			EXPECT_TRUE(subscription.isDisposed());
			EXPECT_TRUE(notifications.is_empty());
		}

		TEST_F(SchedulerTest, CancellationPreventsAllQueuedInnerSubscriptions)
		{
			StrictMock<MockFunction<void(int)>> factory;
			const auto							subscription = subscribe(Observable<int>::fromIterable(std::array{1, 2})
																			 .flatMap(
														 [&](int value)
														 {
															 return Observable<int>::defer(
																		[&, value]
																		{
																			factory.Call(value);
																			return Observable<int>::just(value);
																		})
																 .subscribeOn(loop);
														 }));
			subscription.dispose();
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, InnerErrorCancelsOtherQueuedInnerSubscription)
		{
			StrictMock<MockFunction<void()>> factory;
			EXPECT_CALL(observer, error(failure));
			const auto subscription = subscribe(Observable<int>::fromIterable(std::array{1, 2})
													.flatMap(
														[&](int value)
														{
															if (value == 2)
																return Observable<int>::error(failure);
															return Observable<int>::defer(
																	   [&]
																	   {
																		   factory.Call();
																		   return Observable<int>::just(1);
																	   })
																.subscribeOn(loop);
														}));
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, CancellationPreventsAndThenObservableSubscription)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto subscription = subscribe(Completable::complete().subscribeOn(loop).andThen(Observable<int>::defer(factory.AsStdFunction())));
			subscription.dispose();
			drain();
		}

		TEST_F(SchedulerTest, CancellationPreventsAndThenSingleSubscription)
		{
			StrictMock<MockFunction<int()>> callable;
			const auto subscription = subscribe(Completable::complete().subscribeOn(loop).andThen(Single<int>::fromCallable(callable.AsStdFunction())));
			subscription.dispose();
			drain();
		}

		TEST_F(SchedulerTest, CancellationPreventsAndThenMaybeSubscription)
		{
			StrictMock<MockFunction<std::optional<int>()>> callable;
			const auto subscription = subscribe(Completable::complete().subscribeOn(loop).andThen(Maybe<int>::fromCallable(callable.AsStdFunction())));
			subscription.dispose();
			drain();
		}

		TEST_F(SchedulerTest, CancellationPreventsAndThenCompletableSubscription)
		{
			StrictMock<MockFunction<void()>> action;
			const auto subscription = subscribe(Completable::complete().subscribeOn(loop).andThen(Completable::fromAction(action.AsStdFunction())));
			subscription.dispose();
			drain();
		}

		TEST_F(SchedulerTest, CancellationSuppressesQueuedNotifications)
		{
			const auto subscription = subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}).observeOn(loop));
			subscription.dispose();
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerTest, CancellationInNextSuppressesRemainingQueuedNotifications)
		{
			Disposable subscription;
			subscription = Observable<int>::fromIterable(std::array{1, 2, 3})
							   .observeOn(loop)
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
	} // namespace
} // namespace rppw::test
