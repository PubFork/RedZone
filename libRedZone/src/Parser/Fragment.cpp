/*
 * Fragment.cpp
 *
 *  Created on: 2014
 *      Author: jc
 */

#include "Fragment.h"

#include <algorithm>

#include <Common.h>

namespace RedZone {

Fragment::Fragment( std::string const & rawText )
    : m_rawText( rawText ) {
    m_cleanText = cleanFragment();
}

std::string Fragment::cleanFragment() const {
    static const std::vector< std::string > starts { BLOCK_START_TOKEN, VAR_START_TOKEN };
    if( std::find( starts.begin(),
            starts.end(), m_rawText.substr( 0, 2 ) ) != starts.end() ) {
        std::string result;
        std::copy( m_rawText.begin() + 2, m_rawText.end() - 2, std::back_inserter( result ) );

        trimString( result );

        return result;
    }
    return m_rawText;
}

ElementType Fragment::type() const {
    std::string rawStart = m_rawText.substr( 0, 2 );
    if( rawStart == VAR_START_TOKEN ) {
        return ElementType::VarElement;
    }
    else if ( rawStart == BLOCK_START_TOKEN ) {
        return ( m_cleanText == "end" ?
                ElementType::CloseBlockFragment : ElementType::OpenBlockFragment );
    }
    return ElementType::TextFragment;
}

std::string Fragment::raw() const {
    return m_rawText;
}

std::string Fragment::clean() const {
    return m_cleanText;
}

Fragment::~Fragment() {
}

} /* namespace RedZone */
