#include "gseim_sub_parser.h"
#include <wx/textfile.h>
#include <wx/tokenzr.h>
#include <wx/dir.h>
#include <wx/filename.h>

namespace
{
void SplitTokens( const wxString& aText, std::vector<wxString>& aOutput )
{
    wxStringTokenizer tok( aText, wxT( " \t" ), wxTOKEN_STRTOK );
    while( tok.HasMoreTokens() )
        aOutput.push_back( tok.GetNextToken() );
}
}

GSEIM_COMPONENT_INFO ParseSubFile( const wxString& aFilename )
{
    GSEIM_COMPONENT_INFO info;
    wxTextFile file;

    if( !file.Open( aFilename ) )
        return info;

    for( size_t i = 0; i < file.GetLineCount(); ++i )
    {
        wxString line = file.GetLine( i );
        line.Trim( true );
        line.Trim( false );

        if( line.IsEmpty() || line.StartsWith( "#" ) )
            continue;

        if( line.StartsWith( "begin_subckt name=" ) )
        {
            wxString tmp = line.AfterFirst( '=' );
            info.name = tmp.BeforeFirst( ' ' );
            continue;
        }

        if( line.StartsWith( "nodes:" ) )
        {
            SplitTokens( line.AfterFirst( ':' ), info.nodes );
            continue;
        }
    }

    return info;
}

GSEIM_COMPONENT_DB LoadSubDatabase( const wxString& aDirectory )
{
    GSEIM_COMPONENT_DB db;
    wxDir dir( aDirectory );

    if( !dir.IsOpened() )
        return db;

    wxString filename;
    bool cont = dir.GetFirst( &filename, "*.sub", wxDIR_FILES );

    while( cont )
    {
        wxFileName fn( aDirectory, filename );
        GSEIM_COMPONENT_INFO info = ParseSubFile( fn.GetFullPath() );

        if( !info.name.IsEmpty() )
            db.emplace( info.name, std::move( info ) );

        cont = dir.GetNext( &filename );
    }

    return db;
}