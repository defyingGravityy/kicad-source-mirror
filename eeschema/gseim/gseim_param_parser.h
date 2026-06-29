#pragma once

#include <map>

#include <wx/string.h>

std::map<wxString, wxString> ParseGseimParams(
    const wxString& aText );

wxString SerializeGseimParams(
    const std::map<wxString, wxString>& aParams );