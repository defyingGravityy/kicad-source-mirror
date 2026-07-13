#include "gseim_paths.h"

#include <paths.h>

wxString GetGseimResourcePath()
{
    return PATHS::GetStockDataPath() + "/resources/gseim";
}

wxString GetGseimEbePath()
{
    return GetGseimResourcePath() + "/ebe";
}

wxString GetGseimXbePath()
{
    return GetGseimResourcePath() + "/xbe";
}

wxString GetGseimSolverParameterPath()
{
    return GetGseimResourcePath() + "/slvparms.in";
}

wxString GetGseimSubPath()
{
    return GetGseimResourcePath() + "/sub";
}

wxString GetGseimRunPath()
{
    return GetGseimResourcePath() + "/bin/run_gseim";
}

wxString GetGseimPlotterPath()
{
#if defined( __WXMSW__ )
    return GetGseimResourcePath() + "/bin/gseim_plotter/gseim_plotter.exe";
#else
    return GetGseimResourcePath() + "/bin/gseim_plotter/gseim_plotter";
#endif
}