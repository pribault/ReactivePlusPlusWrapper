// stl
#include <exception>

// rppw
#include <rppw/ReactivePlusPlusWrapper.h>

int otherValue();

int main()
{
	int result = 0;
	rppw::Completable::complete()
		.andThen(rppw::Maybe<int>::just(otherValue()).toSingle())
		.subscribe(
			[&result](int value)
			{
				result = value;
			},
			[&result](std::exception_ptr)
			{
				result = -1;
			});
	return result == 42 ? 0 : 1;
}
