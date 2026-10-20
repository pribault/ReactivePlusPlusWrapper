// tests
#include <ConsumerApi.h>

// stl
#include <optional>

namespace rppw::test::consumer
{
	Single<int> makeSingle()
	{
		return Maybe<int>::fromCallable(
				   []
				   {
					   return std::optional{7};
				   })
			.toSingle();
	}
} // namespace rppw::test::consumer
