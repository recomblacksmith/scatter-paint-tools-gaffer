#pragma once

#include "PaintedPointsPythonUtils.h"

#include <set>
#include <string>
#include <vector>

inline bp::dict emptyDiagnostics()
{
	bp::dict result;
	result["invalidPointCount"] = 0;
	result["invalidStrokeCount"] = 0;
	result["topologyMismatchCount"] = 0;
	result["failingFrame"] = 0;
	result["failingTargetPaths"] = bp::list();
	result["lastErrorMessage"] = "";
	result["validationSummary"] = "Empty scatter paint cache";
	bp::list categories;
	for( int i = 0; i <= 6; ++i )
	{
		categories.append( i );
	}
	result["categories"] = categories;
	return result;
}

inline std::string validationCategoryName( int category )
{
	switch( category )
	{
		case 0 : return "cache";
		case 1 : return "lock";
		case 2 : return "topology";
		case 3 : return "attachment";
		case 4 : return "exportReadiness";
		case 5 : return "upgradeState";
		case 6 : return "diagnostics";
		default : return "unknown:" + std::to_string( category );
	}
}

inline std::vector<std::string> validationCategoryStrings( const bp::object &value )
{
	std::vector<std::string> result;
	std::set<std::string> seen;
	if( isNone( value ) )
	{
		result.push_back( validationCategoryName( 6 ) );
		return result;
	}

	for( bp::stl_input_iterator<bp::object> it( value ), end; it != end; ++it )
	{
		const std::string name = validationCategoryName( extractOr<int>( *it, 6 ) );
		if( seen.insert( name ).second )
		{
			result.push_back( name );
		}
	}

	if( result.empty() )
	{
		result.push_back( validationCategoryName( 6 ) );
	}

	return result;
}
