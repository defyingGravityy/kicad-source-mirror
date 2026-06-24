#pragma once
#include <wx/string.h>

struct GSEIM_OUTVAR
{
    wxString name;       // mag_of_C1_v_ac
    wxString baseName;   // C1_v_ac
    wxString expr;       // v_ac_of_C1

    bool isAc = false;
    bool isMagnitude = false;
    bool isPhase = false;
};