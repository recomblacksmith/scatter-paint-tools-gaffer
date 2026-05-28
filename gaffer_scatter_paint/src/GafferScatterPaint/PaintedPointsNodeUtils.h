#pragma once

inline void appendAttachedPointsNodesRecursive( GraphComponent *parent, std::vector<AttachedPoints *> &result )
{
	if( !parent )
	{
		return;
	}

	for( const auto &child : parent->children() )
	{
		GraphComponent *childComponent = child.get();
		if( AttachedPoints *attachedPoints = IECore::runTimeCast<AttachedPoints>( childComponent ) )
		{
			result.push_back( attachedPoints );
		}

		appendAttachedPointsNodesRecursive( childComponent, result );
	}
}

inline AttachedPoints *findAttachedPointsForPaintedNode( const PaintedPoints *node )
{
	if( !node )
	{
		return nullptr;
	}

	GraphComponent *root = const_cast<PaintedPoints *>( node )->ancestor<ScriptNode>();
	if( !root )
	{
		root = const_cast<PaintedPoints *>( node )->parent<GraphComponent>();
	}
	if( !root )
	{
		return nullptr;
	}

	std::vector<AttachedPoints *> attachedPointsNodes;
	appendAttachedPointsNodesRecursive( root, attachedPointsNodes );
	for( AttachedPoints *attachedPoints : attachedPointsNodes )
	{
		if( !attachedPoints )
		{
			continue;
		}

		const Plug *pointsSource = attachedPoints->pointsPlug()->source();
		if( pointsSource == node->outPlug() )
		{
			return attachedPoints;
		}
	}

	return nullptr;
}

inline StringPlug *stringChild( const PaintedPoints *node, size_t index )
{
	return const_cast<StringPlug *>( node->getChild<StringPlug>( index ) );
}

inline IntPlug *intChild( const PaintedPoints *node, size_t index )
{
	return const_cast<IntPlug *>( node->getChild<IntPlug>( index ) );
}

inline FloatPlug *floatChild( const PaintedPoints *node, size_t index )
{
	return const_cast<FloatPlug *>( node->getChild<FloatPlug>( index ) );
}

inline BoolPlug *boolChild( const PaintedPoints *node, size_t index )
{
	return const_cast<BoolPlug *>( node->getChild<BoolPlug>( index ) );
}

inline ObjectPlug *objectChild( const PaintedPoints *node, size_t index )
{
	return const_cast<ObjectPlug *>( node->getChild<ObjectPlug>( index ) );
}

inline Plug *plugChild( const PaintedPoints *node, size_t index )
{
	return const_cast<Plug *>( node->getChild<Plug>( index ) );
}
