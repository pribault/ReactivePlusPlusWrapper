// stl
#include <utility>

// rppw
#include <rppw/Completable.h>

namespace rppw
{
	Completable::Completable(Observable<std::monostate> source)
		: source_{std::move(source.source_)}
	{
	}

	Completable Completable::complete()
	{
		return Completable{Observable<std::monostate>::empty()};
	}

	Completable Completable::error(std::exception_ptr error)
	{
		return Completable{Observable<std::monostate>::error(std::move(error))};
	}
} // namespace rppw
