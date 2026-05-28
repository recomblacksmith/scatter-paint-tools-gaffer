#include "boost/python.hpp"

#include "GafferScatterPlus/ScatterPlus.h"

#include "GafferBindings/DependencyNodeBinding.h"

using namespace boost::python;
using namespace GafferScatterPlus;

BOOST_PYTHON_MODULE( _GafferScatterPlus )
{
    GafferBindings::DependencyNodeClass<ScatterPlus>();
}
