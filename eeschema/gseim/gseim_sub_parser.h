#pragma once
#include "gseim_ebe_parser.h"   // reuse GSEIM_COMPONENT_INFO / GSEIM_COMPONENT_DB

GSEIM_COMPONENT_INFO ParseSubFile( const wxString& aFilename );
GSEIM_COMPONENT_DB LoadSubDatabase( const wxString& aDirectory );