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

#pragma once
#include <wx/string.h>
#include <map>
#include <vector>

struct GSEIM_OUTPUT_BLOCK
{
    wxString outputFile;
    wxString limitLines;
    wxString append;
    std::map<wxString, wxString> controls;   // out_tstart, out_tend, fixed_interval, min_phase, max_phase, delta_phase
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