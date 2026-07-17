/*
 * This file is part of GSEIM Standalone.
 *
 * Copyright (C) 2026 Arsalan arsalansayed9702@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "gseim_param_parser.h"

#include <wx/tokenzr.h>

std::map<wxString, wxString> ParseGseimParams(
    const wxString& aText )
{
    std::map<wxString, wxString> params;

    wxStringTokenizer tokenizer(
        aText,
        " \t",
        wxTOKEN_STRTOK );

    while( tokenizer.HasMoreTokens() )
    {
        wxString token = tokenizer.GetNextToken();

        if( !token.Contains( '=' ) )
            continue;

        wxString name = token.BeforeFirst( '=' );
        wxString value = token.AfterFirst( '=' );

        name.Trim( true );
        name.Trim( false );
        value.Trim( true );
        value.Trim( false );

        if( name.IsEmpty() )
            continue;

        params[name] = value;
    }

    return params;
}

wxString SerializeGseimParams(
    const std::map<wxString, wxString>& aParams )
{
    wxString result;

    for( auto it = aParams.begin(); it != aParams.end(); ++it )
    {
        if( !result.IsEmpty() )
            result += " ";

        result += it->first;
        result += "=";
        result += it->second;
    }

    return result;
}