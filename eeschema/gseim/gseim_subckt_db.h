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