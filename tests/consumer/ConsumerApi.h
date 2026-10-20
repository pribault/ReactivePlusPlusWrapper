#pragma once

// rppw
#include <rppw/ReactivePlusPlusWrapper.h>

namespace rppw::test::consumer
{
	Observable<int> makeObservable();
	Single<int>		makeSingle();
	Completable		makeCompletable();
} // namespace rppw::test::consumer
