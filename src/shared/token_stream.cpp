/**
 * slang - a simple scripting language.
 *
 * token stream implementation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "token_stream.h"

namespace slang
{

archive& operator&(
  archive& ar,
  token_stream& ts)
{
    if(ar.is_reading())
    {
        ts.tokens = std::make_shared<std::vector<token>>();
        ar&(*ts.tokens);
    }
    else
    {
        if(!ts.tokens)
        {
            ts.tokens = std::make_shared<std::vector<token>>();
        }
        ar&(*ts.tokens);
    }
    return ar;
}

}    // namespace slang
