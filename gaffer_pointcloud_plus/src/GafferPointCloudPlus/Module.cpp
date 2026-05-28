#include "boost/python.hpp"

#include "GafferPointCloudPlus/PointCloudPlus.h"

#include "GafferBindings/DependencyNodeBinding.h"

using namespace boost::python;
using namespace GafferPointCloudPlus;

BOOST_PYTHON_MODULE( _GafferPointCloudPlus )
{
    GafferBindings::DependencyNodeClass<PointCloudPlus>();
}
