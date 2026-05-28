#pragma once

#include "IECore/Export.h"

#ifdef GafferPointCloudPlusUI_EXPORTS
	#define GAFFERPOINTCLOUDPLUSUI_API IECORE_EXPORT
#else
	#define GAFFERPOINTCLOUDPLUSUI_API IECORE_IMPORT
#endif
