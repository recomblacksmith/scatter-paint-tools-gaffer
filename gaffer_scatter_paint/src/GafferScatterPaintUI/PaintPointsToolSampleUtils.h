#pragma once

namespace
{

using SampleClock = std::chrono::steady_clock;

double sampleElapsedMs( const SampleClock::time_point &start )
{
	return std::chrono::duration<double, std::milli>( SampleClock::now() - start ).count();
}

bp::list vectorList( const Imath::V3f &value )
{
	bp::list result;
	result.append( value.x );
	result.append( value.y );
	result.append( value.z );
	return result;
}

bp::list vector2List( const Imath::V2f &value )
{
	bp::list result;
	result.append( value.x );
	result.append( value.y );
	return result;
}

float clampedUnitFloat( float value )
{
	return std::max( 0.0f, std::min( 1.0f, value ) );
}

float hashedUnitFloat( std::uint32_t value )
{
	value += 0x9e3779b9u;
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	value ^= value >> 16;
	return static_cast<float>( value ) / 4294967295.0f;
}

Imath::V2f vector2FromObject( const bp::object &value, const Imath::V2f &fallback = Imath::V2f( 0.0f ) )
{
	if( value.is_none() )
	{
		return fallback;
	}

	std::vector<float> values;
	for( bp::stl_input_iterator<bp::object> it( value ), end; it != end; ++it )
	{
		bp::extract<float> extracted( *it );
		values.push_back( extracted.check() ? extracted() : 0.0f );
	}
	if( values.size() != 2 )
	{
		return fallback;
	}
	return Imath::V2f( values[0], values[1] );
}

float hashedSignedFloat( std::uint32_t value )
{
	return ( hashedUnitFloat( value ) * 2.0f ) - 1.0f;
}

float resolvedWidthJitter( const PaintPointsTool *tool )
{
	return clampedUnitFloat( tool->widthJitterPlug()->getValue() );
}

float resolvedScaleJitter( const PaintPointsTool *tool )
{
	return clampedUnitFloat( tool->scaleJitterPlug()->getValue() );
}

int resolvedRotationMode( const PaintPointsTool *tool )
{
	return std::max( 0, std::min( 2, tool->rotationModePlug()->getValue() ) );
}

BrushSampleData applyBrushVariation(
	const PaintPointsTool *tool,
	const BrushSampleData &sample,
	std::uint32_t variationSeed,
	int pointIndex,
	int dabIndex,
	bool expanded
)
{
	BrushSampleData result = sample;
	const float widthJitter = resolvedWidthJitter( tool );
	const float scaleJitter = resolvedScaleJitter( tool );
	if( widthJitter > 0.0f )
	{
		const float widthFactor = std::max( 0.01f, 1.0f + ( hashedSignedFloat( variationSeed ^ 0x68bc21ebu ) * widthJitter ) );
		result.width *= widthFactor;
	}
	if( scaleJitter > 0.0f )
	{
		const float scaleFactor = std::max( 0.01f, 1.0f + ( hashedSignedFloat( variationSeed ^ 0x02e5be93u ) * scaleJitter ) );
		result.scale *= scaleFactor;
	}

	result.normalSpin = 0.0f;
	result.tangentRotation = Imath::V2f( 0.0f );
	const int rotationMode = resolvedRotationMode( tool );
	if( rotationMode == 1 )
	{
		const float step = static_cast<float>( ( pointIndex + std::max( 0, dabIndex ) ) % 8 ) / 8.0f;
		result.normalSpin = 2.0f * M_PI * step;
	}
	else if( rotationMode == 2 )
	{
		result.normalSpin = 2.0f * M_PI * hashedUnitFloat( variationSeed ^ 0x9e3779b9u );
	}

	if( expanded )
	{
		result.sampleExpanded = true;
	}
	return result;
}

bp::dict pythonBrushSampleFromData( const BrushSampleData &sample )
{
	bp::dict result;
	result["point"] = vectorList( sample.point );
	result["tangentRotation"] = vector2List( sample.tangentRotation );
	result["width"] = sample.width;
	result["scale"] = sample.scale;
	result["normalSpin"] = sample.normalSpin;
	result["pressureDensity"] = sample.pressureDensity;
	result["pressureSoftness"] = sample.pressureSoftness;
	result["seed"] = sample.seed;
	result["sourcePath"] = sample.sourcePath;
	result["instanceId"] = sample.instanceId;
	result["instanceSourcePath"] = sample.instanceSourcePath;
	result["triangleIndex"] = sample.triangleIndex;
	result["barycentric"] = vectorList( sample.barycentric );
	result["attachmentResolved"] = sample.attachmentResolved;
	result["sampleOrigin"] = sample.sampleOrigin;
	result["sampleExpanded"] = sample.sampleExpanded;
	result["sampleSourceIndex"] = sample.sampleSourceIndex;
	result["sampleExpansionIndex"] = sample.sampleExpansionIndex;
	result["N"] = vectorList( sample.normal );
	result["valid"] = sample.valid;
	return result;
}

BrushSampleData brushSampleDataFromDict( const bp::dict &sample )
{
	BrushSampleData result;
	result.point = vectorFromObject( sample.get( "point", bp::object() ), Imath::V3f( 0.0f ) );
	result.normal = vectorFromObject( sample.get( "N", bp::object() ), Imath::V3f( 0.0f, 1.0f, 0.0f ) );
	result.barycentric = vectorFromObject( sample.get( "barycentric", bp::object() ), Imath::V3f( 1.0f, 0.0f, 0.0f ) );
	result.tangentRotation = vector2FromObject( sample.get( "tangentRotation", bp::object() ), Imath::V2f( 0.0f ) );
	result.width = bp::extract<float>( sample.get( "width", bp::object( 0.0f ) ) );
	result.scale = bp::extract<float>( sample.get( "scale", bp::object( 1.0f ) ) );
	result.normalSpin = bp::extract<float>( sample.get( "normalSpin", bp::object( 0.0f ) ) );
	result.pressureDensity = bp::extract<float>( sample.get( "pressureDensity", bp::object( 0.0f ) ) );
	result.pressureSoftness = bp::extract<float>( sample.get( "pressureSoftness", bp::object( 0.0f ) ) );
	result.seed = uint64FromObject( sample.get( "seed", bp::object() ), 0 );
	result.sourcePath = bp::extract<std::string>( sample.get( "sourcePath", bp::object( "" ) ) );
	result.instanceId = static_cast<std::uint32_t>( uint64FromObject( sample.get( "instanceId", bp::object() ), 0 ) );
	result.instanceSourcePath = bp::extract<std::string>( sample.get( "instanceSourcePath", bp::object( "" ) ) );
	result.sampleOrigin = bp::extract<std::string>( sample.get( "sampleOrigin", bp::object( "rawHit" ) ) );
	result.triangleIndex = bp::extract<int>( sample.get( "triangleIndex", bp::object( 0 ) ) );
	result.sampleSourceIndex = bp::extract<int>( sample.get( "sampleSourceIndex", bp::object( 0 ) ) );
	result.sampleExpansionIndex = bp::extract<int>( sample.get( "sampleExpansionIndex", bp::object( 0 ) ) );
	result.attachmentResolved = bp::extract<bool>( sample.get( "attachmentResolved", bp::object( false ) ) );
	result.sampleExpanded = bp::extract<bool>( sample.get( "sampleExpanded", bp::object( false ) ) );
	result.valid = bp::extract<bool>( sample.get( "valid", bp::object( true ) ) );
	return result;
}

} // namespace

