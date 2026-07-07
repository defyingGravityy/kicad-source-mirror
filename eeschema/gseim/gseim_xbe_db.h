#pragma once

#include "gseim_xbe_parser.h"

class GSEIM_XBE_DATABASE
{
public:
    static GSEIM_XBE_DATABASE& Instance();

    bool Load( const wxString& aDirectory );

    const GSEIM_XBE_INFO* Find(
        const wxString& aType ) const;

private:
    GSEIM_XBE_DB m_db;
};