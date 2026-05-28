#pragma once

#include "IECore/Export.h"

#ifdef GafferPointCloudPlus_EXPORTS
	#define GAFFERPOINTCLOUDPLUS_API IECORE_EXPORT
#else
	#define GAFFERPOINTCLOUDPLUS_API IECORE_IMPORT
#endif
