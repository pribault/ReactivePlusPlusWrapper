// googletest
#include <gtest/gtest.h>

// rppw
#include <rppw/ReactivePlusPlusWrapper.h>

// rpp
#include <rpp/schedulers/run_loop.hpp>

// stl
#include <array>
#include <chrono>
#include <exception>
#include <future>
#include <semaphore>
#include <thread>
#include <vector>

namespace rppw::test
{
	namespace
	{
		bool drainReady(const rpp::schedulers::run_loop &loop)
		{
			for (int dispatched = 0; dispatched < 256 && loop.is_any_ready_schedulable(); ++dispatched)
				loop.dispatch_if_ready();
			return loop.is_empty();
		}

		class ReleaseCallback
		{
		public:
			explicit ReleaseCallback(std::binary_semaphore &gate)
				: gate_{gate}
			{
			}
			~ReleaseCallback()
			{
				release();
			}

			void release()
			{
				if (!released_)
				{
					released_ = true;
					gate_.release();
				}
			}

		private:
			std::binary_semaphore &gate_;
			bool				   released_ = false;
		};

		class SchedulerIntegrationTest : public ::testing::Test
		{
		protected:
			rpp::schedulers::run_loop loop;
			static constexpr auto	  waitTimeout = std::chrono::seconds{5};
		};

		TEST_F(SchedulerIntegrationTest, DeliversNotificationsOnDispatchingWorker)
		{
			const auto		   subscriberThread = std::this_thread::get_id();
			std::thread::id	   producerThread;
			std::thread::id	   callbackThread;
			std::exception_ptr error;
			int				   received = 0;
			bool			   drained = false;
			const auto		   subscription = Single<int>::fromCallable(
										  [&]
										  {
											  producerThread = std::this_thread::get_id();
											  return 7;
										  })
										  .observeOn(loop)
										  .subscribe(
											  [&](int value)
											  {
												  callbackThread = std::this_thread::get_id();
												  received = value;
											  },
											  [&](std::exception_ptr failure)
											  {
												  error = failure;
											  });
			auto	   worker = std::async(std::launch::async,
										   [&]
										   {
										   drained = drainReady(loop);
										   return std::this_thread::get_id();
									   });
			const auto workerThread = worker.get();
			EXPECT_EQ(producerThread, subscriberThread);
			EXPECT_EQ(callbackThread, workerThread);
			EXPECT_NE(callbackThread, subscriberThread);
			EXPECT_EQ(received, 7);
			EXPECT_EQ(error, nullptr);
			EXPECT_TRUE(drained);
			EXPECT_TRUE(subscription.isDisposed());
		}

		TEST_F(SchedulerIntegrationTest, CancellationAllowsEngagedCallbackToFinishWithoutFurtherSignals)
		{
			std::binary_semaphore entered{0};
			std::binary_semaphore continueCallback{0};
			std::vector<int>	  received;
			std::exception_ptr	  error;
			bool				  completed = false;
			bool				  callbackReturned = false;
			bool				  callbackWaitSucceeded = false;
			bool				  drained = false;
			const auto			  subscription = Observable<int>::fromIterable(std::array{1, 2, 3})
										  .observeOn(loop)
										  .subscribe(
											  [&](int value)
											  {
												  received.push_back(value);
												  if (value == 1)
												  {
													  entered.release();
													  callbackWaitSucceeded = continueCallback.try_acquire_for(waitTimeout);
													  callbackReturned = true;
												  }
											  },
											  [&](std::exception_ptr failure)
											  {
												  error = failure;
											  },
											  [&]
											  {
												  completed = true;
											  });
			auto worker = std::async(std::launch::async,
									 [&]
									 {
										 drained = drainReady(loop);
									 });
			// Release the callback before the future's destructor waits, including on assertion failure.
			ReleaseCallback releaseOnExit{continueCallback};
			const bool		callbackEngaged = entered.try_acquire_for(waitTimeout);
			subscription.dispose();
			const bool disposedWhileEngaged = subscription.isDisposed();
			releaseOnExit.release();
			worker.get();
			EXPECT_TRUE(callbackEngaged);
			EXPECT_TRUE(disposedWhileEngaged);
			EXPECT_TRUE(callbackWaitSucceeded);
			EXPECT_TRUE(callbackReturned);
			EXPECT_EQ(received, std::vector{1});
			EXPECT_FALSE(completed);
			EXPECT_EQ(error, nullptr);
			EXPECT_TRUE(drained);
			EXPECT_TRUE(subscription.isDisposed());
		}
	} // namespace
} // namespace rppw::test
