// stl
#include <utility>

// rppw
#include <rppw/CompositeDisposable.h>
#include <rppw/Disposable.h>

namespace rppw
{
	Disposable::Disposable() = default;

	Disposable Disposable::create()
	{
		return Disposable{rpp::composite_disposable_wrapper::make()};
	}

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

	CompositeDisposable::CompositeDisposable()
		// Native wrappers cancel on final destruction; direct ownership only releases children.
		: disposable_{std::make_shared<rpp::composite_disposable>()}
	{
	}

	void CompositeDisposable::add(const Disposable &disposable) const
	{
		if (disposable_)
			disposable_->add(disposable.disposable_);
		else
			disposable.dispose();
	}

	void CompositeDisposable::remove(const Disposable &disposable) const
	{
		if (disposable_)
			disposable_->remove(disposable.disposable_);
	}

	void CompositeDisposable::dispose() const noexcept
	{
		if (disposable_)
			disposable_->dispose();
	}

	bool CompositeDisposable::isDisposed() const noexcept
	{
		return !disposable_ || disposable_->is_disposed();
	}
} // namespace rppw
