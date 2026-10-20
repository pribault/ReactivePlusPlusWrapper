#pragma once

// googletest
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// rppw
#include <rppw/ReactivePlusPlusWrapper.h>

// stl
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace rppw::test
{
	class SignalSink
	{
	public:
		virtual ~SignalSink() = default;
		virtual void next(int value) = 0;
		virtual void error(std::exception_ptr error) = 0;
		virtual void complete() = 0;
	};

	class MockSignalSink : public SignalSink
	{
	public:
		MOCK_METHOD(void, next, (int), (override));
		MOCK_METHOD(void, error, (std::exception_ptr), (override));
		MOCK_METHOD(void, complete, (), (override));
	};

	template <typename Exception>
	auto ExceptionContaining(std::string text)
	{
		return ::testing::Truly(
			[text = std::move(text)](const std::exception_ptr &error)
			{
				if (!error)
					return false;
				try
				{
					std::rethrow_exception(error);
				}
				catch (const Exception &exception)
				{
					return std::string_view{exception.what()}.find(text) != std::string_view::npos;
				}
				catch (...)
				{
					return false;
				}
			});
	}

	class ReactiveTest : public ::testing::Test
	{
	protected:
		Disposable subscribe(const Observable<int> &source)
		{
			return source.subscribe(
				[this](int value)
				{
					observer.next(value);
				},
				[this](std::exception_ptr error)
				{
					observer.error(error);
				},
				[this]
				{
					observer.complete();
				});
		}

		Disposable subscribe(const Single<int> &source)
		{
			return source.subscribe(
				[this](int value)
				{
					observer.next(value);
				},
				[this](std::exception_ptr error)
				{
					observer.error(error);
				});
		}

		Disposable subscribe(const Maybe<int> &source)
		{
			return source.subscribe(
				[this](int value)
				{
					observer.next(value);
				},
				[this](std::exception_ptr error)
				{
					observer.error(error);
				},
				[this]
				{
					observer.complete();
				});
		}

		Disposable subscribe(const Completable &source)
		{
			return source.subscribe(
				[this]
				{
					observer.complete();
				},
				[this](std::exception_ptr error)
				{
					observer.error(error);
				});
		}

		::testing::StrictMock<MockSignalSink> observer;
		const std::exception_ptr			  failure = std::make_exception_ptr(std::runtime_error{"producer failed"});
	};
} // namespace rppw::test
