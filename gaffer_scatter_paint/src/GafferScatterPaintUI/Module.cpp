#include "boost/python.hpp"

#include "GafferScatterPaintUI/PaintPointsTool.h"

#include "GafferBindings/NodeBinding.h"

#include "GafferSceneUI/SceneView.h"

using namespace boost::python;
using namespace GafferScatterPaintUI;

BOOST_PYTHON_MODULE( _GafferScatterPaintUI )
{
	GafferBindings::NodeClass<PaintPointsTool>( nullptr, no_init )
		.def( init<GafferSceneUI::SceneView *>() )
		.def( "commitDemoStroke", &PaintPointsTool::commitDemoStroke )
		.def( "eraseLastStroke", &PaintPointsTool::eraseLastStroke )
	;
}
