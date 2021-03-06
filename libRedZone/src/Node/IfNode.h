/*
 * IfNode.h
 *
 *  Created on: 2014
 *      Author: jc
 */

#pragma once

#include <Context/json11.hpp>

#include "Node.h"

namespace RedZone {

class Writer;

class RZ_API IfNode: public Node {
public:

    IfNode();

    virtual void render( Writer * stream, Context * context ) const;

    virtual void processFragment( Fragment const * fragment );
    virtual void exitScope( std::string const & endTag );

    virtual std::string name() const;

    virtual ~IfNode();

protected:
    std::string m_expression;
    std::vector< std::shared_ptr< Node > > m_ifNodes;
    std::vector< std::shared_ptr< Node > > m_elseNodes;
};

} /* namespace RedZone */
