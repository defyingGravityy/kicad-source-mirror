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

#include "gseim_component_db.h"

GSEIM_COMPONENT_DATABASE&
GSEIM_COMPONENT_DATABASE::Instance()
{
    static GSEIM_COMPONENT_DATABASE db;
    return db;
}

bool GSEIM_COMPONENT_DATABASE::Load(
    const wxString& aDirectory )
{
    m_db = LoadEbeDatabase( aDirectory );

    return !m_db.empty();
}

const GSEIM_COMPONENT_INFO*
GSEIM_COMPONENT_DATABASE::Find(
    const wxString& aType ) const
{
    auto it = m_db.find( aType );

    if( it == m_db.end() )
        return nullptr;

    return &it->second;
}