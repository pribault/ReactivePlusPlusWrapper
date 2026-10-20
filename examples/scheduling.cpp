// stl
#include <exception>
#include <iostream>

// rpp
#include <rpp/schedulers/run_loop.hpp>

// rppw
#include <rppw/Single.h>

int main()
{
	rpp::schedulers::run_loop producer;
	rpp::schedulers::run_loop consumer;
	bool					  complete = false;
	bool					  failed = false;
	const auto				  source = rppw::Single<int>::fromCallable(
							   []
							   {
								   return 42;
							   })
							.subscribeOn(producer)
							.observeOn(consumer);
	const auto subscription = source.subscribe(
		[&complete](int value)
		{
			std::cout << value << '\n';
			complete = true;
		},
		[&failed](std::exception_ptr)
		{
			failed = true;
			std::cerr << "Scheduled action failed\n";
		});
	std::cout << "queued\n";
	// Both queues are explicitly pumped on this thread; no worker thread is created.
	for (int step = 0; step < 64 && !complete && !failed; ++step)
	{
		producer.dispatch_if_ready();
		consumer.dispatch_if_ready();
	}
	subscription.dispose();
	if (!complete && !failed)
		std::cerr << "Dispatch budget exhausted\n";

	bool	   unexpected = false;
	const auto cancelled = source.subscribe(
		[&unexpected](int)
		{
			unexpected = true;
		},
		[&failed](std::exception_ptr)
		{
			failed = true;
			std::cerr << "Cancelled source failed\n";
		});
	cancelled.dispose();
	producer.dispatch_if_ready();
	consumer.dispatch_if_ready();
	if (unexpected)
		std::cerr << "Cancellation failed\n";
	else
		std::cout << "cancelled\n";
	return !complete || failed || unexpected ? 1 : 0;
}
