#pragma once

#include "IECore/Export.h"

#ifdef GafferScatterPlus_EXPORTS
#define GAFFERSCATTERPLUS_API IECORE_EXPORT
#else
#define GAFFERSCATTERPLUS_API IECORE_IMPORT
#endif
