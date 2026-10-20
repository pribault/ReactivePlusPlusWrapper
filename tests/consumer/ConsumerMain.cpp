// tests
#include <ConsumerApi.h>

namespace
{
	bool consumePublicApi()
	{
		using namespace rppw::test::consumer;
		int		   sum = 0;
		int		   single = 0;
		int		   completions = 0;
		int		   errors = 0;
		const auto onError = [&](std::exception_ptr)
		{
			++errors;
		};
		makeObservable().subscribe(
			[&](int value)
			{
				sum += value;
			},
			onError,
			[&]
			{
				++completions;
			});
		makeSingle().subscribe(
			[&](int value)
			{
				single = value;
			},
			onError);
		makeCompletable().subscribe(
			[&]
			{
				++completions;
			},
			onError);
		return sum == 12 && single == 7 && completions == 2 && errors == 0;
	}
} // namespace

int main()
{
	return consumePublicApi() ? 0 : 1;
}
