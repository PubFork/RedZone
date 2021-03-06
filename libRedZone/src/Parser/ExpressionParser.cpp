/*
 * ExpressionParser.cpp
 *
 *      Author: jc
 */
#include "ExpressionParser.h"

#include <algorithm>
#include <math.h>
#include <regex>
#include <set>
#include <stack>
#include <vector>

#include <Common.h>
#include <Context/Context.h>
#include <Exception/ExpressionException.h>
#include <Exception/TemplateContextError.h>

#define MIN_PRIORITY 0
#define MAX_PRIORITY 3
#define ARGS_SIZE_CHECK(SIZE) if( args.size() != SIZE ) { \
   throw Exception( "Got " + to_string( args.size() ) + \
                    " arguments, expected " + to_string( SIZE ) ); \
   }

using namespace json11;

using namespace std;

namespace RedZone
{

ExpressionParser::ExpressionParser( Context const * context )
   : m_context( context )
{
   auto binaryOperators = m_context->binaryOperators();
   for( auto i = binaryOperators.begin(); i != binaryOperators.end(); ++i ) {
      string opString = get< 0 >( *i );
      copy( opString.begin(), opString.end(), inserter( m_binaryOperatorChars, m_binaryOperatorChars.begin() ) );
   }
}

Json ExpressionParser::parse( string expression ) const {
   {
      struct ValidatorData
      {
         string errorMessage;
         char open;
         char close;
      };
      // validating
      static vector< ValidatorData > const validationData {
         ValidatorData{ "Parentheses mismatch", '(', ')' },
         ValidatorData{ "Square brackets mismatch", '[', ']' },
         ValidatorData{ "Braces mismatch", '{', '}' },
      };
      stack< char > scope;
      bool betweenQuotes = false;
      string errorMessage;
      for( auto chr: expression ) {
         if( chr == '\"' ) {
            betweenQuotes = !betweenQuotes;
            continue;
         }

         if( betweenQuotes ) {
            continue;
         }

         auto matchingValidator = find_if( begin( validationData ), end( validationData ), 
            [ chr ]( auto && data ) {
            return data.open == chr || data.close == chr;
            } );

         if( matchingValidator != validationData.end() ) {
            if( chr == matchingValidator->open ) {
               scope.push( chr );
               errorMessage = matchingValidator->errorMessage;
               continue;
            }
            // chr == matchingValidator->close
            // TODO add assert!
            if( scope.top() == matchingValidator->open ) {
               scope.pop();
            }
         }
      }
      if( betweenQuotes ) {
         throw ExpressionException( expression, "Quotes mismatch" );
      }
      if( !scope.empty() ) {
         throw ExpressionException( expression, errorMessage );
      }
   }

   Json result;
   try {
      result = parseRecursive( expression );
   }
   catch( Exception const & ex ) {
      throw ExpressionException( expression, string( " occurred an exception " ) + ex.what() );
   }
   return result;
}

Json ExpressionParser::parseRecursive( string expression ) const {
   string err;

   // trying to convert the expression to json
   Json result = Json::parse( expression, err );
   if( !err.length() ) {
      return result;
   }

   trimString( expression );

   // perhaps that's a variable
   static regex const variableRegex( R"(^[a-zA-Z][a-zA-Z0-9\.]*$)" );
   smatch match;
   if( regex_match( expression, match, variableRegex ) ) {
      try {
         result = m_context->resolve( expression );
         return result;
      }
      catch ( TemplateContextError const & ) {
         // FFFFUUUUU~!
      }
   }

   // ok... then that's a complex expression, have to parse it... damn! I hate this
   if( expression.front() == '(' && expression.back() == ')' ) {
      // Ok... we might omit parentheses around this expression
      // but something like "(1 + 2) + (3 + 4)" breaks everything!
      // Checking for this sh#t
      bool skip = false;
      int pcount = 0;
      for( auto i = expression.begin(); i != expression.end() ; ++i ) {
         if( *i == '(' ) {
            pcount++;
         } else if ( *i == ')' ) {
            pcount--;
         }
         if( !pcount && i + 1 != expression.end() ) {
            skip = true;
            break;
         }
      }
      if( !skip ) {
         return parseRecursive( expression.substr( 1, expression.length() - 2 ) );
      }
   }

   // if the expression is a function call

   Context::Functions const & functions = m_context->functions();
   static regex const funcRegex( R"(^(\w+)\s*\((.+)\)$)" );
   smatch funcMatch;
   if( regex_match( expression, funcMatch, funcRegex ) ) {
      string funcName = funcMatch[ 1 ];
      Context::Functions::const_iterator foundFunc;
      if( ( foundFunc = functions.find( funcName ) ) == functions.end() ) {
         throw ExpressionException( expression, "No such function: " + funcName );
      }
      string argsString = funcMatch[ 2 ];
      vector< Json > args;
      int pcount = 0, qbcount = 0, bcount = 0;
      bool inQuotes = false;
      decltype( argsString )::const_iterator start = argsString.begin(), current = argsString.begin();
      for( ; current != argsString.end(); ++current ) {
         if( *current == '"' ) {
            inQuotes = !inQuotes;
         }
         if( inQuotes ) {
            continue;
         }
         if( *current == '(' ) {
            pcount++;
         }
         else if( *current == ')' ) {
            pcount--;
         }
         if( *current == '[' ) {
            qbcount++;
         }
         else if( *current == ']' ) {
            qbcount--;
         }
         if( *current == '{' ) {
            bcount++;
         }
         else if( *current == '}' ) {
            bcount--;
         }
         // monkeycoding... hate this!
         bool isInBrackets = pcount != 0 || qbcount != 0 || bcount != 0;
         bool nextIsEnd = current + 1 == argsString.end();
         if( !isInBrackets && ( *current == ',' || nextIsEnd ) ) {
            string argExpr;
            copy( start, current + ( nextIsEnd ? 1 : 0 ), back_inserter( argExpr ) );
            args.push_back( parseRecursive( argExpr ) );
            start = current + 1; // FIXME: oops, it's dangerous
         }
      }
      try {
         result = foundFunc->second( args );
      }
      catch( Exception const & ex ) {
         throw ExpressionException( expression, foundFunc->first + " raised exception: " + ex.what() );
      }
      return result;
   }

   // it the expression is an expression exactly...
   Context::BinaryOperators const & binaryOperators = m_context->binaryOperators();
   for( int priority = MIN_PRIORITY; priority <= MAX_PRIORITY; ++priority ) {
      int len = expression.length(), i = 0, pcount = 0;
      bool inQuotes = false;
      string lhs, rhs;
      i = len - 1;
      while( i >= 0 ) {
         if( expression[ i ] == '"' ) {
            inQuotes = !inQuotes;
         }
         if( inQuotes ) {
            i--;
            continue;
         }
         if( expression[ i ] == ')' ) {
            pcount++;
         }
         else if( expression[ i ] == '(' ) {
            pcount--;
         }
         auto finder = [ & ]( Context::BinaryOperators::value_type const & opData ) {
            if( priority != get< 1 >( opData ) )
               return false;
            string op = get< 0 >( opData );
            string possibleOp;
            int sensor = i;
            for( ;
               sensor < expression.length() && m_binaryOperatorChars.find( expression[ sensor ] ) != m_binaryOperatorChars.end();
               ++sensor ) {
               possibleOp.push_back( expression[ sensor ] );
            }
            return op == possibleOp;
         };
         Context::BinaryOperators::const_iterator opIter;
         if( !pcount && m_binaryOperatorChars.find( expression[ i ] ) != m_binaryOperatorChars.end() &&
            ( opIter = find_if( binaryOperators.begin(), binaryOperators.end(), finder ) ) != binaryOperators.end() ) {
            rhs = expression.substr( i + get< 0 >( *opIter ).length() );
            lhs = expression.substr( 0, i );
            try {
               result = get< 2 >( *opIter )( parseRecursive( lhs ), parseRecursive( rhs ) );
            }
            catch( Exception const & ex ) {
               throw ExpressionException( expression, "operator " + get< 0 >( *opIter ) + " raised exception: " + ex.what() );
            }
            return result;
         }
         i--;
      }
   }

   // else
   throw ExpressionException( expression, "Wrong syntax or undefined variable" );
}

ExpressionParser::~ExpressionParser()
{
}

} /* namespace RedZone */
