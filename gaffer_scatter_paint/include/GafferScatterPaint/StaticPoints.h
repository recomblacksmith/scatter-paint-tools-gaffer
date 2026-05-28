#pragma once

#include "GafferScatterPaint/Export.h"
#include "GafferScatterPaint/TypeIds.h"

#include "Gaffer/StringPlug.h"
#include "Gaffer/Signals.h"
#include "Gaffer/TypedObjectPlug.h"
#include "GafferScene/ObjectSource.h"

#include "boost/python/object.hpp"

namespace GafferScatterPaint
{

class GAFFERSCATTERPAINT_API StaticPoints : public GafferScene::ObjectSource
{

	public :

		GAFFER_NODE_DECLARE_TYPE( GafferScatterPaint::StaticPoints, StaticPointsTypeId, GafferScene::ObjectSource );

		explicit StaticPoints( const std::string &name = defaultName<StaticPoints>() );
		~StaticPoints() override;

		Gaffer::ObjectPlug *pointDataPlug();
		const Gaffer::ObjectPlug *pointDataPlug() const;

		Gaffer::StringPlug *outputLocationPlug();
		const Gaffer::StringPlug *outputLocationPlug() const;

		Gaffer::StringPlug *pointTypePlug();
		const Gaffer::StringPlug *pointTypePlug() const;

		Gaffer::BoolPlug *debugColorPlug();
		const Gaffer::BoolPlug *debugColorPlug() const;

		void setPointRecords( const boost::python::object &records );
		void setFramePointRecords( const boost::python::object &frameRecords );
		void setFramePointData( const boost::python::object &frameData );

		void affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const override;

	protected :

		void hashSource( const Gaffer::Context *context, IECore::MurmurHash &h ) const override;
		IECore::ConstObjectPtr computeSource( const Gaffer::Context *context ) const override;

	private :

		void syncOutputLocation();
		void plugSet( Gaffer::Plug *plug );

		static size_t g_firstPlugIndex;
		Gaffer::Signals::ScopedConnection m_plugSetConnection;

};

IE_CORE_DECLAREPTR( StaticPoints )

} // namespace GafferScatterPaint
