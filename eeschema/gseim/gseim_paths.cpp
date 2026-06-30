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