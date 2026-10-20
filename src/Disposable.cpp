// stl
#include <utility>

// rppw
#include <rppw/Disposable.h>

namespace rppw
{
	Disposable::Disposable() = default;

	Disposable::Disposable(rpp::composite_disposable_wrapper disposable)
		: disposable_{std::move(disposable)}
	{
	}

	void Disposable::dispose() const noexcept
	{
		disposable_.dispose();
	}

	bool Disposable::isDisposed() const noexcept
	{
		return disposable_.is_disposed();
	}
} // namespace rppw
