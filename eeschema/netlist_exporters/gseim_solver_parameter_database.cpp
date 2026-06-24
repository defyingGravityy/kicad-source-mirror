#include "gseim_solver_parameter_database.h"

#include <wx/textfile.h>

bool GSEIM_SOLVER_PARAMETER_DATABASE::Load(
        const wxString& aFilename )
{
    m_parameters.clear();

    wxTextFile file;

    if( !file.Open( aFilename ) )
        return false;

    GSEIM_PARAMETER_INFO current;

    bool inParm = false;
    bool inOptions = false;

    for( size_t i = 0; i < file.GetLineCount(); i++ )
    {
        wxString line = file.GetLine( i ).Strip( wxString::both );

        if( line == "begin_parm" )
        {
            current = GSEIM_PARAMETER_INFO();
            inParm = true;
            inOptions = false;
            continue;
        }

        if( line == "end_parm" )
        {
            if( !current.keyword.IsEmpty() )
                m_parameters[ current.keyword ] = current;

            inParm = false;
            inOptions = false;
            continue;
        }

        if( !inParm )
            continue;

        if( line.StartsWith( "keyword:" ) )
        {
            current.keyword =
                line.AfterFirst( ':' ).Strip( wxString::both );

            continue;
        }

        if( line.StartsWith( "default:" ) )
        {
            current.defaultValue =
                line.AfterFirst( ':' ).Strip( wxString::both );

            continue;
        }

        if( line.StartsWith( "options:" ) )
        {
            inOptions = true;

            wxString rest =
                line.AfterFirst( ':' ).Strip( wxString::both );

            if( !rest.IsEmpty() && rest != "none" )
            {
                wxArrayString tokens = wxSplit( rest, ' ' );

                for( const wxString& token : tokens )
                {
                    wxString opt = token.Strip( wxString::both );

                    if( !opt.IsEmpty() )
                        current.options.push_back( opt );
                }
            }

            continue;
        }

        if( inOptions )
        {
            if( line.StartsWith( "+" ) )
            {
                current.options.push_back(
                    line.AfterFirst( '+' ).Strip( wxString::both ) );

                continue;
            }

            if( line.StartsWith( "keyword:" )
                || line.StartsWith( "default:" ) )
            {
                inOptions = false;
            }
        }
    }

    return true;
}

const GSEIM_PARAMETER_INFO*
GSEIM_SOLVER_PARAMETER_DATABASE::Find(
        const wxString& aKeyword ) const
{
    auto it = m_parameters.find( aKeyword );

    if( it == m_parameters.end() )
        return nullptr;

    return &it->second;
}
