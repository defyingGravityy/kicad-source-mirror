#pragma once

#include <vector>
#include <unordered_map>

#include <wx/string.h>
#include "gseim_ebe_parser.h"
struct GSEIM_XBE_INFO
{
    wxString name;

    std::vector<wxString> input_vars;
    std::vector<wxString> output_vars;

    std::unordered_map<wxString, GSEIM_PARAMETER> rparms;
    std::unordered_map<wxString, GSEIM_PARAMETER> iparms;
    std::unordered_map<wxString, GSEIM_PARAMETER> sparms;
    std::unordered_map<wxString, GSEIM_PARAMETER> stparms;
    std::unordered_map<wxString, GSEIM_PARAMETER> igparms;

    std::vector<wxString> outparms;
    std::vector<wxString> outparms_ac;
};

using GSEIM_XBE_DB = std::unordered_map<wxString, GSEIM_XBE_INFO>;

GSEIM_XBE_INFO ParseXbeFile( const wxString& aFilename );

GSEIM_XBE_DB LoadXbeDatabase( const wxString& aDirectory );