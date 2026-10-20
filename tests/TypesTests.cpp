// googletest
#include <gtest/gtest.h>

// rppw
#include <rppw/Completable.h>
#include <rppw/Concepts.h>
#include <rppw/Disposable.h>
#include <rppw/Maybe.h>
#include <rppw/Observable.h>
#include <rppw/Single.h>

// rpp
#include <rpp/schedulers/run_loop.hpp>

// stl
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// tests
#include <support/ReactiveTest.h>

namespace rppw::test
{
	namespace
	{
		template <typename T>
		concept ObservableValue = requires { typename Observable<T>; };

		struct CopyOnlyError : std::runtime_error
		{
			using std::runtime_error::runtime_error;
			CopyOnlyError(const CopyOnlyError &) = default;
			CopyOnlyError(CopyOnlyError &&) = delete;
			CopyOnlyError &operator=(const CopyOnlyError &) = delete;
		};

		template <typename Source, typename E>
		concept AcceptsError = requires(E &&error) {
			{ Source::error(std::forward<E>(error)) } -> std::same_as<Source>;
		};

		template <typename E>
		concept AcceptedErrorByEverySource =
			AcceptsError<Observable<int>, E> && AcceptsError<Single<int>, E> && AcceptsError<Maybe<int>, E> && AcceptsError<Completable, E>;

		static_assert(AcceptedErrorByEverySource<std::runtime_error>);
		static_assert(AcceptedErrorByEverySource<const std::runtime_error &>);
		static_assert(AcceptedErrorByEverySource<int>);
		static_assert(AcceptedErrorByEverySource<CopyOnlyError &>);
		static_assert(AcceptedErrorByEverySource<std::exception_ptr>);
		static_assert(AcceptedErrorByEverySource<std::exception_ptr &>);
		static_assert(AcceptedErrorByEverySource<const std::exception_ptr &>);
		static_assert(AcceptedErrorByEverySource<std::nullptr_t>);
		static_assert(!AcceptsError<Observable<int>, std::unique_ptr<int>>);
		static_assert(!AcceptsError<Single<int>, std::unique_ptr<int>>);
		static_assert(!AcceptsError<Maybe<int>, std::unique_ptr<int>>);
		static_assert(!AcceptsError<Completable, std::unique_ptr<int>>);
		static_assert(std::is_copy_constructible_v<CopyOnlyError>);
		static_assert(!std::copy_constructible<CopyOnlyError>);

		struct Stringify
		{
			std::string operator()(int value) const
			{
				return std::to_string(value);
			}
		};

		struct ObservableFactory
		{
			Observable<int> operator()() const
			{
				return Observable<int>::just(1);
			}
		};

		struct SingleFactory
		{
			Single<int> operator()() const
			{
				return Single<int>::just(1);
			}
		};

		struct IntCallable
		{
			int operator()() const
			{
				return 1;
			}
		};

		struct OptionalCallable
		{
			std::optional<int> operator()() const
			{
				return 1;
			}
		};

		template <typename Factory>
		concept AcceptsObservableFactory = requires(Factory factory) { Observable<int>::defer(factory); };

		template <typename Factory>
		concept AcceptsSingleCallable = requires(Factory factory) { Single<int>::fromCallable(factory); };

		template <typename Factory>
		concept AcceptsMaybeCallable = requires(Factory factory) { Maybe<int>::fromCallable(factory); };

		template <typename Source, typename F>
		concept AcceptsDefer = requires(F factory) { Source::defer(factory); };

		template <typename F>
		concept AcceptsAction = requires(F action) { Completable::fromAction(action); };

		template <typename Source, typename F>
		concept AcceptsMap = requires(const Source &source, F function) { source.map(function); };

		template <typename Source, typename F>
		concept AcceptsFlatMap = requires(const Source &source, F function) { source.flatMap(function); };

		template <typename Source, typename F>
		concept AcceptsFilter = requires(const Source &source, F function) { source.filter(function); };

