// stl
#include <exception>
#include <iostream>
#include <vector>

// rppw
#include <rppw/Observable.h>

int main()
{
	bool failed = false;
	rppw::Observable<int>::fromIterable(std::vector{1, 2, 3, 4})
		.filter(
			[](int value)
			{
				return value % 2 == 0;
			})
		.map(
			[](int value)
			{
				return value * 10;
			})
		.subscribe(
			[](int value)
			{
				std::cout << value << '\n';
			},
			[&failed](std::exception_ptr)
			{
				failed = true;
				std::cerr << "Observable failed\n";
			},
			[]
			{
				std::cout << "complete\n";
			});
	return failed ? 1 : 0;
}
