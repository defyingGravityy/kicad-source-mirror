#include "gseim_subckt_db.h"

GSEIM_SUBCKT_DATABASE& GSEIM_SUBCKT_DATABASE::Instance()
{
    static GSEIM_SUBCKT_DATABASE db;
    return db;
}

bool GSEIM_SUBCKT_DATABASE::Load( const wxString& aDirectory )
{
    m_db = LoadSubDatabase( aDirectory );
    return !m_db.empty();
}

const GSEIM_COMPONENT_INFO* GSEIM_SUBCKT_DATABASE::Find( const wxString& aType ) const
{
    auto it = m_db.find( aType );

    if( it == m_db.end() )
        return nullptr;

    return &it->second;
}