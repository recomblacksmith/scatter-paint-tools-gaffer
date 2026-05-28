#pragma once

#include "IECore/Export.h"

#ifdef GafferScatterPaint_EXPORTS
	#define GAFFERSCATTERPAINT_API IECORE_EXPORT
#else
	#define GAFFERSCATTERPAINT_API IECORE_IMPORT
#endif
