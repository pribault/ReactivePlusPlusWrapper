#pragma once

// tests
#include <support/ReactiveTest.h>

// rpp
#include <rpp/schedulers/run_loop.hpp>

namespace rppw::test
{
	class RunLoopTest : public ReactiveTest
	{
	protected:
		template <typename Predicate>
		void dispatchUntil(Predicate predicate)
		{
			for (int steps = 0; steps < 256 && !predicate() && loop.is_any_ready_schedulable(); ++steps)
				loop.dispatch_if_ready();
			EXPECT_TRUE(predicate()) << "Expected notification was not dispatched within 256 steps";
		}

		void drain()
		{
			drain(loop);
		}

		void drain(const rpp::schedulers::run_loop &scheduler)
		{
			for (int steps = 0; steps < 256 && scheduler.is_any_ready_schedulable(); ++steps)
				scheduler.dispatch_if_ready();
			EXPECT_TRUE(scheduler.is_empty()) << "Immediate work did not drain within 256 dispatches";
		}

		rpp::schedulers::run_loop loop;
	};
} // namespace rppw::test
