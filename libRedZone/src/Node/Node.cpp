/*
 * Node.cpp
 *
 *  Created on: 2014
 *      Author: jc
 */

#include "Node.h"

#include <Context/Context.h>
#include <IO/Writer.h>

using namespace std::placeholders;

namespace RedZone {

Node::Node( bool createsScope )
    : m_createsScope( createsScope ) {

}

void Node::render( Writer * stream, Context * context ) const {
    stream->write( "*** NOT IMPLEMENTED ***" );
}

void Node::renderChildren( Writer * stream, Context * context,
        std::vector< std::shared_ptr< Node > > children ) const {
    if( !children.size() )
        children = m_children;
    std::for_each( children.begin(), children.end(),
            std::bind( &Node::render, _1, stream, context ) );
}

void Node::addChild( Node * child ) {
    m_children.push_back( std::shared_ptr< Node >( child ) );
}

bool Node::createsScope() const {
    return m_createsScope;
}

json11::Json Node::resolveInContext( std::string const & name, Context const * context ) const {
    return context->resolve( name );
}

void Node::processFragment( Fragment const * fragment ) {
}

void Node::enterScope() {
}

void Node::exitScope( std::string const & ) {
}

std::string Node::name() const {
   return "Node";
}

std::vector< std::shared_ptr< Node > > const & Node::children() {
   return m_children;
}

Node::~Node() {

}

} /* namespace RedZone */
