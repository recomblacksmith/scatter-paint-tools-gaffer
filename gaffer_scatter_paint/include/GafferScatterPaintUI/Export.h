#pragma once

#include "IECore/Export.h"

#ifdef GafferScatterPaintUI_EXPORTS
	#define GAFFERSCATTERPAINTUI_API IECORE_EXPORT
#else
	#define GAFFERSCATTERPAINTUI_API IECORE_IMPORT
#endif
