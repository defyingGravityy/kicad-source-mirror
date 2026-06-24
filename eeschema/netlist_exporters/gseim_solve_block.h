#pragma once
#include <wx/string.h>
#include <map>
#include <vector>

struct GSEIM_SOLVE_BLOCK
{
    wxString solveType;
    wxString initialSol;

    std::map<wxString, wxString> parameters;

    wxString outputFile;
    std::vector<wxString> outputVars;

    GSEIM_SOLVE_BLOCK() :
        solveType( "trns" ),
        initialSol( "previous" ),
        outputFile( "output_file.dat" )
    {
    }
};