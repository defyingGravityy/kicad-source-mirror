#include "gseim_xbe_db.h"

GSEIM_XBE_DATABASE&
GSEIM_XBE_DATABASE::Instance()
{
    static GSEIM_XBE_DATABASE db;
    return db;
}

bool GSEIM_XBE_DATABASE::Load(
    const wxString& aDirectory )
{
    m_db = LoadXbeDatabase( aDirectory );

    return !m_db.empty();
}

const GSEIM_XBE_INFO*
GSEIM_XBE_DATABASE::Find(
    const wxString& aType ) const
{
    auto it = m_db.find( aType );

    if( it == m_db.end() )
        return nullptr;

    return &it->second;
}