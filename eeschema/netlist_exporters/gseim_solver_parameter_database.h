#pragma once

#include <map>
#include <vector>
#include <wx/string.h>

struct GSEIM_PARAMETER_INFO
{
    wxString keyword;
    std::vector<wxString> options;
    wxString defaultValue;
};

class GSEIM_SOLVER_PARAMETER_DATABASE
{
public:
    bool Load( const wxString& aFilename );

    const GSEIM_PARAMETER_INFO* Find(
            const wxString& aKeyword ) const;

    const std::map<wxString, GSEIM_PARAMETER_INFO>& GetParameters() const
    {
        return m_parameters;
    }

private:
    std::map<wxString, GSEIM_PARAMETER_INFO> m_parameters;
};