		template <typename F>
		concept AcceptsPrefixPredicates = requires(const Observable<int> &source, F predicate) {
			{ source.takeUntil(predicate) } -> std::same_as<Observable<int>>;
			{ source.takeWhile(predicate) } -> std::same_as<Observable<int>>;
			{ source.dropUntil(predicate) } -> std::same_as<Observable<int>>;
			{ source.dropWhile(predicate) } -> std::same_as<Observable<int>>;
		};

		static_assert(AcceptsPrefixPredicates<decltype([](const int &value) { return value > 0; })>);
		static_assert(AcceptsPrefixPredicates<decltype([](int value) { return value; })>);
		static_assert(!AcceptsPrefixPredicates<decltype([](int) {})>);
		static_assert(!AcceptsPrefixPredicates<decltype([](int &&) { return true; })>);
		static_assert(!AcceptsPrefixPredicates<decltype([flag = false](int) mutable { return flag = !flag; })>);

		template <typename Source, typename... Callbacks>
		concept AcceptsSubscribe = requires(const Source &source, Callbacks... callbacks) { source.subscribe(callbacks...); };

		template <typename R>
		concept AcceptsRange = requires(R &&range) { Observable<int>::fromIterable(std::forward<R>(range)); };

		template <typename R>
		concept AcceptsObservableComposition = requires(R &&sources) {
			{ Observable<int>::concat(std::forward<R>(sources)) } -> std::same_as<Observable<int>>;
			{ Observable<int>::merge(std::forward<R>(sources)) } -> std::same_as<Observable<int>>;
		};

		static_assert(AcceptsObservableComposition<std::vector<Observable<int>>>);
		static_assert(AcceptsObservableComposition<const std::vector<Observable<int>> &>);
		static_assert(AcceptsObservableComposition<std::vector<Single<int>> &>);
		static_assert(AcceptsObservableComposition<std::vector<Maybe<int>>>);
		static_assert(AcceptsObservableComposition<std::ranges::empty_view<Observable<int>>>);
		static_assert(!AcceptsObservableComposition<std::vector<Observable<long>>>);
		static_assert(!AcceptsObservableComposition<std::vector<Completable>>);
		static_assert(!AcceptsObservableComposition<std::vector<int>>);
		static_assert(!AcceptsObservableComposition<int>);

		template <typename R>
		concept AcceptsAndThen = requires(R source) { Completable::complete().andThen(source); };

		template <typename F>
		concept RejectedByEveryMap = !AcceptsMap<Observable<int>, F> && !AcceptsMap<Single<int>, F> && !AcceptsMap<Maybe<int>, F>;

		template <typename F>
		concept RejectedByEveryFlatMap = !AcceptsFlatMap<Observable<int>, F> && !AcceptsFlatMap<Single<int>, F> && !AcceptsFlatMap<Maybe<int>, F>;

		struct SourceImpostor
		{
			using value_type = int;
		};

		struct InvalidValueSource
		{
			using value_type = void;
		};

		using VoidMapper = decltype([](int) {});
		using MoveOnlyMapper = decltype([](int) { return std::unique_ptr<int>{}; });
		using ReferenceMapper = decltype([](const int &value) -> const int & { return value; });
		using RvalueMapper = decltype([](int &&value) { return value; });
		using MutableMapper = decltype([value = 0](int) mutable { return ++value; });
		using IntCallback = decltype([](int) { return 1; });
		using ErrorCallback = decltype([](std::exception_ptr &) { return 1; });
		using CompletionCallback = decltype([] { return 1; });
		using MutableErrorCallback = decltype([value = 0](std::exception_ptr) mutable { return ++value; });
		using MutableCompletionCallback = decltype([value = 0]() mutable { return ++value; });
		using RvalueErrorCallback = decltype([](std::exception_ptr &&) {});
		using MaybeFactory = decltype([] { return Maybe<int>::empty(); });
		using CompletableFactory = decltype([] { return Completable::complete(); });
		using MutableFactory = decltype([value = 0]() mutable { return ++value; });
		using MutableOptionalCallable = decltype([value = 0]() mutable { return std::optional<int>{++value}; });
		using MutableObservableFactory = decltype([value = 0]() mutable { return Observable<int>::just(++value); });
		using ObservableMapper = decltype([](int) { return Observable<int>::just(1); });
		using SingleMapper = decltype([](int) { return Single<int>::just(1); });
		using MaybeMapper = decltype([](int) { return Maybe<int>::empty(); });
		using ImpostorMapper = decltype([](int) { return SourceImpostor{}; });
		using InvalidValueMapper = decltype([](int) { return InvalidValueSource{}; });
		using MutableObservableMapper = decltype([value = 0](int) mutable { return Observable<int>::just(++value); });
		using Action = decltype([] {});

