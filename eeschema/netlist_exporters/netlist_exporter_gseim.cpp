#include <netlist_exporter_gseim.h>
#include <gseim_ebe_parser.h>
#include <sim/sim_model.h>
#include <sim/spice_generator.h>
#include <wx/tokenzr.h>
#include <fmt/core.h>
#include <richio.h>
#include <unordered_set>
#include <set>
#include "../gseim/gseim_component_db.h"
#include "../gseim/gseim_param_parser.h"

#include "gseim_outvar.h"
#include "../gseim/gseim_paths.h"

namespace
{


using GROUND_NETS = std::unordered_set<wxString>;

wxString NormalizeNet( const wxString& aNet,
                       const GROUND_NETS& aGroundNets )
{
    if( aGroundNets.count( aNet ) )
        return "0";

    return aNet;
}

wxString NetNameToGseim( const std::string& aNetName )
{
    wxString name( aNetName );

    if( name.StartsWith( wxT( "/" ) ) )
        name = name.Mid( 1 );

    return name;
}


std::string GetParamValue( const SPICE_ITEM& aItem, const std::string& aParamName )
{
    if( !aItem.model )
        return "";

    for( int ii = 0; ii < aItem.model->GetParamCount(); ++ii )
    {
        const SIM_MODEL::PARAM& param = aItem.model->GetParam( ii );

        if( param.info.name == aParamName )
            return param.value;
    }

    return "";
}


wxString GetNetForPin( const SPICE_ITEM& aItem, const std::string& aPinNumber )
{
    for( size_t ii = 0; ii < aItem.pinNumbers.size() && ii < aItem.pinNetNames.size(); ++ii )
    {
        if( aItem.pinNumbers[ii] == aPinNumber )
            return NetNameToGseim( aItem.pinNetNames[ii] );
    }

    return wxEmptyString;
}


wxString GetFieldValue( const SPICE_ITEM& aItem, const wxString& aFieldName )
{
    for( const SCH_FIELD& field : aItem.fields )
    {
        if( field.GetName() == aFieldName )
            return field.GetText();
    }

    return wxEmptyString;
}

wxString MakeOutvarName( const wxString& aNet )
{
    wxString name = aNet.Upper();

    for( size_t i = 0; i < name.Length(); ++i )
    {
        wxChar c = name[i];

        if( !( wxIsalnum( c ) || c == '_' ) )
            name[i] = '_';
    }

    return "V" + name;
}

} // anonymous namespace

