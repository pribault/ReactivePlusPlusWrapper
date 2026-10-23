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

	Disposable Completable::subscribe() const
	{
		return Observable<std::monostate>{source_}.subscribe();
	}

	Completable Completable::repeat(std::size_t count) const
	{
		return Completable{Observable<std::monostate>{source_}.repeat(count)};
	}

	Completable Completable::repeat() const
	{
		return Completable{Observable<std::monostate>{source_}.repeat()};
	}

	Completable Completable::retry(std::size_t count) const
	{
		return Completable{Observable<std::monostate>{source_}.retry(count)};
	}

	Completable Completable::retry() const
	{
		return Completable{Observable<std::monostate>{source_}.retry()};
	}
} // namespace rppw