		static_assert(ObservableValue<int>);
		static_assert(!ObservableValue<void>);
		static_assert(!ObservableValue<int &>);
		static_assert(!ObservableValue<const int>);
		static_assert(!ObservableValue<std::unique_ptr<int>>);
		static_assert(AcceptsObservableFactory<ObservableFactory>);
		static_assert(!AcceptsObservableFactory<SingleFactory>);
		static_assert(concepts::SourceFactoryFor<ObservableFactory, int, Observable>);
		static_assert(concepts::SourceFactoryFor<SingleFactory, int, Single>);
		static_assert(concepts::SourceFactoryFor<MaybeFactory, int, Maybe>);
		static_assert(!concepts::SourceFactoryFor<SingleFactory, int, Observable>);
		static_assert(!concepts::SourceFactoryFor<ObservableFactory, int, Single>);
		static_assert(!concepts::SourceFactoryFor<SingleFactory, int, Maybe>);
		static_assert(!concepts::SourceFactoryFor<MutableObservableFactory, int, Observable>);
		static_assert(!concepts::SourceFactoryFor<int, int, Observable>);
		static_assert(AcceptsSingleCallable<IntCallable>);
		static_assert(!AcceptsSingleCallable<OptionalCallable>);
		static_assert(AcceptsMaybeCallable<OptionalCallable>);
		static_assert(!AcceptsMaybeCallable<IntCallable>);
		static_assert(AcceptsDefer<Single<int>, SingleFactory>);
		static_assert(AcceptsDefer<Maybe<int>, MaybeFactory>);
		static_assert(AcceptsDefer<Completable, CompletableFactory>);
		static_assert(!AcceptsDefer<Single<int>, ObservableFactory>);
		static_assert(!AcceptsDefer<Maybe<int>, SingleFactory>);
		static_assert(!AcceptsDefer<Completable, MaybeFactory>);
		static_assert(!AcceptsDefer<Observable<int>, int>);
		static_assert(!AcceptsSingleCallable<MutableFactory>);
		static_assert(!AcceptsMaybeCallable<MutableOptionalCallable>);
		static_assert(!AcceptsDefer<Observable<int>, MutableObservableFactory>);
		static_assert(AcceptsAction<Action>);
		static_assert(!AcceptsAction<IntCallable>);
		static_assert(!AcceptsAction<MutableCompletionCallback>);

		static_assert(RejectedByEveryMap<VoidMapper>);
		static_assert(RejectedByEveryMap<MoveOnlyMapper>);
		static_assert(RejectedByEveryMap<ReferenceMapper>);
		static_assert(RejectedByEveryMap<RvalueMapper>);
		static_assert(RejectedByEveryMap<MutableMapper>);
		static_assert(RejectedByEveryMap<int>);
		static_assert(AcceptsMap<Observable<int>, Stringify>);
		static_assert(AcceptsMap<Single<int>, Stringify>);
		static_assert(AcceptsMap<Maybe<int>, Stringify>);
		static_assert(AcceptsFilter<Observable<int>, IntCallback>);
		static_assert(AcceptsFilter<Single<int>, IntCallback>);
		static_assert(AcceptsFilter<Maybe<int>, IntCallback>);
		static_assert(!AcceptsFilter<Observable<int>, Stringify>);
		static_assert(!AcceptsFilter<Single<int>, VoidMapper>);
		static_assert(!AcceptsFilter<Maybe<int>, MutableMapper>);