bp::list demoPoints( const PaintPointsTool *tool )
{
	const int count = std::max( 1, tool->previewCountPlug()->getValue() );
	const float radius = std::max( 0.001f, tool->brushSizePlug()->getValue() );
	const float density = std::max( 0.0f, pressureScaledDensity( tool ) );
	const float softness = std::max( 0.0f, pressureScaledSoftness( tool ) );
	const std::array<int, 2> triangleIndices = { 0, 1 };
	const std::array<Imath::V3f, 2> barycentrics = {
		Imath::V3f( 0.5f, 0.25f, 0.25f ),
		Imath::V3f( 0.25f, 0.5f, 0.25f )
	};

	bp::list result;
	for( int index = 0; index < count; ++index )
	{
		const size_t attachmentIndex = static_cast<size_t>( index ) % triangleIndices.size();
		const float angle = static_cast<float>( static_cast<double>( index ) / static_cast<double>( count ) * ( 2.0 * M_PI ) );
		const float x = radius * std::cos( angle );
		const float z = radius * std::sin( angle );

		BrushSampleData sample;
		sample.point = Imath::V3f( x, 0.0f, z );
		sample.normal = Imath::V3f( 0.0f, 1.0f, 0.0f );
		sample.barycentric = barycentrics[attachmentIndex];
		sample.width = radius;
		sample.scale = std::max( 0.1f, density );
		sample.pressureDensity = density;
		sample.pressureSoftness = softness;
		sample.seed = static_cast<std::uint64_t>( index );
		sample.sourcePath = "/paintPlane";
		sample.triangleIndex = triangleIndices[attachmentIndex];
		sample.attachmentResolved = true;
		sample.valid = true;
		sample = applyBrushVariation( tool, sample, static_cast<std::uint32_t>( sample.seed ), index, 0, false );

		bp::dict point;
		bp::list position;
		position.append( sample.point.x );
		position.append( 0.0f );
		position.append( sample.point.z );
		point["P"] = position;
		point["width"] = sample.width;
		point["scale"] = sample.scale;
		point["normalSpin"] = sample.normalSpin;
		point["tangentRotation"] = vector2List( sample.tangentRotation );
		point["pressureDensity"] = sample.pressureDensity;
		point["pressureSoftness"] = sample.pressureSoftness;
		point["seed"] = static_cast<int>( sample.seed );
		point["sourcePath"] = sample.sourcePath;
		point["triangleIndex"] = sample.triangleIndex;
		bp::list barycentric;
		barycentric.append( sample.barycentric.x );
		barycentric.append( sample.barycentric.y );
		barycentric.append( sample.barycentric.z );
		point["barycentric"] = barycentric;
		point["attachmentResolved"] = sample.attachmentResolved;
		point["valid"] = sample.valid;
		result.append( point );
	}

	return result;
}

