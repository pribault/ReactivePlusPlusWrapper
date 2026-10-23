#pragma once

// stl
#include <cstdio>
#include <exception>

namespace rppw::detail
{
	inline void logUnhandledError(const std::exception_ptr &error) noexcept
	{
		if (!error)
		{
			std::fputs("rppw::subscribe(): unhandled error: empty exception_ptr\n", stderr);
			return;
		}
		try
		{
			std::rethrow_exception(error);
		}
		catch (const std::exception &exception)
		{
			std::fprintf(stderr, "rppw::subscribe(): unhandled error: %s\n", exception.what());
		}
		catch (...)
		{
			std::fputs("rppw::subscribe(): unhandled error: non-standard exception\n", stderr);
		}
	}
} // namespace rppw::detail
