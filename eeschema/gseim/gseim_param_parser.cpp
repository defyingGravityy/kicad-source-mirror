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