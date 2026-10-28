// googletest
#include <gtest/gtest.h>

// rppw
#include <rppw/ReactivePlusPlusWrapper.h>

// stl
#include <memory>
#include <thread>
#include <utility>

namespace rppw::test
{
	namespace
	{
		class CompositeDisposableTest : public ::testing::Test
		{
		};

		TEST_F(CompositeDisposableTest, DefaultScopeIsActive)
		{
			const CompositeDisposable scope;
			EXPECT_FALSE(scope.isDisposed());
		}

		TEST_F(CompositeDisposableTest, DisposesAttachedChildren)
		{
			const CompositeDisposable scope;
			const auto				  first = Disposable::create();
			const auto				  second = Disposable::create();
			scope.add(first);
			scope.add(second);
			scope.dispose();
			scope.dispose();
			EXPECT_TRUE(scope.isDisposed());
			EXPECT_TRUE(first.isDisposed());
			EXPECT_TRUE(second.isDisposed());
		}

		TEST_F(CompositeDisposableTest, DisposesChildAddedAfterDisposal)
		{
			const CompositeDisposable scope;
			scope.dispose();
			const auto child = Disposable::create();
			scope.add(child);
			EXPECT_TRUE(child.isDisposed());
		}

		TEST_F(CompositeDisposableTest, RemovesChildWithoutCancellingIt)
		{
			const CompositeDisposable scope;
			const auto				  child = Disposable::create();
			scope.add(child);
			scope.remove(child);
			EXPECT_FALSE(child.isDisposed());
			scope.dispose();
			EXPECT_FALSE(child.isDisposed());
		}

		TEST_F(CompositeDisposableTest, RemovesDuplicateRegistrationsThroughCopy)
		{
			const CompositeDisposable scope;
			const auto				  child = Disposable::create();
			const auto				  copy = child;
			scope.add(child);
			scope.add(child);
			scope.remove(copy);
			scope.dispose();
			EXPECT_FALSE(child.isDisposed());
		}

		TEST_F(CompositeDisposableTest, RemovingDisposedChildReleasesOwnership)
		{
			const CompositeDisposable						   scope;
			std::weak_ptr<rpp::interface_composite_disposable> weak;
			{
				auto native = rpp::composite_disposable_wrapper::make();
				weak = native.lock();
				const Disposable child{std::move(native)};
				scope.add(child);
				child.dispose();
				scope.remove(child);
			}
			EXPECT_TRUE(weak.expired());
			EXPECT_FALSE(scope.isDisposed());
		}

		TEST_F(CompositeDisposableTest, RemovingMissingChildHasNoEffect)
		{
			const CompositeDisposable scope;
			const auto				  attached = Disposable::create();
			const auto				  missing = Disposable::create();
			scope.add(attached);
			scope.remove(missing);
			scope.dispose();
			EXPECT_TRUE(attached.isDisposed());
			EXPECT_FALSE(missing.isDisposed());
		}

		TEST_F(CompositeDisposableTest, CopiesShareCancellation)
		{
			const CompositeDisposable scope;
			const auto				  copy = scope;
			const auto				  child = Disposable::create();
			copy.add(child);
			scope.dispose();
			EXPECT_TRUE(copy.isDisposed());
			EXPECT_TRUE(child.isDisposed());
		}

		TEST_F(CompositeDisposableTest, SeparateScopesCancelIndependently)
		{
			const CompositeDisposable first;
			const CompositeDisposable second;
			first.dispose();
			EXPECT_FALSE(second.isDisposed());
		}

		TEST_F(CompositeDisposableTest, DestroyedScopeDoesNotCancelChild)
		{
			const auto child = Disposable::create();
			{
				const CompositeDisposable scope;
				scope.add(child);
			}
			EXPECT_FALSE(child.isDisposed());
		}

		TEST_F(CompositeDisposableTest, DestroyedCopyDoesNotCancelScope)
		{
			const CompositeDisposable scope;
			{
				const auto copy = scope;
			}
			EXPECT_FALSE(scope.isDisposed());
		}

		TEST_F(CompositeDisposableTest, AcceptsEmptyOrDisposedChildren)
		{
			const CompositeDisposable scope;
			const Disposable		  empty;
			const auto				  disposed = Disposable::create();
			disposed.dispose();
			scope.add(empty);
			scope.add(disposed);
			scope.remove(empty);
			scope.remove(disposed);
			EXPECT_FALSE(scope.isDisposed());
			scope.dispose();
			EXPECT_TRUE(scope.isDisposed());
		}

		TEST_F(CompositeDisposableTest, MovedFromScopeIsDisposed)
		{
			CompositeDisposable		  original;
			const CompositeDisposable moved{std::move(original)};
			const auto				  child = Disposable::create();
			original.add(child);
			original.remove(child);
			original.dispose();
			EXPECT_TRUE(original.isDisposed());
			EXPECT_TRUE(child.isDisposed());
			EXPECT_FALSE(moved.isDisposed());
		}

		TEST_F(CompositeDisposableTest, ConcurrentAddAndDisposalCancelChild)
		{
			const CompositeDisposable scope;
			const auto				  child = Disposable::create();
			std::thread				  adding{[&scope, &child]
								 {
									 scope.add(child);
								 }};
			scope.dispose();
			adding.join();
			EXPECT_TRUE(scope.isDisposed());
			EXPECT_TRUE(child.isDisposed());
		}
	} // namespace
} // namespace rppw::test
