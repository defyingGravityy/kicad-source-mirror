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