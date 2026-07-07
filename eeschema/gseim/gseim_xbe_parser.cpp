#include "gseim_xbe_parser.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/textfile.h>
#include <wx/tokenzr.h>

enum class PARSE_SECTION
{
    NONE,
    INPUT_VARS,
    OUTPUT_VARS,
    RPARMS,
    IPARMS,
    SPARMS,
    STPARMS,
    OUTPARMS
};

static void SplitTokens( const wxString& aText,
                         std::vector<wxString>& aOutput )
{
    wxStringTokenizer tok( aText, " \t", wxTOKEN_STRTOK );

    while( tok.HasMoreTokens() )
        aOutput.push_back( tok.GetNextToken() );
}

GSEIM_XBE_INFO ParseXbeFile( const wxString& aFilename )
{
    GSEIM_XBE_INFO info;

    wxTextFile file;

    if( !file.Open( aFilename ) )
        return info;

    auto ParseParameters = []( const wxString& aText, auto& aMap )
    {
        wxStringTokenizer tok( aText, " \t", wxTOKEN_STRTOK );

        while( tok.HasMoreTokens() )
        {
            wxString text = tok.GetNextToken();

            wxString name  = text.BeforeFirst( '=' );
            wxString value = text.AfterFirst( '=' );

            name.Trim( true );
            name.Trim( false );
            value.Trim( true );
            value.Trim( false );

            if( !name.IsEmpty() )
                aMap[name].defaultValue = value;
        }
    };

    PARSE_SECTION section = PARSE_SECTION::NONE;

    for( size_t i = 0; i < file.GetLineCount(); ++i )
    {
        wxString line = file.GetLine( i );

        line.Trim( true );
        line.Trim( false );

        if( line.IsEmpty() || line.StartsWith( "#" ) )
            continue;

        if( line.StartsWith( "xbe name=" ) )
        {
            wxString tmp = line.AfterFirst( '=' );
            info.name = tmp.BeforeFirst( ' ' );
            section = PARSE_SECTION::NONE;
            continue;
        }

        if( line.StartsWith( "input_vars:" ) )
        {
            section = PARSE_SECTION::INPUT_VARS;
            SplitTokens( line.AfterFirst( ':' ), info.inputs );
            continue;
        }

        if( line.StartsWith( "output_vars:" ) )
        {
            section = PARSE_SECTION::OUTPUT_VARS;
            SplitTokens( line.AfterFirst( ':' ), info.outputs );
            continue;
        }

        if( line.StartsWith( "outparms:" ) )
        {
            section = PARSE_SECTION::OUTPARMS;
            SplitTokens( line.AfterFirst( ':' ), info.outparms );
            continue;
        }

        if( line.StartsWith( "rparms:" ) )
        {
            section = PARSE_SECTION::RPARMS;
            ParseParameters( line.AfterFirst( ':' ), info.rparms );
            continue;
        }

        if( line.StartsWith( "iparms:" ) )
        {
            section = PARSE_SECTION::IPARMS;
            ParseParameters( line.AfterFirst( ':' ), info.iparms );
            continue;
        }

        if( line.StartsWith( "sparms:" ) )
        {
            section = PARSE_SECTION::SPARMS;
            ParseParameters( line.AfterFirst( ':' ), info.sparms );
            continue;
        }

        if( line.StartsWith( "stparms:" ) )
        {
            section = PARSE_SECTION::STPARMS;
            ParseParameters( line.AfterFirst( ':' ), info.stparms );
            continue;
        }

        if( !line.StartsWith( "+" ) )
            continue;

        wxString text = line.AfterFirst( '+' );

        switch( section )
        {
        case PARSE_SECTION::INPUT_VARS:
            SplitTokens( text, info.inputs );
            break;

        case PARSE_SECTION::OUTPUT_VARS:
            SplitTokens( text, info.outputs );
            break;

        case PARSE_SECTION::OUTPARMS:
            SplitTokens( text, info.outparms );
            break;

        case PARSE_SECTION::RPARMS:
            ParseParameters( text, info.rparms );
            break;

        case PARSE_SECTION::IPARMS:
            ParseParameters( text, info.iparms );
            break;

        case PARSE_SECTION::SPARMS:
            ParseParameters( text, info.sparms );
            break;

        case PARSE_SECTION::STPARMS:
            ParseParameters( text, info.stparms );
            break;

        default:
            break;
        }
    }

    return info;
}

GSEIM_XBE_DB LoadXbeDatabase( const wxString& aDirectory )
{
    GSEIM_XBE_DB db;

    wxDir dir( aDirectory );

    if( !dir.IsOpened() )
        return db;

    wxString filename;

    bool cont = dir.GetFirst( &filename, "*.xbe", wxDIR_FILES );

    while( cont )
    {
        wxFileName fn( aDirectory, filename );

        GSEIM_XBE_INFO info = ParseXbeFile( fn.GetFullPath() );

        if( !info.name.IsEmpty() )
            db.emplace( info.name, std::move( info ) );

        cont = dir.GetNext( &filename );
    }

    return db;
}