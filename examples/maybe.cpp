// stl
#include <exception>
#include <iostream>
#include <optional>
#include <string>

// rppw
#include <rppw/Maybe.h>

int main()
{
	bool	   failed = false;
	const auto onError = [&failed](std::exception_ptr)
	{
		failed = true;
		std::cerr << "Lookup failed\n";
	};
	rppw::Maybe<std::string>::fromCallable(
		[]
		{
			return std::optional<std::string>{"found"};
		})
		.subscribe(
			[](const std::string &value)
			{
				std::cout << value << '\n';
			},
			onError,
			[]
			{
				std::cout << "empty\n";
			});
	rppw::Maybe<std::string>::fromCallable(
		[]
		{
			return std::optional<std::string>{};
		})
		.defaultIfEmpty("fallback")
		.subscribe(
			[](const std::string &value)
			{
				std::cout << value << '\n';
			},
			onError);
	return failed ? 1 : 0;
}
