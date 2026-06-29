#pragma once
#include <wx/string.h>
#include <map>
#include <vector>

struct GSEIM_OUTPUT_BLOCK
{
    wxString outputFile;
    std::vector<wxString> outputVars;

    GSEIM_OUTPUT_BLOCK() :
        outputFile( "output_file.dat" )
    {
    }
};

struct GSEIM_SOLVE_BLOCK
{
    wxString solveType;
    wxString initialSol;

    std::map<wxString, wxString> parameters;

    std::vector<GSEIM_OUTPUT_BLOCK> outputs;

    GSEIM_SOLVE_BLOCK() :
        solveType( "trns" ),
        initialSol( "previous" )
    {
        outputs.emplace_back();
    }
};