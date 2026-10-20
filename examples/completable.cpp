// stl
#include <exception>
#include <iostream>

// rppw
#include <rppw/Completable.h>
#include <rppw/Single.h>

int main()
{
	bool failed = false;
	rppw::Completable::fromAction(
		[]
		{
			std::cout << "prepare\n";
		})
		.andThen(rppw::Single<int>::fromCallable(
			[]
			{
				return 42;
			}))
		.subscribe(
			[](int value)
			{
				std::cout << value << '\n';
			},
			[&failed](std::exception_ptr)
			{
				failed = true;
				std::cerr << "Preparation failed\n";
			});
	return failed ? 1 : 0;
}