std::pair<Imath::V3f, Imath::V3f> brushSampleFrame( const Imath::V3f &normal )
{
	const Imath::V3f safeNormal = normal.length2() > 0.0f ? normal.normalized() : Imath::V3f( 0.0f, 1.0f, 0.0f );
	Imath::V3f tangent = safeNormal.cross( Imath::V3f( 0.0f, 1.0f, 0.0f ) );
	if( tangent.length2() < 1e-8f )
	{
		tangent = safeNormal.cross( Imath::V3f( 1.0f, 0.0f, 0.0f ) );
	}
	tangent.normalize();
	Imath::V3f bitangent = safeNormal.cross( tangent );
	bitangent.normalize();
	return { tangent, bitangent };
}

const char *sampleOriginName( PaintPointsTool::SampleOrigin sampleOrigin )
{
	switch( sampleOrigin )
	{
		case PaintPointsTool::SampleOrigin::DensifiedHit :
			return "densifiedHit";
		case PaintPointsTool::SampleOrigin::RawHit :
		default :
			return "rawHit";
	}
}

bp::dict strokePointFromHit( const PaintPointsTool *tool, const PaintPointsTool::HitRecord &hit, int seed )
{
	const float radius = std::max( 0.001f, tool->brushSizePlug()->getValue() );
	const float density = std::max( 0.0f, pressureScaledDensity( tool ) );
	const float softness = std::max( 0.0f, pressureScaledSoftness( tool ) );
	const Imath::V3f liftNormal = hit.normal.length2() > 0.0f ? hit.normal.normalized() : Imath::V3f( 0.0f, 1.0f, 0.0f );
	const float displayLift = std::max( radius * 0.1f, 0.01f );
	const Imath::V3f liftedPoint = hit.point + ( liftNormal * displayLift );

	bp::dict point;
	bp::list position;
	position.append( liftedPoint.x );
	position.append( liftedPoint.y );
	position.append( liftedPoint.z );
	point["P"] = position;
	point["width"] = radius;
	point["scale"] = std::max( 0.1f, density );
	point["pressureDensity"] = density;
	point["pressureSoftness"] = softness;
	point["seed"] = seed;
	point["sourcePath"] = hit.path;
	point["triangleIndex"] = hit.triangleIndex;
	bp::list barycentric;
	barycentric.append( hit.barycentric.x );
	barycentric.append( hit.barycentric.y );
	barycentric.append( hit.barycentric.z );
	point["barycentric"] = barycentric;
	point["attachmentResolved"] = hit.attachmentResolved;
	bp::list normal;
	normal.append( hit.normal.x );
	normal.append( hit.normal.y );
	normal.append( hit.normal.z );
	point["N"] = normal;
	point["valid"] = true;
	return point;
}

