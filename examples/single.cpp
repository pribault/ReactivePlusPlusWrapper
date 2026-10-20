// stl
#include <exception>
#include <iostream>
#include <stdexcept>

// rppw
#include <rppw/Single.h>

int main()
{
	bool failed = false;
	rppw::Single<int>::just(21)
		.map(
			[](int value)
			{
				return value * 2;
			})
		.subscribe(
			[](int value)
			{
				std::cout << value << '\n';
			},
			[&failed](std::exception_ptr)
			{
				failed = true;
				std::cerr << "Calculation failed\n";
			});

	bool handled = false;
	rppw::Single<int>::fromCallable(
		[]() -> int
		{
			throw std::runtime_error{"expected failure"};
		})
		.subscribe(
			[&failed](int)
			{
				failed = true;
			},
			[&handled](std::exception_ptr error)
			{
				try
				{
					std::rethrow_exception(error);
				}
				catch (const std::runtime_error &exception)
				{
					handled = true;
					std::cout << exception.what() << '\n';
				}
			});
	return failed || !handled ? 1 : 0;
}
