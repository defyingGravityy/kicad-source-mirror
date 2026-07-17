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

#ifndef NETLIST_EXPORTER_GSEIM_H
#define NETLIST_EXPORTER_GSEIM_H
#include "netlist_exporter_spice.h"
#include "gseim_solve_block.h"
#include <netlist_exporters/gseim_outvar.h>
#include <sch_symbol.h>
#include "../gseim/gseim_xbe_db.h"
#include <richio.h>

class NETLIST_EXPORTER_GSEIM : public NETLIST_EXPORTER_SPICE
{
public:
    NETLIST_EXPORTER_GSEIM( SCHEMATIC* aSchematic ) :
        NETLIST_EXPORTER_SPICE( aSchematic )
    {
    }

    bool WriteNetlist( const wxString& aOutFileName,
                       unsigned aNetlistOptions,
                       REPORTER& aReporter ) override;

    void SetSolveBlock( const wxString& aSolveBlock )
    {
        m_solveBlock = aSolveBlock;
    }

    void SetExplicitOutvars( const std::vector<GSEIM_OUTVAR>& aOutvars )
    {
        m_explicitOutvars = aOutvars;
    }

    bool ExportSubcircuit( const wxString& aSubcktName, const wxString& aOutFileName,
                            unsigned aNetlistOptions, REPORTER& aReporter );

    bool ExportXbeElement( const SPICE_ITEM& aItem, const GSEIM_XBE_INFO& aXbeInfo,
                            FILE_OUTPUTFORMATTER& aFormatter );

    void SetSubcktName( const wxString& aName ) { m_subcktName = aName; }
    
private:
    wxString m_solveBlock;
    std::set<wxString> m_outvars;
    std::vector<GSEIM_OUTVAR> m_explicitOutvars;

private:
    struct GSEIM_PORT
    {
        wxString name;
        wxString net;
    };

    struct GSEIM_INSTANCE
    {
        wxString name;
        wxString type;

        std::vector<std::pair<wxString, wxString>> portNets;

        std::map<wxString, wxString> params;
    };

    struct GSEIM_SUBCKT
    {
        wxString             name;
        bool                 isRoot = false;
        SCH_SHEET_PATH       path;
        std::vector<SCH_SYMBOL*>   symbols;
        std::vector<GSEIM_PORT>    ports;
        std::vector<GSEIM_INSTANCE> instances;
    };

    std::vector<GSEIM_SUBCKT> PopulateSubckts();
    wxString m_subcktName;
};
#endif