bool NETLIST_EXPORTER_GSEIM::WriteNetlist( const wxString& aOutFileName, unsigned aNetlistOptions, REPORTER& aReporter )
{
    if( !ReadSchematicAndLibraries( aNetlistOptions, aReporter ) )
        return false;

    m_outvars.clear();

    GSEIM_COMPONENT_DATABASE::Instance().Load(
        GetGseimEbePath() );

    FILE_OUTPUTFORMATTER formatter( aOutFileName );

    formatter.Print( 0, "begin_circuit\n\n" );

    GROUND_NETS groundNets;

    for( const SPICE_ITEM& item : GetItems() )
    {
        wxString gseimType = GetFieldValue( item, wxT( "Gseim.Type" ) );
        if( gseimType == "gnd" )
        {
            if( !item.pinNetNames.empty() )
                groundNets.insert( NetNameToGseim( item.pinNetNames[0] ) );
        }
    }

    for( const SPICE_ITEM& item : GetItems() )
    {
        wxString gseimType = GetFieldValue( item, wxT( "Gseim.Type" ) );

        if( gseimType == "gnd" )
            continue;

        if( !gseimType.IsEmpty() )
        {
            const GSEIM_COMPONENT_INFO* info =
                GSEIM_COMPONENT_DATABASE::Instance().Find( gseimType );

            if( !info )
            {
                formatter.Print( 0, "* missing ebe definition for %s\n", item.refName.c_str() );
                continue;
            }

            wxString paramText = GetFieldValue( item, "Gseim.Params" );

            std::map<wxString, wxString> params = ParseGseimParams( paramText );

            formatter.Print( 0, "   eelement name=%s type=%s",
                item.refName.c_str(), TO_UTF8( gseimType ) );

            int pinNumber = 1;
            for( const wxString& nodeName : info->nodes )
            {
                wxString net = NormalizeNet(
                    GetNetForPin( item, std::to_string( pinNumber ) ), groundNets );

                if( net != "0" )
                    m_outvars.insert( net );

                formatter.Print( 0, " %s=%s", TO_UTF8( nodeName ), TO_UTF8( net ) );
                ++pinNumber;
            }

            for( const auto& p : params )
                formatter.Print( 0, " %s=%s", TO_UTF8( p.first ), TO_UTF8( p.second ) );

            formatter.Print( 0, "\n" );
            continue;
        }

        // Path 2: fallback SIM_MODEL
        if( !item.model )
        {
            formatter.Print( 0, "* skipped %s (no model)\n", item.refName.c_str() );
            continue;
        }

        wxString p = NormalizeNet( GetNetForPin( item, "1" ), groundNets );
        wxString n = NormalizeNet( GetNetForPin( item, "2" ), groundNets );

        if( p != "0" ) m_outvars.insert( p );
        if( n != "0" ) m_outvars.insert( n );

        if( p.IsEmpty() || n.IsEmpty() )
        {
            formatter.Print( 0, "* skipped %s (missing pin 1 or 2)\n", item.refName.c_str() );
            continue;
        }

        switch( item.model->GetDeviceType() )
        {
        case SIM_MODEL::DEVICE_T::R:
            formatter.Print( 0, "   eelement name=%s type=r p=%s n=%s r=%s\n",
                item.refName.c_str(), TO_UTF8( p ), TO_UTF8( n ),
                GetParamValue( item, "r" ).c_str() );
            break;
        case SIM_MODEL::DEVICE_T::C:
            formatter.Print( 0, "   eelement name=%s type=c p=%s n=%s c=%s\n",
                item.refName.c_str(), TO_UTF8( p ), TO_UTF8( n ),
                GetParamValue( item, "c" ).c_str() );
            break;
        case SIM_MODEL::DEVICE_T::L:
            formatter.Print( 0, "   eelement name=%s type=l p=%s n=%s l=%s\n",
                item.refName.c_str(), TO_UTF8( p ), TO_UTF8( n ),
                GetParamValue( item, "l" ).c_str() );
            break;
        case SIM_MODEL::DEVICE_T::V:
            formatter.Print( 0, "   eelement name=%s type=vsrc_dc p=%s n=%s vdc=%s\n",
                item.refName.c_str(), TO_UTF8( p ), TO_UTF8( n ),
                GetParamValue( item, "dc" ).c_str() );
            break;
        case SIM_MODEL::DEVICE_T::I:
            formatter.Print( 0, "   eelement name=%s type=isrc_dc p=%s n=%s idc=%s\n",
                item.refName.c_str(), TO_UTF8( p ), TO_UTF8( n ),
                GetParamValue( item, "dc" ).c_str() );
            break;
        default:
            formatter.Print( 0, "* unsupported device type for %s\n", item.refName.c_str() );
            break;
        }
    }

    formatter.Print( 0, "   ref_node=0\n" );

    // outvar: section
    bool useExplicit = !m_explicitOutvars.empty();

    if( useExplicit || !m_outvars.empty() )
    {
        formatter.Print( 0, "   outvar:\n" );

        if( useExplicit )
        {
            std::set<wxString> emittedAcBases;


            for( const GSEIM_OUTVAR& ov : m_explicitOutvars )
            {
                if( ov.isAc )
                {
                    if( emittedAcBases.count( ov.baseName ) )
                        continue;

                    emittedAcBases.insert( ov.baseName );

                    formatter.Print( 0, "+    %s=%s\n", TO_UTF8( ov.baseName ), TO_UTF8( ov.expr ) );
                }
                else
                {
                    formatter.Print( 0, "+    %s=%s\n", TO_UTF8( ov.name ), TO_UTF8( ov.expr ) );
                }
            }
        }
        else
        {
            for( const auto& net : m_outvars )
            {
                wxString varName = MakeOutvarName( net );

                formatter.Print( 0, "+    %s=nodev_of_%s\n", TO_UTF8( varName ), TO_UTF8( net ) );
            }
        }
    }

    formatter.Print( 0, "end_circuit\n\n" );

    if( !m_solveBlock.IsEmpty() )
    {
        wxString solveBlock = m_solveBlock;
        solveBlock.Trim( true ).Trim( false );
        formatter.Print( 0, "%s\n\n", TO_UTF8( solveBlock ) );
    }

    formatter.Print( 0, "end_cf\n" );

    return true;
} 
