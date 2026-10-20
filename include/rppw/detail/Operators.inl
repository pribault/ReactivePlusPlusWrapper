#pragma once

// stl
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

// rpp
#include <rpp/rpp.hpp>

// rppw
#include <rppw/detail/Fwd.h>

namespace rppw::detail
{
	template <Value T, typename F>
	rpp::dynamic_observable<T> deferred(F factory)
	{
		return rpp::source::create<T>(
				   [factory = std::move(factory)](rpp::dynamic_observer<T> observer)
				   {
					   try
					   {
						   std::invoke(factory).subscribe(observer);
					   }
					   catch (...)
					   {
						   observer.on_error(std::current_exception());
					   }
				   })
			.as_dynamic();
	}

	template <Value T>
	rpp::dynamic_observable<T> cardinality(rpp::dynamic_observable<T> source, bool required, const char *operation)
	{
		return (source |
				rpp::operators::reduce(std::optional<T>{},
									   [operation](std::optional<T> value, const T &next)
									   {
										   if (value)
											   throw std::out_of_range{std::string{operation} + ": expected at most one value"};
										   return std::optional<T>{next};
									   }) |
				rpp::operators::flat_map(
					[required, operation](const std::optional<T> &value) -> rpp::dynamic_observable<T>
					{
						if (value)
							return rpp::source::just(*value).as_dynamic();
						if (required)
							return rpp::source::error<T>(
									   std::make_exception_ptr(std::out_of_range{std::string{operation} + ": expected one value, received zero"}))
								.as_dynamic();
						return rpp::source::empty<T>().as_dynamic();
					}))
			.as_dynamic();
	}
} // namespace rppw::detail
