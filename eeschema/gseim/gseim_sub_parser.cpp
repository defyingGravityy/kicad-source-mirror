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

enum class PARSE_SECTION
{
    NONE,
    RPARMS,
    IPARMS,
    SPARMS,
    STPARMS,
    IGPARMS,
    OUTVAR
};

GSEIM_COMPONENT_INFO ParseSubFile( const wxString& aFilename )
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

        if( line.StartsWith( "igparms:" ) )
        {
            section = PARSE_SECTION::IGPARMS;
            continue;
        }

        if( line.StartsWith( "outvar:" ) )
        {
            section = PARSE_SECTION::OUTVAR;
            continue;
        }

        if( line.StartsWith( "ebe " )
            || line.StartsWith( "aux_nodes:" )
            || line.StartsWith( "C:" )
            || line.StartsWith( "endC" )
            || line.StartsWith( "end_subckt" ) )
        {
            section = PARSE_SECTION::NONE;
            continue;
        }

        if( section != PARSE_SECTION::NONE && line.StartsWith( "+" ) )
        {
            wxString s = line.AfterFirst( '+' );
            s.Trim( true );
            s.Trim( false );

            wxString name  = s.BeforeFirst( '=' );
            wxString value = s.AfterFirst( '=' );

            if( section == PARSE_SECTION::OUTVAR )
            {
                GSEIM_SUBCKT_OUTVAR ov;
                ov.name = name;
                ov.expr = value;
                info.outvars.push_back( ov );
                continue;
            }

            GSEIM_PARAMETER param;
            param.defaultValue = value;

            switch( section )
            {
            case PARSE_SECTION::RPARMS:
                info.rparms.emplace( name, param );
                break;

            case PARSE_SECTION::IPARMS:
                info.iparms.emplace( name, param );
                break;

            case PARSE_SECTION::SPARMS:
                info.sparms.emplace( name, param );
                break;

            case PARSE_SECTION::STPARMS:
                info.stparms.emplace( name, param );
                break;

            case PARSE_SECTION::IGPARMS:
                info.igparms.emplace( name, param );
                break;

            default:
                break;
            }

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