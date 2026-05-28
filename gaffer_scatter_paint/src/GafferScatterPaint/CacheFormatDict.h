#pragma once

#include "GafferScatterPaint/CacheFormat.h"

#include "boost/python.hpp"

namespace GafferScatterPaint
{

boost::python::dict cacheSchemaToDict( const CacheSchema &schema );
CacheSchema dictToCacheSchema( const boost::python::dict &dict );

} // namespace GafferScatterPaint
