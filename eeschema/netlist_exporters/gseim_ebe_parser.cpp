#include "gseim_ebe_parser.h"

#include <wx/file.h>
#include <wx/textfile.h>
#include <wx/tokenzr.h>
#include <wx/dir.h>
#include <wx/filename.h>

enum class PARSE_SECTION
{
    NONE,
    RPARMS,
    IPARMS,
    SPARMS,
    STPARMS
};

static void SplitTokens(
    const wxString& aText,
    std::vector<wxString>& aOutput )
{
    wxStringTokenizer tok(
        aText,
        wxT( " \t" ),
        wxTOKEN_STRTOK );

    while( tok.HasMoreTokens() )
        aOutput.push_back( tok.GetNextToken() );
}

GSEIM_COMPONENT_INFO ParseEbeFile(
    const wxString& aFilename )
{
    GSEIM_COMPONENT_INFO info;

    wxTextFile file;

    if( !file.Open( aFilename ) )
        return info;

    PARSE_SECTION section = PARSE_SECTION::NONE;

    for( size_t i = 0; i < file.GetLineCount(); ++i )
    {
        wxString line = file.GetLine( i );

        line.Trim( true );
        line.Trim( false );

        if( line.IsEmpty() )
            continue;

        if( line.StartsWith( "#" ) )
            continue;

        if( line.StartsWith( "ebe name=" ) )
        {
            wxString tmp = line.AfterFirst( '=' );

            info.name = tmp.BeforeFirst( ' ' );
            continue;
        }

        if( line.StartsWith( "nodes:" ) )
        {
            section = PARSE_SECTION::NONE;

            SplitTokens(
                line.AfterFirst( ':' ),
                info.nodes );

            continue;
        }

        if( line.StartsWith( "outparms:" ) )
        {
            section = PARSE_SECTION::NONE;

            SplitTokens(
                line.AfterFirst( ':' ),
                info.outparms );

            continue;
        }

        if( line.StartsWith( "outparms_ac:" ) )
        {
            section = PARSE_SECTION::NONE;

            SplitTokens(
                line.AfterFirst( ':' ),
                info.outparms_ac );

            continue;
        }

        if( line.StartsWith( "rparms:" ) )
        {
            section = PARSE_SECTION::RPARMS;
            continue;
        }

        if( line.StartsWith( "iparms:" ) )
        {
            section = PARSE_SECTION::IPARMS;
            continue;
        }

        if( line.StartsWith( "sparms:" ) )
        {
            section = PARSE_SECTION::SPARMS;
            continue;
        }

        if( line.StartsWith( "stparms:" ) )
        {
            section = PARSE_SECTION::STPARMS;
            continue;
        }

        if( !line.StartsWith( "+" ) )
            continue;

        wxString text =
            line.AfterFirst( '+' );

        text.Trim( true );
        text.Trim( false );

        wxString name =
            text.BeforeFirst( '=' );

        wxString value =
            text.AfterFirst( '=' );

        switch( section )
        {
        case PARSE_SECTION::RPARMS:
            info.rparms[name] = value;
            break;

        case PARSE_SECTION::IPARMS:
            info.iparms[name] = value;
            break;

        case PARSE_SECTION::SPARMS:
            info.sparms[name] = value;
            break;

        case PARSE_SECTION::STPARMS:
            info.stparms[name] = value;
            break;

        default:
            break;
        }
    }

    return info;
}

GSEIM_COMPONENT_DB LoadEbeDatabase(
    const wxString& aDirectory )
{
    GSEIM_COMPONENT_DB db;

    wxDir dir( aDirectory );

    if( !dir.IsOpened() )
        return db;

    wxString filename;

    bool cont =
        dir.GetFirst(
            &filename,
            "*.ebe",
            wxDIR_FILES );

    while( cont )
    {
        wxFileName fn(
            aDirectory,
            filename );

        GSEIM_COMPONENT_INFO info =
            ParseEbeFile(
                fn.GetFullPath() );

        if( !info.name.IsEmpty() )
            db.emplace(
                info.name,
                std::move( info ) );

        cont =
            dir.GetNext(
                &filename );
    }

    return db;
}