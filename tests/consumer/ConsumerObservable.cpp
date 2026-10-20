// tests
#include <ConsumerApi.h>

// stl
#include <array>

namespace rppw::test::consumer
{
	Observable<int> makeObservable()
	{
		return Observable<int>::fromIterable(std::array{1, 2, 3})
			.map(
				[](int value)
				{
					return value * 2;
				});
	}

	Completable makeCompletable()
	{
		return Completable::complete().andThen(Completable::fromAction([] {}));
	}
} // namespace rppw::test::consumer
