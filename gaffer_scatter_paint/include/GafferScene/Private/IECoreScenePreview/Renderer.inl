//////////////////////////////////////////////////////////////////////////
//
//  Compatibility shim for Gaffer 1.7 pre-release packages that ship
//  Renderer.h with this private inline include missing from the runtime.
//  Remove this when the packaged Gaffer runtime includes the file.
//
//////////////////////////////////////////////////////////////////////////

namespace IECoreScenePreview
{

template<typename T, typename S>
Renderer::Samples<T> Renderer::staticSamplesCast( const Renderer::Samples<S> &samples )
{
	Renderer::Samples<T> result;
	result.reserve( samples.size() );
	for( const auto &s : samples )
	{
		if constexpr( std::is_pointer_v<S> )
		{
			result.push_back( static_cast<T>( s ) );
		}
		else
		{
			if constexpr( std::is_pointer_v<T> )
			{
				result.push_back( static_cast<T>( s.get() ) );
			}
			else
			{
				result.push_back( boost::static_pointer_cast<typename T::element_type>( s ) );
			}
		}
	}
	return result;
}

} // namespace IECoreScenePreview
