#ifndef NETLIST_EXPORTER_GSEIM_H
#define NETLIST_EXPORTER_GSEIM_H
#include "netlist_exporter_spice.h"
#include "gseim_solve_block.h"
#include <netlist_exporters/gseim_outvar.h>

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

private:
    wxString m_solveBlock;
    std::set<wxString> m_outvars;
    std::vector<GSEIM_OUTVAR> m_explicitOutvars;
};
#endif