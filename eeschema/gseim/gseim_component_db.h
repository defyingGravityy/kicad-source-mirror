#pragma once

#include "gseim_ebe_parser.h"

class GSEIM_COMPONENT_DATABASE
{
public:
    static GSEIM_COMPONENT_DATABASE& Instance();

    bool Load( const wxString& aDirectory );

    const GSEIM_COMPONENT_INFO* Find(
        const wxString& aType ) const;

private:
    GSEIM_COMPONENT_DB m_db;
};