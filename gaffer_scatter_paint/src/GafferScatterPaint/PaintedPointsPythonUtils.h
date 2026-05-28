#pragma once

#include <chrono>
#include <cstring>

template<typename T>
T extractOr( const bp::object &value, const T &defaultValue )
{
	bp::extract<T> extractor( value );
	return extractor.check() ? extractor() : defaultValue;
}

inline bp::object dictGet( const bp::dict &dictionary, const char *key )
{
	return dictionary.has_key( key ) ? dictionary[key] : bp::object();
}

inline PyObject *dictItemBorrowed( const bp::dict &dictionary, const char *key )
{
	PyObject *value = PyDict_GetItemString( dictionary.ptr(), key );
	if( value )
	{
		return value;
	}
	PyErr_Clear();
	return nullptr;
}

inline PyObject *dictItemBorrowed( PyObject *dictionary, const char *key )
{
	if( !dictionary || !PyDict_Check( dictionary ) )
	{
		return nullptr;
	}
	PyObject *value = PyDict_GetItemString( dictionary, key );
	if( value )
	{
		return value;
	}
	PyErr_Clear();
	return nullptr;
}

template<typename T>
T dictValue( const bp::dict &dictionary, const char *key, const T &defaultValue )
{
	return dictionary.has_key( key ) ? extractOr<T>( dictionary[key], defaultValue ) : defaultValue;
}

inline std::string pyString( const bp::object &value )
{
	return bp::extract<std::string>( bp::str( value ) );
}

inline float pyFloatFast( PyObject *value, float defaultValue )
{
	if( !value || value == Py_None )
	{
		return defaultValue;
	}
	if( PyFloat_Check( value ) )
	{
		return static_cast<float>( PyFloat_AS_DOUBLE( value ) );
	}
	if( PyLong_Check( value ) )
	{
		return static_cast<float>( PyLong_AsLong( value ) );
	}
	return extractOr<float>( bp::object( bp::handle<>( bp::borrowed( value ) ) ), defaultValue );
}

inline int pyIntFast( PyObject *value, int defaultValue )
{
	if( !value || value == Py_None )
	{
		return defaultValue;
	}
	if( PyLong_Check( value ) )
	{
		return static_cast<int>( PyLong_AsLong( value ) );
	}
	if( PyFloat_Check( value ) )
	{
		return static_cast<int>( PyFloat_AS_DOUBLE( value ) );
	}
	return extractOr<int>( bp::object( bp::handle<>( bp::borrowed( value ) ) ), defaultValue );
}

inline std::uint32_t pyUInt32Fast( PyObject *value, std::uint32_t defaultValue )
{
	if( !value || value == Py_None )
	{
		return defaultValue;
	}
	if( PyLong_Check( value ) )
	{
		return static_cast<std::uint32_t>( PyLong_AsUnsignedLong( value ) );
	}
	if( PyFloat_Check( value ) )
	{
		return static_cast<std::uint32_t>( PyFloat_AS_DOUBLE( value ) );
	}
	return extractOr<std::uint32_t>( bp::object( bp::handle<>( bp::borrowed( value ) ) ), defaultValue );
}

inline bool pyBoolFast( PyObject *value, bool defaultValue )
{
	if( !value || value == Py_None )
	{
		return defaultValue;
	}
	if( PyBool_Check( value ) )
	{
		return value == Py_True;
	}
	const int truth = PyObject_IsTrue( value );
	if( truth < 0 )
	{
		PyErr_Clear();
		return defaultValue;
	}
	return truth != 0;
}

inline std::string pyStringFast( PyObject *value, const std::string &defaultValue )
{
	if( !value || value == Py_None )
	{
		return defaultValue;
	}
	if( PyUnicode_Check( value ) )
	{
		const char *utf8 = PyUnicode_AsUTF8( value );
		if( utf8 )
		{
			return utf8;
		}
		PyErr_Clear();
		return defaultValue;
	}
	if( PyBytes_Check( value ) )
	{
		char *buffer = nullptr;
		Py_ssize_t size = 0;
		if( PyBytes_AsStringAndSize( value, &buffer, &size ) == 0 && buffer )
		{
			return std::string( buffer, static_cast<size_t>( size ) );
		}
		PyErr_Clear();
		return defaultValue;
	}
	return extractOr<std::string>( bp::object( bp::handle<>( bp::borrowed( value ) ) ), defaultValue );
}