		static_assert(AcceptsFlatMap<Observable<int>, ObservableMapper>);
		static_assert(AcceptsFlatMap<Single<int>, SingleMapper>);
		static_assert(AcceptsFlatMap<Maybe<int>, MaybeMapper>);
		static_assert(!AcceptsFlatMap<Observable<int>, SingleMapper>);
		static_assert(!AcceptsFlatMap<Observable<int>, MaybeMapper>);
		static_assert(!AcceptsFlatMap<Single<int>, ObservableMapper>);
		static_assert(!AcceptsFlatMap<Single<int>, MaybeMapper>);
		static_assert(!AcceptsFlatMap<Maybe<int>, ObservableMapper>);
		static_assert(!AcceptsFlatMap<Maybe<int>, SingleMapper>);
		static_assert(RejectedByEveryFlatMap<VoidMapper>);
		static_assert(RejectedByEveryFlatMap<IntCallback>);
		static_assert(RejectedByEveryFlatMap<ImpostorMapper>);
		static_assert(RejectedByEveryFlatMap<InvalidValueMapper>);
		static_assert(RejectedByEveryFlatMap<MutableObservableMapper>);
		static_assert(RejectedByEveryFlatMap<MutableMapper>);
		static_assert(RejectedByEveryFlatMap<ReferenceMapper>);

		static_assert(AcceptsSubscribe<Observable<int>, IntCallback, ErrorCallback, CompletionCallback>);
		static_assert(AcceptsSubscribe<Single<int>, IntCallback, ErrorCallback>);
		static_assert(AcceptsSubscribe<Maybe<int>, IntCallback, ErrorCallback, CompletionCallback>);
		static_assert(AcceptsSubscribe<Completable, CompletionCallback, ErrorCallback>);
		static_assert(!AcceptsSubscribe<Observable<int>, MutableMapper, ErrorCallback, CompletionCallback>);
		static_assert(!AcceptsSubscribe<Observable<int>, IntCallback, MutableErrorCallback, CompletionCallback>);
		static_assert(!AcceptsSubscribe<Observable<int>, IntCallback, ErrorCallback, MutableCompletionCallback>);
		static_assert(!AcceptsSubscribe<Observable<int>, ErrorCallback, ErrorCallback, CompletionCallback>);
		static_assert(!AcceptsSubscribe<Single<int>, MutableMapper, ErrorCallback>);
		static_assert(!AcceptsSubscribe<Single<int>, IntCallback, MutableErrorCallback>);
		static_assert(!AcceptsSubscribe<Maybe<int>, MutableMapper, ErrorCallback, CompletionCallback>);
		static_assert(!AcceptsSubscribe<Maybe<int>, IntCallback, MutableErrorCallback, CompletionCallback>);
		static_assert(!AcceptsSubscribe<Maybe<int>, IntCallback, ErrorCallback, MutableCompletionCallback>);
		static_assert(!AcceptsSubscribe<Completable, MutableCompletionCallback, ErrorCallback>);
		static_assert(!AcceptsSubscribe<Completable, CompletionCallback, MutableErrorCallback>);
		static_assert(!AcceptsSubscribe<Completable, CompletionCallback, RvalueErrorCallback>);

		static_assert(AcceptsRange<std::vector<int>>);
		static_assert(AcceptsRange<std::vector<short> &>);
		static_assert(AcceptsRange<decltype(std::views::iota(0, 3))>);
		static_assert(!AcceptsRange<int>);
		static_assert(!AcceptsRange<std::vector<std::string>>);
		static_assert(AcceptsAndThen<Observable<int>>);
		static_assert(AcceptsAndThen<Single<int>>);
		static_assert(AcceptsAndThen<Maybe<int>>);
		static_assert(AcceptsAndThen<Completable>);
		static_assert(!AcceptsAndThen<int>);
		static_assert(!AcceptsAndThen<void>);
		static_assert(!AcceptsAndThen<SourceImpostor>);
		static_assert(!AcceptsAndThen<InvalidValueSource>);

