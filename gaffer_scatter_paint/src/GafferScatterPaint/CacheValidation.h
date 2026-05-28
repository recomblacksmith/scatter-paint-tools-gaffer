#pragma once

#include "boost/python.hpp"

#include <string>

namespace GafferScatterPaint
{

class PaintedPoints;

boost::python::dict validateStore( const PaintedPoints *node, const boost::python::dict &store, const std::string &loadError = std::string() );

} // namespace GafferScatterPaint
