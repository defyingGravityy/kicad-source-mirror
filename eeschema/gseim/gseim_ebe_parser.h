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