		static_assert(std::same_as<Observable<int>::value_type, int>);
		static_assert(std::same_as<Single<int>::value_type, int>);
		static_assert(std::same_as<Maybe<int>::value_type, int>);
		static_assert(std::same_as<Completable::value_type, std::monostate>);
		static_assert(std::same_as<decltype(Observable<int>::just(1).map(Stringify{})), Observable<std::string>>);
		static_assert(std::same_as<decltype(Single<int>::just(1).map(Stringify{})), Single<std::string>>);
		static_assert(std::same_as<decltype(Maybe<int>::just(1).map(Stringify{})), Maybe<std::string>>);
		static_assert(std::same_as<decltype(Observable<int>::just(1).flatMap(
									   [](int)
									   {
										   return Observable<std::string>::just("value");
									   })),
								   Observable<std::string>>);
		static_assert(std::same_as<decltype(Single<int>::just(1).flatMap(
									   [](int)
									   {
										   return Single<std::string>::just("value");
									   })),
								   Single<std::string>>);
		static_assert(std::same_as<decltype(Maybe<int>::just(1).flatMap(
									   [](int)
									   {
										   return Maybe<std::string>::just("value");
									   })),
								   Maybe<std::string>>);
		static_assert(std::same_as<decltype(Single<int>::just(1).filter(
									   [](int)
									   {
										   return true;
									   })),
								   Maybe<int>>);
		static_assert(std::same_as<decltype(Maybe<int>::empty().defaultIfEmpty(1)), Single<int>>);
		static_assert(std::same_as<decltype(Observable<int>::empty().singleOrError()), Single<int>>);
		static_assert(std::same_as<decltype(Observable<int>::empty().singleElement()), Maybe<int>>);
		static_assert(std::same_as<decltype(Observable<int>::empty().ignoreElements()), Completable>);
		static_assert(std::same_as<decltype(Single<int>::just(1).ignoreElement()), Completable>);
		static_assert(std::same_as<decltype(Maybe<int>::empty().ignoreElement()), Completable>);
		static_assert(std::same_as<decltype(Single<int>::just(1).toObservable()), Observable<int>>);
		static_assert(std::same_as<decltype(Completable::complete().toObservable()), Observable<std::monostate>>);
		static_assert(std::same_as<decltype(Completable::complete().toMaybe()), Maybe<std::monostate>>);
		static_assert(std::same_as<decltype(Completable::complete().toObservable<int>()), Observable<int>>);
		static_assert(std::same_as<decltype(Completable::complete().toMaybe<int>()), Maybe<int>>);
		static_assert(std::same_as<decltype(Single<int>::just(1).toMaybe()), Maybe<int>>);
		static_assert(std::same_as<decltype(Maybe<int>::empty().toObservable()), Observable<int>>);
		static_assert(std::same_as<decltype(Maybe<int>::empty().toSingle()), Single<int>>);
		static_assert(std::same_as<decltype(Completable::complete().andThen(Observable<int>::empty())), Observable<int>>);
		static_assert(std::same_as<decltype(Completable::complete().andThen(Single<int>::just(1))), Single<int>>);
		static_assert(std::same_as<decltype(Completable::complete().andThen(Maybe<int>::empty())), Maybe<int>>);
		static_assert(std::same_as<decltype(Completable::complete().andThen(Completable::complete())), Completable>);

