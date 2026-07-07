#ifndef NETLIST_EXPORTER_GSEIM_H
#define NETLIST_EXPORTER_GSEIM_H
#include "netlist_exporter_spice.h"
#include "gseim_solve_block.h"
#include <netlist_exporters/gseim_outvar.h>
#include <sch_symbol.h>

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