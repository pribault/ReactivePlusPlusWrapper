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
		class SideEffectTest : public RunLoopTest
		{
		protected:
			static Source success()
			{
				if constexpr (std::same_as<Source, Completable>)
					return Source::complete();
				else
					return Source::just(7);
			}

			void expectSuccess()
			{
				if constexpr (!std::same_as<Source, Completable>)
					EXPECT_CALL(observer, next(7));
				if constexpr (std::same_as<Source, Completable> || std::same_as<Source, Observable<int>>)
					EXPECT_CALL(observer, complete());
			}
		};

		using SourceTypes = ::testing::Types<Completable, Single<int>, Maybe<int>, Observable<int>>;
		TYPED_TEST_SUITE(SideEffectTest, SourceTypes);

		TYPED_TEST(SideEffectTest, ObservesErrorBeforeForwardingOriginalError)
		{
			StrictMock<MockFunction<void(std::exception_ptr)>> action;
			const auto										   source = TypeParam::error(this->failure).doOnError(action.AsStdFunction());
			InSequence										   sequence;
			EXPECT_CALL(action, Call(this->failure));
			EXPECT_CALL(this->observer, error(this->failure));
			EXPECT_TRUE(this->subscribe(source).isDisposed());
		}

		TYPED_TEST(SideEffectTest, SkipsErrorHookOnSuccess)
		{
			StrictMock<MockFunction<void(std::exception_ptr)>> action;
			this->expectSuccess();
			this->subscribe(this->success().doOnError(action.AsStdFunction()));
		}

		TYPED_TEST(SideEffectTest, ExecutesCompletionHookBeforeTerminalNotification)
		{
			StrictMock<MockFunction<void()>> action;
			const auto						 source = this->success().doOnComplete(action.AsStdFunction());
			InSequence						 sequence;
			if constexpr (std::same_as<TypeParam, Observable<int>>)
				EXPECT_CALL(this->observer, next(7));
			EXPECT_CALL(action, Call());
			if constexpr (std::same_as<TypeParam, Observable<int>>)
				EXPECT_CALL(this->observer, complete());
			else
				this->expectSuccess();
			EXPECT_TRUE(this->subscribe(source).isDisposed());
		}

		TYPED_TEST(SideEffectTest, SkipsCompletionHookOnError)
		{
			StrictMock<MockFunction<void()>> action;
			EXPECT_CALL(this->observer, error(this->failure));
			this->subscribe(TypeParam::error(this->failure).doOnComplete(action.AsStdFunction()));
		}

		TYPED_TEST(SideEffectTest, RepeatsCompletionHookForEverySubscription)
		{
			StrictMock<MockFunction<void()>> action;
			const auto						 source = this->success().doOnComplete(action.AsStdFunction());
			EXPECT_CALL(action, Call()).Times(2);
			if constexpr (!std::same_as<TypeParam, Completable>)
				EXPECT_CALL(this->observer, next(7)).Times(2);
			if constexpr (std::same_as<TypeParam, Completable> || std::same_as<TypeParam, Observable<int>>)
				EXPECT_CALL(this->observer, complete()).Times(2);
			this->subscribe(source);
			this->subscribe(source);
		}

		TYPED_TEST(SideEffectTest, RepeatsErrorHookForEverySubscription)
		{
			StrictMock<MockFunction<void(std::exception_ptr)>> action;
			const auto										   source = TypeParam::error(this->failure).doOnError(action.AsStdFunction());
			EXPECT_CALL(action, Call(this->failure)).Times(2);
			EXPECT_CALL(this->observer, error(this->failure)).Times(2);
			this->subscribe(source);
			this->subscribe(source);
		}

		TYPED_TEST(SideEffectTest, CancelsScheduledCompletionHook)
		{
			StrictMock<MockFunction<void()>> action;
			const auto						 subscription = this->subscribe(this->success().observeOn(this->loop).doOnComplete(action.AsStdFunction()));
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(SideEffectTest, CancelsScheduledErrorHook)
		{
			StrictMock<MockFunction<void(std::exception_ptr)>> action;
			const auto subscription = this->subscribe(TypeParam::error(this->failure).observeOn(this->loop).doOnError(action.AsStdFunction()));
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(SideEffectTest, PreservesSourceWhenAddingHooks)
		{
			StrictMock<MockFunction<void()>> action;
			const auto						 original = this->success();
			const auto						 decorated = original.doOnComplete(action.AsStdFunction());
			this->expectSuccess();
			this->subscribe(original);
			EXPECT_CALL(action, Call());
			this->expectSuccess();
			this->subscribe(decorated);
		}

		template <typename Source>
		class SuccessSideEffectTest : public RunLoopTest
		{
		};
		using SuccessSources = ::testing::Types<Single<int>, Maybe<int>>;
		TYPED_TEST_SUITE(SuccessSideEffectTest, SuccessSources);

		TYPED_TEST(SuccessSideEffectTest, ObservesSuccessBeforeForwardingUnchangedValue)
		{
			StrictMock<MockFunction<void(int)>> action;
			const auto							source = TypeParam::just(7).doOnSuccess(action.AsStdFunction());
			InSequence							sequence;
			EXPECT_CALL(action, Call(7));
			EXPECT_CALL(this->observer, next(7));
			this->subscribe(source);
		}

		TYPED_TEST(SuccessSideEffectTest, SkipsSuccessHookOnError)
		{
			StrictMock<MockFunction<void(int)>> action;
			EXPECT_CALL(this->observer, error(this->failure));
			this->subscribe(TypeParam::error(this->failure).doOnSuccess(action.AsStdFunction()));
		}

		TYPED_TEST(SuccessSideEffectTest, RepeatsSuccessHookForEverySubscription)
		{
			StrictMock<MockFunction<void(int)>> action;
			const auto							source = TypeParam::just(7).doOnSuccess(action.AsStdFunction());
			EXPECT_CALL(action, Call(7)).Times(2);
			EXPECT_CALL(this->observer, next(7)).Times(2);
			this->subscribe(source);
			this->subscribe(source);
		}

		TYPED_TEST(SuccessSideEffectTest, CancelsScheduledSuccessHook)
		{
			StrictMock<MockFunction<void(int)>> action;
			const auto							subscription = this->subscribe(TypeParam::just(7).observeOn(this->loop).doOnSuccess(action.AsStdFunction()));
			subscription.dispose();
			this->drain();
		}

		class SideEffectOperatorTest : public RunLoopTest
		{
		};

		TEST_F(SideEffectOperatorTest, CompletesEmptyMaybeBeforeForwardingCompletion)
		{
			StrictMock<MockFunction<void()>> action;
			InSequence						 sequence;
			EXPECT_CALL(action, Call());
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::empty().doOnComplete(action.AsStdFunction()));
		}

		TEST_F(SideEffectOperatorTest, SkipsSuccessHookForEmptyMaybe)
		{
			StrictMock<MockFunction<void(int)>> action;
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::empty().doOnSuccess(action.AsStdFunction()));
		}

		TEST_F(SideEffectOperatorTest, InvokesMaybeCompletionOnlyOnceThroughObservableConversion)
		{
			StrictMock<MockFunction<void()>> action;
			EXPECT_CALL(action, Call());
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(Maybe<int>::just(7).doOnComplete(action.AsStdFunction()).toObservable());
		}

		TEST_F(SideEffectOperatorTest, IsolatesMaybeCompletionStateBetweenSubscriptions)
		{
			StrictMock<MockFunction<void()>> action;
			int								 invocations = 0;
			const auto						 source = Maybe<int>::defer(
									  [&]
									  {
										  return ++invocations == 1 ? Maybe<int>::just(7) : Maybe<int>::empty();
									  })
									.doOnComplete(action.AsStdFunction());
			EXPECT_CALL(action, Call()).Times(2);
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(source);
			subscribe(source);
		}

		TEST_F(SideEffectOperatorTest, RunsHooksInChainOrder)
		{
			StrictMock<MockFunction<void(int)>> action;
			const auto							source = Single<int>::just(7)
									.doOnSuccess(
										[&](int)
										{
											action.Call(1);
										})
									.doOnComplete(
										[&]
										{
											action.Call(2);
										})
									.doOnSuccess(
										[&](int)
										{
											action.Call(3);
										});
			InSequence sequence;
			EXPECT_CALL(action, Call(1));
			EXPECT_CALL(action, Call(2));
			EXPECT_CALL(action, Call(3));
			EXPECT_CALL(observer, next(7));
			subscribe(source);
		}

		TEST_F(SideEffectOperatorTest, ObservesEveryValueBeforeForwardingIt)
		{
			StrictMock<MockFunction<void(int)>> action;
			const auto							source = Observable<int>::fromIterable(std::array{1, 2}).doOnNext(action.AsStdFunction());
			InSequence							sequence;
			EXPECT_CALL(action, Call(1));
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(action, Call(2));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			subscribe(source);
		}

		TEST_F(SideEffectOperatorTest, SkipsNextHookForEmptyObservable)
		{
			StrictMock<MockFunction<void(int)>> action;
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::empty().doOnNext(action.AsStdFunction()));
		}

		TEST_F(SideEffectOperatorTest, SkipsNextHookOnError)
		{
			StrictMock<MockFunction<void(int)>> action;
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::error(failure).doOnNext(action.AsStdFunction()));
		}

		TEST_F(SideEffectOperatorTest, CompletesObservableAfterAllValues)
		{
			StrictMock<MockFunction<void()>> action;
			InSequence						 sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(action, Call());
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2}).doOnComplete(action.AsStdFunction()));
		}

		TEST_F(SideEffectOperatorTest, CompletesEmptyObservable)
		{
			StrictMock<MockFunction<void()>> action;
			EXPECT_CALL(action, Call());
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::empty().doOnComplete(action.AsStdFunction()));
		}

		TEST_F(SideEffectOperatorTest, CancelsNextHookAfterTakeLimit)
		{
			StrictMock<MockFunction<void(int)>> action;
			EXPECT_CALL(action, Call(1));
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2}).doOnNext(action.AsStdFunction()).take(1));
		}

		TEST_F(SideEffectOperatorTest, RepeatsNextHookForEverySubscription)
		{
			StrictMock<MockFunction<void(int)>> action;
			const auto							source = Observable<int>::just(7).doOnNext(action.AsStdFunction());
			EXPECT_CALL(action, Call(7)).Times(2);
			EXPECT_CALL(observer, next(7)).Times(2);
			EXPECT_CALL(observer, complete()).Times(2);
			subscribe(source);
			subscribe(source);
		}

		TEST_F(SideEffectOperatorTest, PreservesErrorWhenHooksAreChained)
		{
			StrictMock<MockFunction<void(std::exception_ptr)>> first;
			StrictMock<MockFunction<void(std::exception_ptr)>> second;
			InSequence										   sequence;
			EXPECT_CALL(first, Call(failure));
			EXPECT_CALL(second, Call(failure));
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::error(failure).doOnError(first.AsStdFunction()).doOnError(second.AsStdFunction()));
		}

		TEST_F(SideEffectOperatorTest, TerminatesWhenNextHookThrows)
		{
			EXPECT_DEATH(subscribe(Observable<int>::just(7).doOnNext(
							 [](int)
							 {
								 throw std::runtime_error{"hook"};
							 })),
						 "");
		}

		TEST_F(SideEffectOperatorTest, TerminatesWhenErrorHookThrows)
		{
			EXPECT_DEATH(subscribe(Completable::error(failure).doOnError(
							 [](const std::exception_ptr &)
							 {
								 throw std::runtime_error{"hook"};
							 })),
						 "");
		}

		TEST_F(SideEffectOperatorTest, TerminatesWhenCompletionHookThrows)
		{
			EXPECT_DEATH(subscribe(Maybe<int>::empty().doOnComplete(
							 []
							 {
								 throw std::runtime_error{"hook"};
							 })),
						 "");
		}

		TEST_F(SideEffectOperatorTest, TerminatesWhenSuccessHookThrows)
		{
			EXPECT_DEATH(subscribe(Single<int>::just(7).doOnSuccess(
							 [](int)
							 {
								 throw std::runtime_error{"hook"};
							 })),
						 "");
		}
	} // namespace
} // namespace rppw::test