		static_assert(!std::is_convertible_v<Observable<int>, Single<int>>);
		static_assert(!std::is_convertible_v<Observable<int>, Maybe<int>>);
		static_assert(!std::is_convertible_v<Observable<int>, Completable>);
		static_assert(!std::is_convertible_v<Single<int>, Observable<int>>);
		static_assert(!std::is_convertible_v<Single<int>, Maybe<int>>);
		static_assert(!std::is_convertible_v<Maybe<int>, Single<int>>);
		static_assert(!std::is_convertible_v<Maybe<int>, Observable<int>>);

		template <typename Source>
		constexpr bool preservesSchedulerType = std::same_as<decltype(std::declval<Source>().subscribeOn(std::declval<rpp::schedulers::run_loop>())), Source> &&
												std::same_as<decltype(std::declval<Source>().observeOn(std::declval<rpp::schedulers::run_loop>())), Source>;

		static_assert(preservesSchedulerType<Observable<int>>);
		static_assert(preservesSchedulerType<Single<int>>);
		static_assert(preservesSchedulerType<Maybe<int>>);
		static_assert(preservesSchedulerType<Completable>);

		template <typename Source>
		class ErrorFactoryTest : public ReactiveTest
		{
		};

		using ErrorSources = ::testing::Types<Observable<int>, Single<int>, Maybe<int>, Completable>;
		TYPED_TEST_SUITE(ErrorFactoryTest, ErrorSources);

		TYPED_TEST(ErrorFactoryTest, CapturesExceptionObject)
		{
			EXPECT_CALL(this->observer, error(ExceptionContaining<std::logic_error>("object")));
			this->subscribe(TypeParam::error(std::logic_error{"object"}));
		}

		TYPED_TEST(ErrorFactoryTest, OwnsCopiedErrorForEverySubscription)
		{
			const auto source = []
			{
				const std::runtime_error error{"copied"};
				return TypeParam::error(error);
			}();
			EXPECT_CALL(this->observer, error(ExceptionContaining<std::runtime_error>("copied"))).Times(2);
			this->subscribe(source);
			this->subscribe(source);
		}

		TYPED_TEST(ErrorFactoryTest, PreservesNonStandardExceptionType)
		{
			std::exception_ptr captured;
			EXPECT_CALL(this->observer, error(::testing::_)).WillOnce(::testing::SaveArg<0>(&captured));
			this->subscribe(TypeParam::error(42));
			ASSERT_TRUE(captured);
			EXPECT_THROW(std::rethrow_exception(captured), int);
		}

		TYPED_TEST(ErrorFactoryTest, SupportsCopyOnlyExceptions)
		{
			EXPECT_CALL(this->observer, error(ExceptionContaining<CopyOnlyError>("copy-only")));
			this->subscribe(TypeParam::error(CopyOnlyError{"copy-only"}));
		}

		TYPED_TEST(ErrorFactoryTest, RejectsNullPointerLiteral)
		{
			EXPECT_THROW(TypeParam::error(nullptr), std::invalid_argument);
		}

		class TypesTest : public ::testing::Test
		{
		};

		TEST_F(TypesTest, SupportsNonIntegralMappedValuesAtRuntime)
		{
			std::string result;
			Single<int>::just(42)
				.map(Stringify{})
				.subscribe(
					[&](std::string value)
					{
						result = std::move(value);
					},
					[](std::exception_ptr)
					{
						ADD_FAILURE() << "Unexpected error";
					});
			EXPECT_EQ(result, "42");
		}

		TEST_F(TypesTest, IgnoresSubscriptionCallbackResults)
		{
			int result = 0;
			Single<int>::just(42).subscribe(
				[&](int value)
				{
					return result = value;
				},
				[](std::exception_ptr)
				{
					return false;
				});
			EXPECT_EQ(result, 42);
		}

		TEST_F(TypesTest, PassesErrorsToLvalueCallbacks)
		{
			bool received = false;
			Completable::error(std::make_exception_ptr(std::runtime_error{"failure"}))
				.subscribe([] {},
						   [&](std::exception_ptr &error)
						   {
							   received = static_cast<bool>(error);
						   });
			EXPECT_TRUE(received);
		}
	} // namespace
} // namespace rppw::test
