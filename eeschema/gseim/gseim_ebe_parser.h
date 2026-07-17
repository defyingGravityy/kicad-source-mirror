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

#include <vector>
#include <unordered_map>

#include <wx/string.h>

struct GSEIM_PARAMETER
{
    wxString defaultValue;
    std::vector<wxString> options;
};

struct GSEIM_SUBCKT_OUTVAR
{
    wxString name;   // e.g. "v_in"
    wxString expr;   // e.g. "v_of_Ri"
};

struct GSEIM_COMPONENT_INFO
{
    wxString name;

    std::vector<wxString> nodes;

    std::unordered_map<wxString, GSEIM_PARAMETER> rparms;
    std::unordered_map<wxString, GSEIM_PARAMETER> iparms;
    std::unordered_map<wxString, GSEIM_PARAMETER> sparms;
    std::unordered_map<wxString, GSEIM_PARAMETER> stparms;
    std::unordered_map<wxString, GSEIM_PARAMETER> igparms;

    std::vector<wxString> xVars;

    std::vector<wxString> outparms;
    std::vector<wxString> outparms_ac;

    std::vector<GSEIM_SUBCKT_OUTVAR> outvars;   // only populated when parsed from a .sub file
};

using GSEIM_COMPONENT_DB =
std::unordered_map<wxString, GSEIM_COMPONENT_INFO>;

GSEIM_COMPONENT_INFO ParseEbeFile(
const wxString& aFilename );

GSEIM_COMPONENT_DB LoadEbeDatabase(
const wxString& aDirectory );