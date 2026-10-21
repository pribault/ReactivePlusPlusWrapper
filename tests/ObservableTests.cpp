// tests
#include <support/RunLoopTest.h>

// stl
#include <array>
#include <limits>
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

		TEST_F(ObservableTest, RepeatZeroCompletesWithoutSubscribing)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::defer(factory.AsStdFunction()).repeat(0));
		}

		TEST_F(ObservableTest, RepeatPreservesEachSequenceOrder)
		{
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2}).repeat(2));
		}

		TEST_F(ObservableTest, RepeatOneSubscribesOnlyOnce)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Observable<int>::just(7)));
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::defer(factory.AsStdFunction()).repeat(1));
		}

		TEST_F(ObservableTest, RepeatStopsOnFirstError)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Observable<int>::error(failure)));
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::defer(factory.AsStdFunction()).repeat(3));
		}

		TEST_F(ObservableTest, RepeatDefersEverySubscription)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto									source = Observable<int>::defer(factory.AsStdFunction()).repeat(2);
			EXPECT_CALL(factory, Call()).Times(4).WillRepeatedly(::testing::Return(Observable<int>::just(7)));
			EXPECT_CALL(observer, next(7)).Times(4);
			EXPECT_CALL(observer, complete()).Times(2);
			subscribe(source);
			subscribe(source);
		}

		TEST_F(ObservableTest, RepeatEmptySourceIsStackSafe)
		{
			int		   attempts = 0;
			const auto source = Observable<int>::defer(
				[&]
				{
					++attempts;
					return Observable<int>::empty();
				});
			EXPECT_CALL(observer, complete());
			subscribe(source.repeat(10000));
			EXPECT_EQ(attempts, 10000);
		}

		TEST_F(ObservableTest, UnlimitedRepeatStopsOnDownstreamCancellation)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(factory, Call()).Times(3).WillRepeatedly(::testing::Return(Observable<int>::just(7)));
			EXPECT_CALL(observer, next(7)).Times(3);
			EXPECT_CALL(observer, complete());
			EXPECT_TRUE(subscribe(Observable<int>::defer(factory.AsStdFunction()).repeat().take(3)).isDisposed());
		}

		TEST_F(ObservableTest, MaximumRepeatCountRemainsCancellable)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(factory, Call()).Times(2).WillRepeatedly(::testing::Return(Observable<int>::just(7)));
			EXPECT_CALL(observer, next(7)).Times(2);
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::defer(factory.AsStdFunction()).repeat(std::numeric_limits<std::size_t>::max()).take(2));
		}

		TEST_F(ObservableTest, RetryZeroMakesOnlyInitialAttempt)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Observable<int>::error(failure)));
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::defer(factory.AsStdFunction()).retry(0));
		}

		TEST_F(ObservableTest, RetryExhaustsAdditionalAttemptsBeforeForwardingLastError)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(factory, Call())
				.WillOnce(::testing::Return(Observable<int>::error(std::runtime_error{"first"})))
				.WillOnce(::testing::Return(Observable<int>::error(std::runtime_error{"second"})))
				.WillOnce(::testing::Return(Observable<int>::error(failure)));
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::defer(factory.AsStdFunction()).retry(2));
		}

		TEST_F(ObservableTest, RetryPreservesValuesFromFailedAttempts)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto									failed = Observable<int>::concat(std::array{Observable<int>::just(1), Observable<int>::error(failure)});
			EXPECT_CALL(factory, Call())
				.WillOnce(::testing::Return(failed))
				.WillOnce(::testing::Return(failed))
				.WillOnce(::testing::Return(Observable<int>::just(9)));
			InSequence sequence;
			EXPECT_CALL(observer, next(1)).Times(2);
			EXPECT_CALL(observer, next(9));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::defer(factory.AsStdFunction()).retry(2));
		}

		TEST_F(ObservableTest, RetryDoesNotRepeatSuccessfulSource)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Observable<int>::just(7)));
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::defer(factory.AsStdFunction()).retry(3));
		}

		TEST_F(ObservableTest, RetryDoesNotRepeatEmptyCompletion)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Observable<int>::empty()));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::defer(factory.AsStdFunction()).retry());
		}

		TEST_F(ObservableTest, RetryDefersAndResetsForEverySubscriber)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto									source = Observable<int>::defer(factory.AsStdFunction()).retry(1);
			EXPECT_CALL(factory, Call()).Times(4).WillRepeatedly(::testing::Return(Observable<int>::error(failure)));
			EXPECT_CALL(observer, error(failure)).Times(2);
			subscribe(source);
			subscribe(source);
		}

		TEST_F(ObservableTest, RetrySynchronousErrorsAreStackSafe)
		{
			int		   attempts = 0;
			const auto source = Observable<int>::defer(
				[&]
				{
					++attempts;
					return Observable<int>::error(failure);
				});
			EXPECT_CALL(observer, error(failure));
			subscribe(source.retry(10000));
			EXPECT_EQ(attempts, 10001);
		}

		TEST_F(ObservableTest, UnlimitedRetryStopsOnSuccess)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(factory, Call())
				.WillOnce(::testing::Return(Observable<int>::error(failure)))
				.WillOnce(::testing::Return(Observable<int>::error(failure)))
				.WillOnce(::testing::Return(Observable<int>::just(7)));
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::defer(factory.AsStdFunction()).retry());
		}

		TEST_F(ObservableTest, UnlimitedRetryStopsOnDownstreamCancellation)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto									failed = Observable<int>::concat(std::array{Observable<int>::just(7), Observable<int>::error(failure)});
			EXPECT_CALL(factory, Call()).Times(3).WillRepeatedly(::testing::Return(failed));
			EXPECT_CALL(observer, next(7)).Times(3);
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::defer(factory.AsStdFunction()).retry().take(3));
		}

		TEST_F(ObservableTest, MaximumRetryCountAllowsRecovery)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Observable<int>::error(failure))).WillOnce(::testing::Return(Observable<int>::just(7)));
			EXPECT_CALL(observer, next(7));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::defer(factory.AsStdFunction()).retry(std::numeric_limits<std::size_t>::max()));
		}

		template <typename Case>
		class ObservableResubscriptionTest : public RunLoopTest
		{
		protected:
			Observable<int> resubscribe(const Observable<int> &source)
			{
				if constexpr (Case::value)
					return source.retry(3);
				else
					return source.repeat(3);
			}
		};

		using ResubscriptionCases = ::testing::Types<std::false_type, std::true_type>;
		TYPED_TEST_SUITE(ObservableResubscriptionTest, ResubscriptionCases);

		TYPED_TEST(ObservableResubscriptionTest, CancelsBeforeInitialScheduledAttempt)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto subscription = this->subscribe(this->resubscribe(Observable<int>::defer(factory.AsStdFunction()).subscribeOn(this->loop)));
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ObservableResubscriptionTest, CancellationPreventsNextScheduledAttempt)
		{
			int		   attempts = 0;
			const auto source = Observable<int>::defer(
									[&]
									{
										++attempts;
										if constexpr (TypeParam::value)
											return Observable<int>::error(this->failure);
										else
											return Observable<int>::just(7);
									})
									.subscribeOn(this->loop);
			if constexpr (!TypeParam::value)
				EXPECT_CALL(this->observer, next(7));
			const auto subscription = this->subscribe(this->resubscribe(source));
			this->dispatchUntil(
				[&]
				{
					return attempts == 1;
				});
			subscription.dispose();
			this->drain();
			EXPECT_EQ(attempts, 1);
			EXPECT_TRUE(subscription.isDisposed());
		}

		template <typename Case>
		class ObservableAggregateTest : public RunLoopTest
		{
		protected:
			auto aggregate(const Observable<int> &source)
			{
				if constexpr (Case::value == 0)
					return source.count();
				else if constexpr (Case::value == 1)
					return source.max();
				else
					return source.min();
			}

			Disposable subscribeResult(const Observable<int> &source)
			{
				if constexpr (Case::value == 0)
					return subscribe(aggregate(source).map(
						[](std::size_t value)
						{
							return static_cast<int>(value);
						}));
				else
					return subscribe(aggregate(source));
			}

			static constexpr int expectedValue = Case::value == 0 ? 4 : Case::value == 1 ? 7 : -2;
		};

		using AggregateCases = ::testing::Types<std::integral_constant<int, 0>, std::integral_constant<int, 1>, std::integral_constant<int, 2>>;
		TYPED_TEST_SUITE(ObservableAggregateTest, AggregateCases);

		TYPED_TEST(ObservableAggregateTest, AggregatesAllValues)
		{
			EXPECT_CALL(this->observer, next(TestFixture::expectedValue));
			EXPECT_TRUE(this->subscribeResult(Observable<int>::fromIterable(std::array{4, -2, 7, -2})).isDisposed());
		}

		TYPED_TEST(ObservableAggregateTest, HandlesEmptySource)
		{
			if constexpr (TypeParam::value == 0)
				EXPECT_CALL(this->observer, next(0));
			else
				EXPECT_CALL(this->observer, complete());
			EXPECT_TRUE(this->subscribeResult(Observable<int>::empty()).isDisposed());
		}

		TYPED_TEST(ObservableAggregateTest, HandlesSingleValue)
		{
			EXPECT_CALL(this->observer, next(TypeParam::value == 0 ? 1 : 9));
			this->subscribeResult(Observable<int>::just(9));
		}

		TYPED_TEST(ObservableAggregateTest, ForwardsOriginalError)
		{
			EXPECT_CALL(this->observer, error(this->failure));
			EXPECT_TRUE(this->subscribeResult(Observable<int>::error(this->failure)).isDisposed());
		}

		TYPED_TEST(ObservableAggregateTest, DiscardsPartialResultOnError)
		{
			const auto source = Observable<int>::concat(std::array{Observable<int>::just(9), Observable<int>::error(this->failure)});
			EXPECT_CALL(this->observer, error(this->failure));
			this->subscribeResult(source);
		}

		TYPED_TEST(ObservableAggregateTest, DefersSourceForEachSubscription)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto									result = this->aggregate(Observable<int>::defer(factory.AsStdFunction()));
			InSequence									sequence;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Observable<int>::fromIterable(std::array{4, -2, 7, -2})));
			EXPECT_CALL(this->observer, next(TestFixture::expectedValue));
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Observable<int>::fromIterable(std::array{4, -2, 7, -2})));
			EXPECT_CALL(this->observer, next(TestFixture::expectedValue));
			if constexpr (TypeParam::value == 0)
			{
				this->subscribe(result.map(
					[](std::size_t value)
					{
						return static_cast<int>(value);
					}));
				this->subscribe(result.map(
					[](std::size_t value)
					{
						return static_cast<int>(value);
					}));
			}
			else
			{
				this->subscribe(result);
				this->subscribe(result);
			}
		}

		TYPED_TEST(ObservableAggregateTest, WaitsForNormalCompletion)
		{
			const auto source =
				Observable<int>::concat(std::array{Observable<int>::fromIterable(std::array{4, -2, 7, -2}), Observable<int>::empty().subscribeOn(this->loop)});
			const auto subscription = this->subscribeResult(source);
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(this->observer, next(TestFixture::expectedValue));
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ObservableAggregateTest, CancelsScheduledUpstream)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto subscription = this->subscribeResult(Observable<int>::defer(factory.AsStdFunction()).subscribeOn(this->loop));
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ObservableAggregateTest, CancellationDiscardsPartialResult)
		{
			const auto source = Observable<int>::concat(std::array{Observable<int>::just(9), Observable<int>::empty().subscribeOn(this->loop)});
			const auto subscription = this->subscribeResult(source);
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		template <typename Case>
		class ObservableExtremumTest : public ReactiveTest
		{
		protected:
			template <detail::Value T, typename Compare>
			Maybe<T> select(const Observable<T> &source, Compare compare)
			{
				if constexpr (Case::value)
					return source.max(std::move(compare));
				else
					return source.min(std::move(compare));
			}
		};

		using ExtremumCases = ::testing::Types<std::false_type, std::true_type>;
		TYPED_TEST_SUITE(ObservableExtremumTest, ExtremumCases);

		TYPED_TEST(ObservableExtremumTest, SupportsReverseOrdering)
		{
			EXPECT_CALL(this->observer, next(TypeParam::value ? -2 : 7));
			this->subscribe(this->select(Observable<int>::fromIterable(std::array{4, -2, 7}), std::greater<>{}));
		}

		TYPED_TEST(ObservableExtremumTest, PropagatesComparatorException)
		{
			StrictMock<MockFunction<bool(const int &, const int &)>> compare;
			EXPECT_CALL(compare, Call(_, _)).WillOnce(::testing::Throw(std::runtime_error{"compare"}));
			EXPECT_CALL(this->observer, error(ExceptionContaining<std::runtime_error>("compare")));
			const auto result = this->select(Observable<int>::fromIterable(std::array{1, 2, 3}), compare.AsStdFunction());
			EXPECT_TRUE(this->subscribe(result).isDisposed());
		}

		TYPED_TEST(ObservableExtremumTest, DoesNotCompareEmptySource)
		{
			StrictMock<MockFunction<bool(const int &, const int &)>> compare;
			EXPECT_CALL(this->observer, complete());
			this->subscribe(this->select(Observable<int>::empty(), compare.AsStdFunction()));
		}

		TYPED_TEST(ObservableExtremumTest, DoesNotCompareSingleValue)
		{
			StrictMock<MockFunction<bool(const int &, const int &)>> compare;
			EXPECT_CALL(this->observer, next(7));
			this->subscribe(this->select(Observable<int>::just(7), compare.AsStdFunction()));
		}

		struct ExtremumValue
		{
			explicit ExtremumValue(int key, int id)
				: key{key}
				, id{id}
			{
			}

			int key;
			int id;
		};

		TYPED_TEST(ObservableExtremumTest, KeepsFirstEquivalentNonDefaultConstructibleValue)
		{
			const auto source = Observable<ExtremumValue>::fromIterable(std::array{ExtremumValue{5, 1}, ExtremumValue{5, 2}});
			const auto result = this->select(source,
											 [](const ExtremumValue &left, const ExtremumValue &right)
											 {
												 return left.key < right.key;
											 });
			EXPECT_CALL(this->observer, next(1));
			this->subscribe(result.map(
				[](const ExtremumValue &value)
				{
					return value.id;
				}));
		}

		template <typename Case>
		class ObservableSelectionTest : public RunLoopTest
		{
		protected:
			Maybe<int> select(const Observable<int> &source)
			{
				if constexpr (Case::value == 0)
					return source.elementAt(1);
				else if constexpr (Case::value == 1)
					return source.first();
				else
					return source.last();
			}

			static constexpr int expectedValue = Case::value == 0 ? 2 : Case::value == 1 ? 1 : 3;
		};

		using SelectionCases = ::testing::Types<std::integral_constant<int, 0>, std::integral_constant<int, 1>, std::integral_constant<int, 2>>;
		TYPED_TEST_SUITE(ObservableSelectionTest, SelectionCases);

		TYPED_TEST(ObservableSelectionTest, SelectsExpectedValue)
		{
			EXPECT_CALL(this->observer, next(TestFixture::expectedValue));
			EXPECT_TRUE(this->subscribe(this->select(Observable<int>::fromIterable(std::array{1, 2, 3}))).isDisposed());
		}

		TYPED_TEST(ObservableSelectionTest, CompletesEmptySourceWithoutValue)
		{
			EXPECT_CALL(this->observer, complete());
			EXPECT_TRUE(this->subscribe(this->select(Observable<int>::empty())).isDisposed());
		}

		TYPED_TEST(ObservableSelectionTest, ForwardsOriginalError)
		{
			EXPECT_CALL(this->observer, error(this->failure));
			EXPECT_TRUE(this->subscribe(this->select(Observable<int>::error(this->failure))).isDisposed());
		}

		TYPED_TEST(ObservableSelectionTest, DefersSourceForEachSubscription)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto									result = this->select(Observable<int>::defer(factory.AsStdFunction()));
			InSequence									sequence;
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Observable<int>::fromIterable(std::array{1, 2, 3})));
			EXPECT_CALL(this->observer, next(TestFixture::expectedValue));
			EXPECT_CALL(factory, Call()).WillOnce(::testing::Return(Observable<int>::fromIterable(std::array{1, 2, 3})));
			EXPECT_CALL(this->observer, next(TestFixture::expectedValue));
			this->subscribe(result);
			this->subscribe(result);
		}

		TYPED_TEST(ObservableSelectionTest, WaitsForScheduledValues)
		{
			const auto subscription = this->subscribe(this->select(Observable<int>::fromIterable(std::array{1, 2, 3}).observeOn(this->loop)));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(this->observer, next(TestFixture::expectedValue));
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ObservableSelectionTest, CancelsScheduledUpstream)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto subscription = this->subscribe(this->select(Observable<int>::defer(factory.AsStdFunction()).subscribeOn(this->loop)));
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableTest, ElementAtZeroSelectsFirstValue)
		{
			EXPECT_CALL(observer, next(1));
			subscribe(Observable<int>::fromIterable(std::array{1, 2}).elementAt(0));
		}

		TEST_F(ObservableTest, ElementAtCancelsBeforeProducingFollowingValue)
		{
			StrictMock<MockFunction<void(int)>> produced;
			InSequence							sequence;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(produced, Call(2));
			EXPECT_CALL(observer, next(2));
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}).doOnNext(produced.AsStdFunction()).elementAt(1));
		}

		TEST_F(ObservableTest, FirstCancelsBeforeProducingFollowingValue)
		{
			StrictMock<MockFunction<void(int)>> produced;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(observer, next(1));
			subscribe(Observable<int>::fromIterable(std::array{1, 2}).doOnNext(produced.AsStdFunction()).first());
		}

		TEST_F(ObservableTest, ElementAtSourceLengthCompletesEmpty)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2}).elementAt(2));
		}

		TEST_F(ObservableTest, ElementAtMaximumIndexCompletesEmpty)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::just(1).elementAt(std::numeric_limits<std::size_t>::max()));
		}

		TEST_F(ObservableTest, ElementAtPreservesErrorBeforeSelectedValue)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::concat(std::array{Observable<int>::just(1), Observable<int>::error(failure)}).elementAt(1));
		}

		TEST_F(ObservableTest, ElementAtIgnoresErrorAfterSelectedValue)
		{
			EXPECT_CALL(observer, next(2));
			subscribe(Observable<int>::concat(std::array{Observable<int>::fromIterable(std::array{1, 2}), Observable<int>::error(failure)}).elementAt(1));
		}

		TEST_F(ObservableTest, FirstIgnoresErrorAfterSelectedValue)
		{
			EXPECT_CALL(observer, next(1));
			subscribe(Observable<int>::concat(std::array{Observable<int>::just(1), Observable<int>::error(failure)}).first());
		}

		TEST_F(ObservableTest, LastDiscardsBufferedValueOnError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::concat(std::array{Observable<int>::just(1), Observable<int>::error(failure)}).last());
		}

		TEST_F(ObservableTest, LastEmitsOnlyAfterAllValuesAreProduced)
		{
			StrictMock<MockFunction<void(int)>> produced;
			InSequence							sequence;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(produced, Call(2));
			EXPECT_CALL(produced, Call(3));
			EXPECT_CALL(observer, next(3));
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}).doOnNext(produced.AsStdFunction()).last());
		}

		class ObservableLastTest : public RunLoopTest
		{
		};

		TEST_F(ObservableLastTest, WaitsForCompletionAfterLastValue)
		{
			const auto source = Observable<int>::concat(std::array{Observable<int>::just(7), Observable<int>::empty().subscribeOn(loop)});
			const auto subscription = subscribe(source.last());
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(observer, next(7));
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableLastTest, CancellationDiscardsBufferedValue)
		{
			const auto source = Observable<int>::concat(std::array{Observable<int>::just(7), Observable<int>::empty().subscribeOn(loop)});
			const auto subscription = subscribe(source.last());
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		template <typename Case>
		class ObservableQuantifierTest : public RunLoopTest
		{
		protected:
			template <typename F>
			Single<bool> apply(const Observable<int> &source, F predicate)
			{
				if constexpr (Case::value == 0)
					return source.allOf(std::move(predicate));
				else if constexpr (Case::value == 1)
					return source.anyOf(std::move(predicate));
				else
					return source.noneOf(std::move(predicate));
			}

			Disposable subscribeResult(const Single<bool> &source)
			{
				return subscribe(source.map(
					[](bool value)
					{
						return value ? 1 : 0;
					}));
			}

			static constexpr bool decisive = Case::value != 0;
			static constexpr int  matchedResult = Case::value == 1 ? 1 : 0;
			static constexpr int  exhaustedResult = Case::value == 1 ? 0 : 1;
		};

		using QuantifierCases = ::testing::Types<std::integral_constant<int, 0>, std::integral_constant<int, 1>, std::integral_constant<int, 2>>;
		TYPED_TEST_SUITE(ObservableQuantifierTest, QuantifierCases);

		TYPED_TEST(ObservableQuantifierTest, ReturnsIdentityForEmptySource)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(this->observer, next(TestFixture::exhaustedResult));
			EXPECT_TRUE(this->subscribeResult(this->apply(Observable<int>::empty(), predicate.AsStdFunction())).isDisposed());
		}

		TYPED_TEST(ObservableQuantifierTest, ReturnsResultAfterExhaustingSource)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(predicate, Call(1)).WillOnce(::testing::Return(!TestFixture::decisive));
			EXPECT_CALL(predicate, Call(2)).WillOnce(::testing::Return(!TestFixture::decisive));
			EXPECT_CALL(this->observer, next(TestFixture::exhaustedResult));
			this->subscribeResult(this->apply(Observable<int>::fromIterable(std::array{1, 2}), predicate.AsStdFunction()));
		}

		TYPED_TEST(ObservableQuantifierTest, CancelsUpstreamAtFirstDecisiveValue)
		{
			StrictMock<MockFunction<void(int)>> produced;
			StrictMock<MockFunction<bool(int)>> predicate;
			InSequence							sequence;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(predicate, Call(1)).WillOnce(::testing::Return(!TestFixture::decisive));
			EXPECT_CALL(produced, Call(2));
			EXPECT_CALL(predicate, Call(2)).WillOnce(::testing::Return(TestFixture::decisive));
			EXPECT_CALL(this->observer, next(TestFixture::matchedResult));
			const auto source = Observable<int>::fromIterable(std::array{1, 2, 3}).doOnNext(produced.AsStdFunction());
			EXPECT_TRUE(this->subscribeResult(this->apply(source, predicate.AsStdFunction())).isDisposed());
		}

		TYPED_TEST(ObservableQuantifierTest, SkipsErrorsAfterDecisiveValue)
		{
			const auto source = Observable<int>::concat(std::array{Observable<int>::just(1), Observable<int>::error(this->failure)});
			EXPECT_CALL(this->observer, next(TestFixture::matchedResult));
			this->subscribeResult(this->apply(source,
											  [](int)
											  {
												  return TestFixture::decisive;
											  }));
		}

		TYPED_TEST(ObservableQuantifierTest, PreservesUpstreamErrorWithoutResult)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(this->observer, error(this->failure));
			this->subscribeResult(this->apply(Observable<int>::error(this->failure), predicate.AsStdFunction()));
		}

		TYPED_TEST(ObservableQuantifierTest, PreservesErrorAfterInconclusiveValues)
		{
			const auto source = Observable<int>::concat(std::array{Observable<int>::just(1), Observable<int>::error(this->failure)});
			EXPECT_CALL(this->observer, error(this->failure));
			this->subscribeResult(this->apply(source,
											  [](int)
											  {
												  return !TestFixture::decisive;
											  }));
		}

		TYPED_TEST(ObservableQuantifierTest, PropagatesPredicateExceptionWithoutResult)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(predicate, Call(1)).WillOnce(::testing::Throw(std::runtime_error{"quantifier"}));
			EXPECT_CALL(this->observer, error(ExceptionContaining<std::runtime_error>("quantifier")));
			const auto subscription = this->subscribeResult(this->apply(Observable<int>::fromIterable(std::array{1, 2}), predicate.AsStdFunction()));
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ObservableQuantifierTest, DefersEvaluationForEverySubscription)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			const auto							source = this->apply(Observable<int>::just(1), predicate.AsStdFunction());
			InSequence							sequence;
			EXPECT_CALL(predicate, Call(1)).WillOnce(::testing::Return(TestFixture::decisive));
			EXPECT_CALL(this->observer, next(TestFixture::matchedResult));
			EXPECT_CALL(predicate, Call(1)).WillOnce(::testing::Return(!TestFixture::decisive));
			EXPECT_CALL(this->observer, next(TestFixture::exhaustedResult));
			this->subscribeResult(source);
			this->subscribeResult(source);
		}

		TYPED_TEST(ObservableQuantifierTest, WaitsForCompletionWithoutDecisiveValue)
		{
			const auto source = Observable<int>::concat(std::array{Observable<int>::just(1), Observable<int>::empty().subscribeOn(this->loop)});
			const auto subscription = this->subscribeResult(this->apply(source,
																		[](int)
																		{
																			return !TestFixture::decisive;
																		}));
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(this->observer, next(TestFixture::exhaustedResult));
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ObservableQuantifierTest, CancelsScheduledUpstreamWithoutResult)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			StrictMock<MockFunction<bool(int)>>			predicate;
			const auto									source = Observable<int>::defer(factory.AsStdFunction()).subscribeOn(this->loop);
			const auto									subscription = this->subscribeResult(this->apply(source, predicate.AsStdFunction()));
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ObservableQuantifierTest, AcceptsBooleanConvertiblePredicateResult)
		{
			EXPECT_CALL(this->observer, next(TypeParam::value == 2 ? 0 : 1));
			this->subscribeResult(this->apply(Observable<int>::just(1),
											  [](const int &value)
											  {
												  return value;
											  }));
		}

		template <typename Case>
		class ObservablePrefixTest : public RunLoopTest
		{
		protected:
			template <typename F>
			Observable<int> apply(const Observable<int> &source, F predicate)
			{
				if constexpr (Case::value == 0)
					return source.takeUntil(std::move(predicate));
				else if constexpr (Case::value == 1)
					return source.takeWhile(
						[predicate = std::move(predicate)](const int &value)
						{
							return !std::invoke(predicate, value);
						});
				else if constexpr (Case::value == 2)
					return source.dropUntil(std::move(predicate));
				else
					return source.dropWhile(
						[predicate = std::move(predicate)](const int &value)
						{
							return !std::invoke(predicate, value);
						});
			}

			Disposable collect(const Observable<int> &source)
			{
				return source.subscribe(
					[this](int value)
					{
						values.push_back(value);
					},
					[this](std::exception_ptr error)
					{
						observer.error(error);
					},
					[this]
					{
						observer.complete();
					});
			}

			std::vector<int> expected()
			{
				if constexpr (Case::value == 0)
					return {1, 2};
				else if constexpr (Case::value == 1)
					return {1};
				else
					return {2, 3, 1};
			}

			std::vector<int> values;
		};

		using PrefixCases =
			::testing::Types<std::integral_constant<int, 0>, std::integral_constant<int, 1>, std::integral_constant<int, 2>, std::integral_constant<int, 3>>;
		TYPED_TEST_SUITE(ObservablePrefixTest, PrefixCases);

		TYPED_TEST(ObservablePrefixTest, SelectsPrefixBoundaryExactlyOnce)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(predicate, Call(1)).WillOnce(::testing::Return(false));
			EXPECT_CALL(predicate, Call(2)).WillOnce(::testing::Return(true));
			EXPECT_CALL(this->observer, complete());
			const auto subscription = this->collect(this->apply(Observable<int>::fromIterable(std::array{1, 2, 3, 1}), predicate.AsStdFunction()));
			EXPECT_EQ(this->values, this->expected());
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ObservablePrefixTest, CompletesEmptySourceWithoutEvaluatingPredicate)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(this->observer, complete());
			this->collect(this->apply(Observable<int>::empty(), predicate.AsStdFunction()));
			EXPECT_TRUE(this->values.empty());
		}

		TYPED_TEST(ObservablePrefixTest, PreservesUpstreamError)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(this->observer, error(this->failure));
			this->collect(this->apply(Observable<int>::error(this->failure), predicate.AsStdFunction()));
			EXPECT_TRUE(this->values.empty());
		}

		TYPED_TEST(ObservablePrefixTest, PropagatesPredicateExceptionWithoutForwardingValue)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			EXPECT_CALL(predicate, Call(1)).WillOnce(::testing::Throw(std::runtime_error{"predicate"}));
			EXPECT_CALL(this->observer, error(ExceptionContaining<std::runtime_error>("predicate")));
			const auto subscription = this->collect(this->apply(Observable<int>::fromIterable(std::array{1, 2}), predicate.AsStdFunction()));
			EXPECT_TRUE(this->values.empty());
			EXPECT_TRUE(subscription.isDisposed());
		}

		TYPED_TEST(ObservablePrefixTest, HandlesBoundaryAtFirstValue)
		{
			EXPECT_CALL(this->observer, complete());
			this->collect(this->apply(Observable<int>::fromIterable(std::array{1, 2, 3}),
									  [](int)
									  {
										  return true;
									  }));
			if constexpr (TypeParam::value == 0)
				EXPECT_EQ(this->values, (std::vector{1}));
			else if constexpr (TypeParam::value == 1)
				EXPECT_TRUE(this->values.empty());
			else
				EXPECT_EQ(this->values, (std::vector{1, 2, 3}));
		}

		TYPED_TEST(ObservablePrefixTest, HandlesSourceWithoutBoundary)
		{
			EXPECT_CALL(this->observer, complete());
			this->collect(this->apply(Observable<int>::fromIterable(std::array{1, 2, 3}),
									  [](int)
									  {
										  return false;
									  }));
			if constexpr (TypeParam::value < 2)
				EXPECT_EQ(this->values, (std::vector{1, 2, 3}));
			else
				EXPECT_TRUE(this->values.empty());
		}

		TYPED_TEST(ObservablePrefixTest, ResetsStateForEachSubscription)
		{
			const auto source = this->apply(Observable<int>::fromIterable(std::array{1, 2, 3, 1}),
											[](int value)
											{
												return value == 2;
											});
			EXPECT_CALL(this->observer, complete()).Times(2);
			this->collect(source);
			EXPECT_EQ(this->values, this->expected());
			this->values.clear();
			this->collect(source);
			EXPECT_EQ(this->values, this->expected());
		}

		TYPED_TEST(ObservablePrefixTest, CancelsScheduledUpstream)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			StrictMock<MockFunction<bool(int)>>			predicate;
			const auto									subscription =
				this->collect(this->apply(Observable<int>::defer(factory.AsStdFunction()).subscribeOn(this->loop), predicate.AsStdFunction()));
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			this->drain();
			EXPECT_TRUE(subscription.isDisposed());
			EXPECT_TRUE(this->values.empty());
		}

		TYPED_TEST(ObservablePrefixTest, DoesNotEvaluatePredicateDuringComposition)
		{
			StrictMock<MockFunction<bool(int)>> predicate;
			const auto							source = this->apply(Observable<int>::just(1), predicate.AsStdFunction());
			EXPECT_CALL(predicate, Call(1)).WillOnce(::testing::Return(false));
			EXPECT_CALL(this->observer, complete());
			this->collect(source);
		}

		TYPED_TEST(ObservablePrefixTest, PreservesErrorBeforeBoundary)
		{
			const auto source = Observable<int>::concat(std::array{Observable<int>::just(1), Observable<int>::error(this->failure)});
			EXPECT_CALL(this->observer, error(this->failure));
			this->collect(this->apply(source,
									  [](int)
									  {
										  return false;
									  }));
			if constexpr (TypeParam::value < 2)
				EXPECT_EQ(this->values, (std::vector{1}));
			else
				EXPECT_TRUE(this->values.empty());
		}

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

		TEST_F(ObservableTest, TakeUntilCancelsBeforeProducingNextValue)
		{
			StrictMock<MockFunction<void(int)>> produced;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(produced, Call(2));
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3})
						  .doOnNext(produced.AsStdFunction())
						  .takeUntil(
							  [](int value)
							  {
								  return value == 2;
							  }));
		}

		TEST_F(ObservableTest, TakeWhileCancelsBeforeProducingFollowingValue)
		{
			StrictMock<MockFunction<void(int)>> produced;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(produced, Call(2));
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3})
						  .doOnNext(produced.AsStdFunction())
						  .takeWhile(
							  [](int value)
							  {
								  return value < 2;
							  }));
		}

		TEST_F(ObservableTest, TakeUntilPropagatesDownstreamCancellation)
		{
			StrictMock<MockFunction<void(int)>> produced;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3})
						  .doOnNext(produced.AsStdFunction())
						  .takeUntil(
							  [](int)
							  {
								  return false;
							  })
						  .take(1));
		}

		TEST_F(ObservableTest, DropPropagatesDownstreamCancellation)
		{
			StrictMock<MockFunction<void(int)>> produced;
			EXPECT_CALL(produced, Call(1));
			EXPECT_CALL(produced, Call(2));
			EXPECT_CALL(produced, Call(3));
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3, 4}).doOnNext(produced.AsStdFunction()).drop(2).take(1));
		}

		TEST_F(ObservableTest, DropZeroPreservesAllValues)
		{
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2}).drop(0));
		}

		TEST_F(ObservableTest, DropDiscardsOnlyInitialCount)
		{
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2, 3}).drop(2));
		}

		TEST_F(ObservableTest, DropCountBeyondSourceCompletesWithoutValues)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::just(1).drop(std::numeric_limits<std::size_t>::max()));
		}

		TEST_F(ObservableTest, DropCountEqualToSourceCompletesWithoutValues)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::fromIterable(std::array{1, 2}).drop(2));
		}

		TEST_F(ObservableTest, DropPreservesEmptyCompletion)
		{
			EXPECT_CALL(observer, complete());
			subscribe(Observable<int>::empty().drop(2));
		}

		TEST_F(ObservableTest, DropPreservesUpstreamError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(Observable<int>::error(failure).drop(2));
		}

		TEST_F(ObservableTest, DropResetsCountForEachSubscription)
		{
			const auto source = Observable<int>::fromIterable(std::array{1, 2}).drop(1);
			EXPECT_CALL(observer, next(2)).Times(2);
			EXPECT_CALL(observer, complete()).Times(2);
			subscribe(source);
			subscribe(source);
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

		class ObservableReduceTest : public RunLoopTest
		{
		protected:
			template <typename T, typename R, typename F>
			Single<R> reduce(const Observable<T> &source, R initial, F accumulator)
			{
				return source.reduce(std::move(initial), std::move(accumulator));
			}
		};

		TEST_F(ObservableReduceTest, SumsAllValues)
		{
			EXPECT_CALL(observer, next(10));
			subscribe(reduce(Observable<int>::fromIterable(std::array{1, 2, 3, 4}), 0,
							 [](int acc, int value)
							 {
								 return acc + value;
							 }));
		}

		TEST_F(ObservableReduceTest, ReturnsInitialValueForEmptySource)
		{
			EXPECT_CALL(observer, next(42));
			subscribe(reduce(Observable<int>::empty(), 42,
							 [](int acc, int value)
							 {
								 return acc + value;
							 }));
		}

		TEST_F(ObservableReduceTest, ReturnsInitialValueForSingleValue)
		{
			EXPECT_CALL(observer, next(5));
			subscribe(reduce(Observable<int>::just(5), 0,
							 [](int acc, int value)
							 {
								 return acc + value;
							 }));
		}

		TEST_F(ObservableReduceTest, MultipliesAllValues)
		{
			EXPECT_CALL(observer, next(24));
			subscribe(reduce(Observable<int>::fromIterable(std::array{1, 2, 3, 4}), 1,
							 [](int acc, int value)
							 {
								 return acc * value;
							 }));
		}

		TEST_F(ObservableReduceTest, ForwardsUpstreamError)
		{
			EXPECT_CALL(observer, error(failure));
			subscribe(reduce(Observable<int>::error(failure), 0,
							 [](int acc, int value)
							 {
								 return acc + value;
							 }));
		}

		TEST_F(ObservableReduceTest, PropagatesReducerException)
		{
			EXPECT_CALL(observer, error(ExceptionContaining<std::runtime_error>("reduce")));
			subscribe(reduce(Observable<int>::fromIterable(std::array{1, 2}), 0,
							 [](int, int) -> int
							 {
								 throw std::runtime_error{"reduce"};
							 }));
		}

		TEST_F(ObservableReduceTest, DefersEvaluationForEachSubscription)
		{
			StrictMock<MockFunction<int(int, int)>> reducer;
			const auto								source = reduce(Observable<int>::fromIterable(std::array{1, 2}), 0, reducer.AsStdFunction());
			InSequence								sequence;
			EXPECT_CALL(reducer, Call(0, 1)).WillOnce(::testing::Return(1));
			EXPECT_CALL(reducer, Call(1, 2)).WillOnce(::testing::Return(3));
			EXPECT_CALL(observer, next(3));
			EXPECT_CALL(reducer, Call(0, 1)).WillOnce(::testing::Return(1));
			EXPECT_CALL(reducer, Call(1, 2)).WillOnce(::testing::Return(3));
			EXPECT_CALL(observer, next(3));
			subscribe(source);
			subscribe(source);
		}

		TEST_F(ObservableReduceTest, WaitsForCompletion)
		{
			const auto source =
				reduce(Observable<int>::concat(std::array{Observable<int>::fromIterable(std::array{1, 2}), Observable<int>::empty().subscribeOn(loop)}), 0,
					   [](int acc, int value)
					   {
						   return acc + value;
					   });
			const auto subscription = subscribe(source);
			EXPECT_FALSE(subscription.isDisposed());
			EXPECT_CALL(observer, next(3));
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableReduceTest, CancelsScheduledUpstream)
		{
			StrictMock<MockFunction<Observable<int>()>> factory;
			const auto									subscription = subscribe(reduce(Observable<int>::defer(factory.AsStdFunction()).subscribeOn(loop), 0,
																						[](int acc, int value)
																						{
															return acc + value;
														}));
			EXPECT_FALSE(subscription.isDisposed());
			subscription.dispose();
			drain();
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(ObservableReduceTest, UsesStatefulReducer)
		{
			int callCount = 0;
			EXPECT_CALL(observer, next(3));
			subscribe(reduce(Observable<int>::fromIterable(std::array{1, 2}), 0,
							 [&callCount](int acc, int value)
							 {
								 ++callCount;
								 return acc + value;
							 }));
			EXPECT_EQ(callCount, 2);
		}
	} // namespace
} // namespace rppw::test
