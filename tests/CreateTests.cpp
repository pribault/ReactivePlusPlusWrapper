// tests
#include <support/ReactiveTest.h>

// stl
#include <optional>

namespace rppw::test
{
	namespace
	{
		using ::testing::InSequence;

		template <typename Source>
		class CreateTest : public ReactiveTest
		{
		protected:
			using Observer = rpp::dynamic_observer<typename Source::value_type>;
			std::optional<Observer> retained;
		};

		using Sources = ::testing::Types<Observable<int>, Single<int>, Maybe<int>, Completable>;
		TYPED_TEST_SUITE(CreateTest, Sources);

		TYPED_TEST(CreateTest, InvokesProducerLazilyForEverySubscription)
		{
			int		   calls = 0;
			const auto source = TypeParam::create(
				[&calls](typename TestFixture::Observer)
				{
					++calls;
				});
			EXPECT_EQ(calls, 0);
			this->subscribe(source).dispose();
			this->subscribe(source).dispose();
			EXPECT_EQ(calls, 2);
		}

		TYPED_TEST(CreateTest, ForwardsProducerError)
		{
			EXPECT_CALL(this->observer, error(this->failure));
			this->subscribe(TypeParam::create(
				[this](typename TestFixture::Observer target)
				{
					target.on_error(this->failure);
				}));
		}

		TYPED_TEST(CreateTest, RoutesProducerExceptionToError)
		{
			EXPECT_CALL(this->observer, error(ExceptionContaining<std::runtime_error>("producer")));
			EXPECT_NO_THROW(this->subscribe(TypeParam::create(
				[](typename TestFixture::Observer)
				{
					throw std::runtime_error{"producer"};
				})));
		}

		TYPED_TEST(CreateTest, DeliversRetainedObserverError)
		{
			this->subscribe(TypeParam::create(
				[this](typename TestFixture::Observer target)
				{
					this->retained = std::move(target);
				}));
			ASSERT_TRUE(this->retained.has_value());
			EXPECT_CALL(this->observer, error(this->failure));
			this->retained->on_error(this->failure);
		}

		TYPED_TEST(CreateTest, SuppressesRetainedNotificationsAfterDisposal)
		{
			const auto subscription = this->subscribe(TypeParam::create(
				[this](typename TestFixture::Observer target)
				{
					this->retained = std::move(target);
				}));
			subscription.dispose();
			ASSERT_TRUE(this->retained.has_value());
			EXPECT_TRUE(this->retained->is_disposed());
			this->retained->on_next(typename TypeParam::value_type{});
			this->retained->on_error(this->failure);
			this->retained->on_completed();
		}

		TYPED_TEST(CreateTest, DisposesProducerUpstreamOnCancellation)
		{
			const auto upstream = rpp::composite_disposable_wrapper::make();
			const auto subscription = this->subscribe(TypeParam::create(
				[upstream](typename TestFixture::Observer target)
				{
					target.set_upstream(upstream);
				}));
			EXPECT_FALSE(upstream.is_disposed());
			subscription.dispose();
			EXPECT_TRUE(upstream.is_disposed());
		}

		template <typename Source, typename F>
		concept AcceptsCreate = requires(F producer) {
			{ Source::create(producer) } -> std::same_as<Source>;
		};

		TYPED_TEST(CreateTest, ConstrainsProducerToConstInvocableObserverCallback)
		{
			const auto valid = [](typename TestFixture::Observer) {};
			const auto wrongArgument = [](std::string) {};
			const auto mutableProducer = [value = 0](typename TestFixture::Observer) mutable
			{
				++value;
			};
			static_assert(AcceptsCreate<TypeParam, decltype(valid)>);
			static_assert(!AcceptsCreate<TypeParam, decltype(wrongArgument)>);
			static_assert(!AcceptsCreate<TypeParam, decltype(mutableProducer)>);
			SUCCEED();
		}

		class CreateSignalsTest : public ReactiveTest
		{
		};

		TEST_F(CreateSignalsTest, ObservableForwardsMultipleValuesThenCompletion)
		{
			InSequence sequence;
			EXPECT_CALL(observer, next(1));
			EXPECT_CALL(observer, next(2));
			EXPECT_CALL(observer, complete());
			EXPECT_TRUE(subscribe(Observable<int>::create(
									  [](rpp::dynamic_observer<int> target)
									  {
										  target.on_next(1);
										  target.on_next(2);
										  target.on_completed();
									  }))
							.isDisposed());
		}

		TEST_F(CreateSignalsTest, SingleTerminatesOnRetainedSuccess)
		{
			std::optional<rpp::dynamic_observer<int>> retained;
			const auto								  subscription = subscribe(Single<int>::create(
				   [&retained](auto target)
				   {
					   retained = std::move(target);
				   }));
			ASSERT_TRUE(retained.has_value());
			EXPECT_CALL(observer, next(7));
			retained->on_next(7);
			EXPECT_TRUE(subscription.isDisposed());
			EXPECT_TRUE(retained->is_disposed());
		}

		TEST_F(CreateSignalsTest, MaybeTerminatesOnSuccessWithoutEmptyCompletion)
		{
			EXPECT_CALL(observer, next(7));
			EXPECT_TRUE(subscribe(Maybe<int>::create(
									  [](auto target)
									  {
										  target.on_next(7);
									  }))
							.isDisposed());
		}

		TEST_F(CreateSignalsTest, MaybeAcceptsEmptyCompletion)
		{
			EXPECT_CALL(observer, complete());
			EXPECT_TRUE(subscribe(Maybe<int>::create(
									  [](auto target)
									  {
										  target.on_completed();
									  }))
							.isDisposed());
		}

		TEST_F(CreateSignalsTest, CompletableIgnoresValuesUntilCompletion)
		{
			EXPECT_CALL(observer, complete());
			EXPECT_TRUE(subscribe(Completable::create(
									  [](rpp::dynamic_observer<std::monostate> target)
									  {
										  target.on_next({});
										  target.on_completed();
									  }))
							.isDisposed());
		}

		template <typename Source>
		class CreateConversionTest : public ReactiveTest
		{
		};

		using ScalarSources = ::testing::Types<Single<int>, Maybe<int>>;
		TYPED_TEST_SUITE(CreateConversionTest, ScalarSources);

		TYPED_TEST(CreateConversionTest, CompletesConvertedObservableOnFirstValue)
		{
			InSequence sequence;
			EXPECT_CALL(this->observer, next(7));
			EXPECT_CALL(this->observer, complete());
			this->subscribe(TypeParam::create(
								[](auto target)
								{
									target.on_next(7);
									EXPECT_TRUE(target.is_disposed());
								})
								.toObservable());
		}
	} // namespace
} // namespace rppw::test