inline Imath::V3f pyV3fFast( PyObject *value, const Imath::V3f &defaultValue )
{
	if( !value || value == Py_None )
	{
		return defaultValue;
	}
	if( PyList_CheckExact( value ) )
	{
		if( PyList_GET_SIZE( value ) != 3 )
		{
			return defaultValue;
		}
		return Imath::V3f(
			pyFloatFast( PyList_GET_ITEM( value, 0 ), defaultValue.x ),
			pyFloatFast( PyList_GET_ITEM( value, 1 ), defaultValue.y ),
			pyFloatFast( PyList_GET_ITEM( value, 2 ), defaultValue.z )
		);
	}
	if( PyTuple_CheckExact( value ) )
	{
		if( PyTuple_GET_SIZE( value ) != 3 )
		{
			return defaultValue;
		}
		return Imath::V3f(
			pyFloatFast( PyTuple_GET_ITEM( value, 0 ), defaultValue.x ),
			pyFloatFast( PyTuple_GET_ITEM( value, 1 ), defaultValue.y ),
			pyFloatFast( PyTuple_GET_ITEM( value, 2 ), defaultValue.z )
		);
	}
	PyObject *sequence = PySequence_Fast( value, nullptr );
	if( !sequence )
	{
		PyErr_Clear();
		return defaultValue;
	}
	if( PySequence_Fast_GET_SIZE( sequence ) != 3 )
	{
		Py_DECREF( sequence );
		return defaultValue;
	}
	PyObject **items = PySequence_Fast_ITEMS( sequence );
	const Imath::V3f result(
		pyFloatFast( items[0], defaultValue.x ),
		pyFloatFast( items[1], defaultValue.y ),
		pyFloatFast( items[2], defaultValue.z )
	);
	Py_DECREF( sequence );
	return result;
}

inline bp::object storeBridgeModule()
{
	return bp::import( "GafferScatterPaint._storebridge" );
}

inline bp::object storeBridgeFunction( const char *name )
{
	return storeBridgeModule().attr( name );
}

inline bp::object coreHelpersModule()
{
	return bp::import( "GafferScatterPaint._core" );
}

inline bp::object coreHelperFunction( const char *name )
{
	return coreHelpersModule().attr( name );
}

inline bool isNone( const bp::object &value )
{
	return value.ptr() == Py_None;
}

inline std::string bytesFromPythonBytes( const bp::object &value )
{
	char *buffer = nullptr;
	Py_ssize_t size = 0;
	if( PyBytes_AsStringAndSize( value.ptr(), &buffer, &size ) != 0 )
	{
		bp::throw_error_already_set();
	}
	return std::string( buffer, size );
}

inline bp::object pythonBytes( const std::string &value )
{
	return bp::object( bp::handle<>( PyBytes_FromStringAndSize( value.data(), value.size() ) ) );
}

inline std::string blobBytesFromObject( const IECore::Object *object )
{
	const IECore::UCharVectorData *data = IECore::runTimeCast<const IECore::UCharVectorData>( object );
	if( !data )
	{
		return std::string();
	}

	const auto &readable = data->readable();
	if( readable.empty() )
	{
		return std::string();
	}
	return std::string( reinterpret_cast<const char *>( readable.data() ), readable.size() );
}

inline IECore::UCharVectorDataPtr blobObjectFromBytes( const std::string &value, double *resizeMs = nullptr, double *copyMs = nullptr )
{
	IECore::UCharVectorDataPtr result = new IECore::UCharVectorData();
	auto &writable = result->writable();
	const auto resizeStart = std::chrono::steady_clock::now();
	writable.resize( value.size() );
	if( resizeMs )
	{
		*resizeMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - resizeStart ).count();
	}
	if( !value.empty() )
	{
		const auto copyStart = std::chrono::steady_clock::now();
		std::memcpy( writable.data(), value.data(), value.size() );
		if( copyMs )
		{
			*copyMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - copyStart ).count();
		}
	}
	else if( copyMs )
	{
		*copyMs = 0.0;
	}
	return result;
}

inline std::vector<std::string> stringVector( const bp::object &value )
{
	std::vector<std::string> result;
	if( isNone( value ) )
	{
		return result;
	}
	for( bp::stl_input_iterator<bp::object> it( value ), end; it != end; ++it )
	{
		result.push_back( pyString( *it ) );
	}
	return result;
}

inline std::vector<float> floatVector( const bp::object &value, size_t expectedSize, const std::vector<float> &defaultValue )
{
	std::vector<float> result;
	if( isNone( value ) )
	{
		return defaultValue;
	}
	for( bp::stl_input_iterator<bp::object> it( value ), end; it != end; ++it )
	{
		result.push_back( extractOr<float>( *it, 0.0f ) );
	}
	if( result.size() != expectedSize )
	{
		return defaultValue;
	}
	return result;
}

inline Imath::V3f vectorFromValues( const bp::object &value, const Imath::V3f &defaultValue )
{
	const std::vector<float> values = floatVector(
		value,
		3,
		{ defaultValue.x, defaultValue.y, defaultValue.z }
	);
	return Imath::V3f( values[0], values[1], values[2] );
}

inline Imath::V2f vector2FromValues( const bp::object &value, const Imath::V2f &defaultValue )
{
	const std::vector<float> values = floatVector(
		value,
		2,
		{ defaultValue.x, defaultValue.y }
	);
	return Imath::V2f( values[0], values[1] );
}

inline bp::list listFromVector( const Imath::V3f &value )
{
	bp::list result;
	result.append( value.x );
	result.append( value.y );
	result.append( value.z );
	return result;
}

inline bp::list listFromVector2( const Imath::V2f &value )
{
	bp::list result;
	result.append( value.x );
	result.append( value.y );
	return result;
}
