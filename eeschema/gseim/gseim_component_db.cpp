#include "gseim_component_db.h"

GSEIM_COMPONENT_DATABASE&
GSEIM_COMPONENT_DATABASE::Instance()
{
    static GSEIM_COMPONENT_DATABASE db;
    return db;
}

bool GSEIM_COMPONENT_DATABASE::Load(
    const wxString& aDirectory )
{
    m_db = LoadEbeDatabase( aDirectory );

    return !m_db.empty();
}

const GSEIM_COMPONENT_INFO*
GSEIM_COMPONENT_DATABASE::Find(
    const wxString& aType ) const
{
    auto it = m_db.find( aType );

    if( it == m_db.end() )
        return nullptr;

    return &it->second;
}