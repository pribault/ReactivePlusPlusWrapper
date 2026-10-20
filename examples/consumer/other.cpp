// stl
#include <exception>

// rppw
#include <rppw/Single.h>

int otherValue()
{
	int result = 0;
	rppw::Single<int>::just(21)
		.map(
			[](int value)
			{
				return value * 2;
			})
		.subscribe(
			[&result](int value)
			{
				result = value;
			},
			[&result](std::exception_ptr)
			{
				result = -1;
			});
	return result;
}