bp::dict brushSampleFromHit( const PaintPointsTool *tool, const PaintPointsTool::HitRecord &hit, int seed )
{
	return pythonBrushSampleFromData( brushSampleDataFromHit( tool, hit, seed ) );
}

BrushSampleData brushSampleDataFromHit( const PaintPointsTool *tool, const PaintPointsTool::HitRecord &hit, int seed )
{
	const float radius = std::max( 0.001f, tool->brushSizePlug()->getValue() );
	const float density = std::max( 0.0f, pressureScaledDensity( tool ) );
	const float softness = std::max( 0.0f, pressureScaledSoftness( tool ) );

	BrushSampleData sample;
	sample.point = hit.point;
	sample.normal = hit.normal;
	sample.barycentric = hit.barycentric;
	sample.width = radius;
	sample.scale = std::max( 0.1f, density );
	sample.pressureDensity = density;
	sample.pressureSoftness = softness;
	sample.seed = static_cast<std::uint64_t>( seed );
	sample.sourcePath = hit.path;
	sample.sampleOrigin = sampleOriginName( hit.sampleOrigin );
	sample.triangleIndex = hit.triangleIndex;
	sample.sampleSourceIndex = seed;
	sample.sampleExpansionIndex = 0;
	sample.attachmentResolved = hit.attachmentResolved;
	sample.sampleExpanded = false;
	sample.valid = true;
	return applyBrushVariation( tool, sample, static_cast<std::uint32_t>( sample.seed ), seed, 0, false );
}

bp::list brushSamplesFromStroke( const PaintPointsTool *tool, const PaintPointsTool::StrokePoints &strokePoints )
{
	return brushSamplesToPythonList( brushSampleDataFromStroke( tool, strokePoints ) );
}

BrushSampleDataList brushSampleDataFromStroke(
	const PaintPointsTool *tool,
	const PaintPointsTool::StrokePoints &strokePoints,
	double *buildMs
)
{
	const auto start = SampleClock::now();
	BrushSampleDataList result;
	result.reserve( strokePoints.size() );
	for( size_t i = 0; i < strokePoints.size(); ++i )
	{
		result.emplace_back( brushSampleDataFromHit( tool, strokePoints[i], static_cast<int>( i ) ) );
	}
	if( buildMs )
	{
		*buildMs = sampleElapsedMs( start );
	}
	return result;
}

