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