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
#include "gseim_sub_parser.h"

class GSEIM_SUBCKT_DATABASE
{
public:
    static GSEIM_SUBCKT_DATABASE& Instance();

    bool Load( const wxString& aDirectory );
    const GSEIM_COMPONENT_INFO* Find( const wxString& aType ) const;

private:
    GSEIM_COMPONENT_DB m_db;
};