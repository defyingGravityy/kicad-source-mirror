#pragma once

#include <vector>
#include <unordered_map>

#include <wx/string.h>

struct GSEIM_COMPONENT_INFO
{
    wxString name;

    std::vector<wxString> nodes;

    std::unordered_map<wxString, wxString> rparms;
    std::unordered_map<wxString, wxString> iparms;
    std::unordered_map<wxString, wxString> sparms;
    std::unordered_map<wxString, wxString> stparms;

    std::vector<wxString> outparms;
    std::vector<wxString> outparms_ac;
};

using GSEIM_COMPONENT_DB =
    std::unordered_map<wxString, GSEIM_COMPONENT_INFO>;

GSEIM_COMPONENT_INFO ParseEbeFile(
    const wxString& aFilename );

GSEIM_COMPONENT_DB LoadEbeDatabase(
    const wxString& aDirectory );