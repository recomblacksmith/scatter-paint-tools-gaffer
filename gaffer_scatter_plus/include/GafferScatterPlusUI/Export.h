#pragma once

#include "IECore/Export.h"

#ifdef GafferScatterPlusUI_EXPORTS
#define GAFFERSCATTERPLUSUI_API IECORE_EXPORT
#else
#define GAFFERSCATTERPLUSUI_API IECORE_IMPORT
#endif