BrushSampleDataList expandBrushSampleData(
	const PaintPointsTool *tool,
	const BrushSampleDataList &samples,
	double *jitterMs
)
{
	const int countPerDab = effectivePointsPerDab( tool );
	if( countPerDab <= 1 )
	{
		if( jitterMs )
		{
			*jitterMs = 0.0;
		}
		return samples;
	}

	double localJitterMs = 0.0;
	BrushSampleDataList expanded;
	expanded.reserve( samples.size() * static_cast<size_t>( countPerDab ) );
	for( const BrushSampleData &sample : samples )
	{
		const auto frame = brushSampleFrame( sample.normal );
		const float radius = std::max( 0.0f, sample.width );
		const std::uint32_t baseSeed = static_cast<std::uint32_t>( sample.seed );
		for( int i = 0; i < countPerDab; ++i )
		{
			const auto jitterStart = SampleClock::now();
			const std::uint32_t jitterSeed = baseSeed ^ ( 0x9e3779b9u * static_cast<std::uint32_t>( i + 1 ) );
			const float radial = radius * std::sqrt( hashedUnitFloat( jitterSeed ) );
			const float angle = 2.0f * M_PI * hashedUnitFloat( jitterSeed ^ 0x85ebca6bu );
			const Imath::V3f offset = frame.first * ( std::cos( angle ) * radial ) + frame.second * ( std::sin( angle ) * radial );
			BrushSampleData expandedSample = sample;
			expandedSample.point = sample.point + offset;
			expandedSample.seed = static_cast<std::uint64_t>( jitterSeed & 0x7fffffffu );
			expandedSample = applyBrushVariation( tool, expandedSample, jitterSeed, sample.sampleSourceIndex, i, true );
			expandedSample.sampleExpansionIndex = i;
			expanded.emplace_back( std::move( expandedSample ) );
			localJitterMs += sampleElapsedMs( jitterStart );
		}
	}

	if( jitterMs )
	{
		*jitterMs = localJitterMs;
	}
	return expanded;
}

bp::list brushSamplesToPythonList( const BrushSampleDataList &samples, double *constructMs )
{
	const auto start = SampleClock::now();
	bp::list result;
	for( const BrushSampleData &sample : samples )
	{
		result.append( pythonBrushSampleFromData( sample ) );
	}
	if( constructMs )
	{
		*constructMs = sampleElapsedMs( start );
	}
	return result;
}

bp::list expandedPaintSamples(
	const PaintPointsTool *tool,
	const bp::list &samples,
	double *baseDecodeMs,
	double *jitterMs,
	double *constructMs
)
{
	const int countPerDab = effectivePointsPerDab( tool );
	const auto baseDecodeStart = SampleClock::now();
	BrushSampleDataList typedSamples;
	typedSamples.reserve( static_cast<size_t>( bp::len( samples ) ) );
	for( bp::stl_input_iterator<bp::object> it( samples ), end; it != end; ++it )
	{
		typedSamples.emplace_back( brushSampleDataFromDict( bp::extract<bp::dict>( *it ) ) );
	}
	const double localBaseDecodeMs = sampleElapsedMs( baseDecodeStart );
	if( countPerDab <= 1 )
	{
		if( baseDecodeMs )
		{
			*baseDecodeMs = localBaseDecodeMs;
		}
		if( jitterMs )
		{
			*jitterMs = 0.0;
		}
		if( constructMs )
		{
			*constructMs = 0.0;
		}
		return brushSamplesToPythonList( typedSamples, nullptr );
	}
	double localJitterMs = 0.0;
	BrushSampleDataList expanded = expandBrushSampleData( tool, typedSamples, &localJitterMs );
	double localConstructMs = 0.0;
	bp::list expandedPython = brushSamplesToPythonList( expanded, &localConstructMs );
	if( baseDecodeMs )
	{
		*baseDecodeMs = localBaseDecodeMs;
	}
	if( jitterMs )
	{
		*jitterMs = localJitterMs;
	}
	if( constructMs )
	{
		*constructMs = localConstructMs;
	}

	return expandedPython;
}

size_t authoredPointCount( const bp::list &points )
{
	return static_cast<size_t>( bp::len( points ) );
}

int resolvedAuthoredPointCount( const bp::list &points )
{
	int resolved = 0;
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		try
		{
			bp::dict point = bp::extract<bp::dict>( *it );
			if( bp::extract<bool>( point.get( "attachmentResolved", false ) ) )
			{
				++resolved;
			}
		}
		catch( const bp::error_already_set & )
		{
			PyErr_Clear();
		}
	}
	return resolved;
}
