#include <netlist_exporter_gseim.h>

#include <sim/sim_model.h>
#include <sim/spice_generator.h>
#include <wx/tokenzr.h>
#include <fmt/core.h>
#include <richio.h>
#include <unordered_set>
#include <set>
#include "../gseim/gseim_component_db.h"
#include "../gseim/gseim_param_parser.h"
#include <../gseim/gseim_ebe_parser.h>

#include "gseim_outvar.h"
#include "../gseim/gseim_paths.h"

#include <paths.h>
#include <wx/dir.h>

#include <sch_sheet_path.h>
#include <sch_sheet.h>
#include <sch_sheet_pin.h>
#include <sch_screen.h>
 #include <sch_pin.h>
 #include <wx/filename.h>
 #include "../gseim/gseim_subckt_db.h"
 #include <../gseim/gseim_xbe_db.h>

namespace
{


using GROUND_NETS = std::unordered_set<wxString>;

wxString NormalizeNet( const wxString& aNet,
                       const GROUND_NETS& aGroundNets )
{
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

wxString GetNetForXbeVar( const SPICE_ITEM& aItem, const GSEIM_XBE_INFO& aXbeInfo,
                           const wxString& aVarName )
{
    int pinNumber = 1;

    for( const wxString& v : aXbeInfo.input_vars )
    {
        if( v == aVarName )
            return GetNetForPin( aItem, std::to_string( pinNumber ) );

        ++pinNumber;
    }

    for( const wxString& v : aXbeInfo.output_vars )
    {
        if( v == aVarName )
            return GetNetForPin( aItem, std::to_string( pinNumber ) );

        ++pinNumber;
    }

    return wxEmptyString;
}

wxString GetNetForEbeXVar( const SPICE_ITEM& aItem, const GSEIM_COMPONENT_INFO& aInfo,
                            const wxString& aVarName )
{
    int pinNumber = static_cast<int>( aInfo.nodes.size() ) + 1;

    for( const wxString& v : aInfo.xVars )
    {
        if( v == aVarName )
            return GetNetForPin( aItem, std::to_string( pinNumber ) );

        ++pinNumber;
    }

    return wxEmptyString;
}

} // anonymous namespace

std::vector<NETLIST_EXPORTER_GSEIM::GSEIM_SUBCKT> NETLIST_EXPORTER_GSEIM::PopulateSubckts()
{
    std::vector<GSEIM_SUBCKT> subckts;

    SCH_SHEET_LIST sheetList = m_schematic->Hierarchy();

    for( const SCH_SHEET_PATH& path : sheetList )
    {
        SCH_SHEET*  sheet  = path.Last();
        SCH_SCREEN* screen = path.LastScreen();

        GSEIM_SUBCKT subckt;
        subckt.path   = path;
        subckt.isRoot = ( path.size() == 1 );
        subckt.name   = subckt.isRoot ? wxString( "ROOT" ) : sheet->GetName();

        for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
            subckt.symbols.push_back( static_cast<SCH_SYMBOL*>( item ) );

        for( SCH_SHEET_PIN* pin : sheet->GetPins() )
        {
            GSEIM_PORT port;
            port.name = pin->GetText();
            SCH_CONNECTION* conn = pin->Connection( &path );
            if( conn )
                port.net = conn->Name( true );
            subckt.ports.push_back( port );
        }

        for( SCH_ITEM* item : screen->Items().OfType( SCH_SHEET_T ) )
        {
            SCH_SHEET* child = static_cast<SCH_SHEET*>( item );
            GSEIM_INSTANCE inst;

            wxString paramText;

            for( SCH_FIELD& field : child->GetFields() )
            {
                if( field.GetName() == "Gseim.Params" )
                {
                    paramText = field.GetText();
                    break;
                }
            }

            inst.params = ParseGseimParams( paramText );

            wxFileName fn( child->GetFileName() );
            inst.name = child->GetName();
            inst.type = fn.GetName();

            for( SCH_SHEET_PIN* pin : child->GetPins() )
            {
                wxString portName = pin->GetText();
                wxString netName;

                SCH_CONNECTION* conn = pin->Connection( &path );
                if( conn )
                    netName = conn->Name( true );

                inst.portNets.emplace_back( portName, netName );
            }

            subckt.instances.push_back( inst );
        }

        subckts.push_back( std::move( subckt ) );
    }

    return subckts;
}


bool NETLIST_EXPORTER_GSEIM::ExportXbeElement( const SPICE_ITEM& aItem,
                                                const GSEIM_XBE_INFO& aXbeInfo,
                                                FILE_OUTPUTFORMATTER& aFormatter )
{
    wxString paramText = GetFieldValue( aItem, "Gseim.Params" );
    std::map<wxString, wxString> overrides = ParseGseimParams( paramText );

    aFormatter.Print( 0, "   xelement type=%s", TO_UTF8( aXbeInfo.name ) );

    auto emitVar = [&]( const wxString& varName )
    {
        wxString val = GetNetForXbeVar( aItem, aXbeInfo, varName );

        if( val.IsEmpty() )
        {
            auto it = overrides.find( varName );
            if( it != overrides.end() )
                val = it->second;
        }

        aFormatter.Print( 0, " %s=%s", TO_UTF8( varName ), TO_UTF8( val ) );
    };

    for( const wxString& v : aXbeInfo.input_vars )
        emitVar( v );

    for( const wxString& v : aXbeInfo.output_vars )
        emitVar( v );

    aFormatter.Print( 0, "\n" );

    bool any = false;

    for( const auto& [name, param] : aXbeInfo.rparms )
    {
        auto it = overrides.find( name );

        if( it != overrides.end() )
        {
            if( !any ) { aFormatter.Print( 0, "+   " ); any = true; }
            aFormatter.Print( 0, " %s=%s", TO_UTF8( name ), TO_UTF8( it->second ) );
        }
    }

    if( any )
        aFormatter.Print( 0, "\n" );

    return true;
}

bool NETLIST_EXPORTER_GSEIM::ExportSubcircuit( const wxString& aSubcktName,
                                                const wxString& aOutFileName,
                                                unsigned aNetlistOptions, REPORTER& aReporter )
{
    if( !ReadSchematicAndLibraries( aNetlistOptions, aReporter ) )
        return false;

    GSEIM_COMPONENT_DATABASE::Instance().Load( GetGseimEbePath() );

    SCH_SHEET_LIST sheetList = m_schematic->Hierarchy();

    if( sheetList.empty() )
    {
        aReporter.Report( "No schematic sheet found.", RPT_SEVERITY_ERROR );
        return false;
    }

    const SCH_SHEET_PATH& path   = sheetList[0];
    SCH_SCREEN*            screen = path.LastScreen();

    GROUND_NETS groundNets;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
    {
        SCH_SYMBOL* sym = static_cast<SCH_SYMBOL*>( item );

        wxString gseimType;
        for( SCH_FIELD& field : sym->GetFields() )
        {
            if( field.GetName() == wxT( "Gseim.Type" ) )
                gseimType = field.GetText();
        }

        if( gseimType == "gnd" )
        {
            for( SCH_PIN* pin : sym->GetPins( &path ) )
            {
                SCH_CONNECTION* conn = pin->Connection( &path );
                if( conn )
                    groundNets.insert( conn->Name( true ) );
            }
        }
    }

    std::vector<wxString> nodeNames;
    std::set<wxString> auxNodes;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_HIER_LABEL_T ) )
    {
        SCH_LABEL_BASE* label = static_cast<SCH_LABEL_BASE*>( item );
        nodeNames.push_back( label->GetText() );
    }

    // First pass: collect auxiliary nodes
    for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
    {
        SCH_SYMBOL* sym = static_cast<SCH_SYMBOL*>( item );

        wxString gseimType;

        for( SCH_FIELD& field : sym->GetFields() )
        {
            if( field.GetName() == "Gseim.Type" )
            {
                gseimType = field.GetText();
                break;
            }
        }

        if( gseimType.IsEmpty() || gseimType == "gnd" )
            continue;

        const GSEIM_COMPONENT_INFO* info =
            GSEIM_COMPONENT_DATABASE::Instance().Find( gseimType );

        if( !info )
            continue;

        int pinNumber = 1;

        for( const wxString& nodeName : info->nodes )
        {
            wxString net;

            for( SCH_PIN* pin : sym->GetPins( &path ) )
            {
                if( pin->GetNumber() == wxString::Format( "%d", pinNumber ) )
                {
                    SCH_CONNECTION* conn = pin->Connection( &path );

                    if( conn )
                        net = conn->Name( true );

                    break;
                }
            }

            if( !groundNets.count( net )
                && std::find( nodeNames.begin(), nodeNames.end(), net ) == nodeNames.end() )
            {
                auxNodes.insert( net );
            }

            ++pinNumber;
        }
    }

    FILE_OUTPUTFORMATTER formatter( aOutFileName );

    formatter.Print( 0, "begin_subckt name=%s\n", TO_UTF8( aSubcktName ) );

    formatter.Print( 0, "  nodes:" );
    for( const wxString& n : nodeNames )
        formatter.Print( 0, " %s", TO_UTF8( n ) );
    formatter.Print( 0, "\n" );

    if( !auxNodes.empty() )
    {
        formatter.Print( 0, "  aux_nodes:" );
        for( const wxString& n : auxNodes )
            formatter.Print( 0, " %s", TO_UTF8( n ) );
        formatter.Print( 0, "\n" );
    }

    const auto& rparms = m_schematic->GetGseimSubcktRparmValues();
    if( !rparms.empty() )
    {
        formatter.Print( 0, "  rparms:\n" );
        for( const auto& [name, value] : rparms )
        {
            if( !value.IsEmpty() )
                formatter.Print( 0, "+ %s=%s\n", TO_UTF8( name ), TO_UTF8( value ) );
        }
        formatter.Print( 0, "\n" );
    }

    const auto& iparms = m_schematic->GetGseimSubcktIparmValues();

    if( !iparms.empty() )
    {
        formatter.Print( 0, "  iparms:\n" );

        for( const auto& [name, value] : iparms )
        {
            if( !value.IsEmpty() )
                formatter.Print( 0, "+ %s=%s\n",
                                TO_UTF8( name ),
                                TO_UTF8( value ) );
        }

        formatter.Print( 0, "\n" );
    }

    const auto& sparms = m_schematic->GetGseimSubcktSparmValues();

    if( !sparms.empty() )
    {
        formatter.Print( 0, "  sparms:\n" );

        for( const auto& [name, value] : sparms )
        {
            if( !value.IsEmpty() )
                formatter.Print( 0, "+ %s=%s\n",
                                TO_UTF8( name ),
                                TO_UTF8( value ) );
        }

        formatter.Print( 0, "\n" );
    }
    
    const auto& outvars = m_schematic->GetGseimExplicitOutvars();
    if( !outvars.empty() )
    {
        formatter.Print( 0, "  outvar:\n" );
        for( const GSEIM_OUTVAR& ov : outvars )
        {
            formatter.Print( 0, "+   %s=%s\n", TO_UTF8( ov.name ), TO_UTF8( ov.expr ) );
        }
        formatter.Print( 0, "\n" );
    }

    for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
    {
        SCH_SYMBOL* sym = static_cast<SCH_SYMBOL*>( item );

        wxString gseimType;
        for( SCH_FIELD& field : sym->GetFields() )
        {
            if( field.GetName() == wxT( "Gseim.Type" ) )
                gseimType = field.GetText();
        }

        if( gseimType.IsEmpty() || gseimType == "gnd" )
            continue;

        const GSEIM_COMPONENT_INFO* info = GSEIM_COMPONENT_DATABASE::Instance().Find( gseimType );

        if( !info )
        {
            formatter.Print( 0, "* missing ebe definition for %s\n", TO_UTF8( sym->GetRef( &path ) ) );
            continue;
        }

        wxString paramText;
        for( SCH_FIELD& field : sym->GetFields() )
        {
            if( field.GetName() == wxT( "Gseim.Params" ) )
                paramText = field.GetText();
        }

        std::map<wxString, wxString> params = ParseGseimParams( paramText );

        formatter.Print( 0, "  ebe name=%s type=%s",
            TO_UTF8( sym->GetRef( &path ) ), TO_UTF8( gseimType ) );

        int pinNumber = 1;
        for( const wxString& nodeName : info->nodes )
        {
            wxString net;

            for( SCH_PIN* pin : sym->GetPins( &path ) )
            {
                if( pin->GetNumber() == wxString::Format( "%d", pinNumber ) )
                {
                    SCH_CONNECTION* conn = pin->Connection( &path );
                    if( conn )
                        net = conn->Name( true );
                    break;
                }
            }

            if( groundNets.count( net ) )
                net = "0";

            formatter.Print( 0, " %s=%s", TO_UTF8( nodeName ), TO_UTF8( net ) );
            ++pinNumber;
        }

        for( const auto& p : params )
            formatter.Print( 0, " %s=%s", TO_UTF8( p.first ), TO_UTF8( p.second ) );

        formatter.Print( 0, "\n" );
    }
    const wxString& cCode = m_schematic->GetGseimSubcktCCode();

    formatter.Print( 0, "  C:\n" );

    if( !cCode.IsEmpty() )
    {
        wxStringTokenizer tok( cCode, "\n", wxTOKEN_RET_EMPTY );

        while( tok.HasMoreTokens() )
            formatter.Print( 0, "    %s\n", TO_UTF8( tok.GetNextToken() ) );
    }

    formatter.Print( 0, "  endC\n" );
    formatter.Print( 0, "end_subckt\n" );

    return true;
}

bool NETLIST_EXPORTER_GSEIM::WriteNetlist( const wxString& aOutFileName, unsigned aNetlistOptions, REPORTER& aReporter )
{
    std::set<wxString> includedSubckts;
    if( !m_subcktName.IsEmpty() )
    {
        return ExportSubcircuit(
            m_subcktName,
            aOutFileName,
            aNetlistOptions,
            aReporter );
    }

    if( !ReadSchematicAndLibraries( aNetlistOptions, aReporter ) )
        return false;

    m_outvars.clear();

    GSEIM_COMPONENT_DATABASE::Instance().Load(
        GetGseimEbePath() );

    GSEIM_SUBCKT_DATABASE::Instance().Load(
        GetGseimSubPath() );

    GSEIM_XBE_DATABASE::Instance().Load(
        GetGseimXbePath() );
    
    FILE_OUTPUTFORMATTER formatter( aOutFileName );
    SCH_SHEET_LIST sheetList = m_schematic->Hierarchy();
    const SCH_SHEET_PATH& rootPath = sheetList[0];
    SCH_SCREEN* rootScreen = rootPath.LastScreen();

    for( SCH_ITEM* item : rootScreen->Items().OfType( SCH_SHEET_T ) )
    {
        SCH_SHEET* sheet = static_cast<SCH_SHEET*>( item );
        wxFileName fn( sheet->GetFileName() );
        includedSubckts.insert( fn.GetName() );
    }

    GROUND_NETS groundNets;

    for( const SPICE_ITEM& item : GetItems() )
    {
        if( item.sheetPath.size() != rootPath.size() )
            continue;   
        wxString gseimType = GetFieldValue( item, "Gseim.Type" );

        if( gseimType == "gnd" )
        {
            if( !item.pinNetNames.empty() )
                groundNets.insert( NetNameToGseim( item.pinNetNames[0] ) );

            continue;
        }

        if( !gseimType.IsEmpty()
            && !GSEIM_COMPONENT_DATABASE::Instance().Find( gseimType )
            && GSEIM_SUBCKT_DATABASE::Instance().Find( gseimType ) )
        {
            includedSubckts.insert( gseimType );
        }
    }

    const wxString& rparms = m_schematic->GetGseimGlobalRparms();
    const wxString& iparms = m_schematic->GetGseimGlobalIparms();
    const wxString& sparms = m_schematic->GetGseimGlobalSparms();
    const wxString& cCode  = m_schematic->GetGseimGparmCCode();

    wxString title = m_schematic->GetGseimTitle();

    if( !title.IsEmpty() )
        formatter.Print( 0, "title: %s\n\n", TO_UTF8( title ) );

    if( !rparms.IsEmpty()
        || !iparms.IsEmpty()
        || !sparms.IsEmpty()
        || !cCode.IsEmpty() )
    {
        formatter.Print( 0, "begin_gparms\n" );

        formatter.Print( 0, "  iparms: %s\n", TO_UTF8( iparms ) );
        formatter.Print( 0, "  sparms: %s\n", TO_UTF8( sparms ) );
        formatter.Print( 0, "  rparms: %s\n", TO_UTF8( rparms ) );

        formatter.Print( 0, "  C:\n" );

        if( !cCode.IsEmpty() )
        {
            wxStringTokenizer tok( cCode, "\n", wxTOKEN_RET_EMPTY );

            while( tok.HasMoreTokens() )
                formatter.Print( 0, "    %s\n", TO_UTF8( tok.GetNextToken() ) );
        }

        formatter.Print( 0, "  endC\n" );
        formatter.Print( 0, "end_gparms\n\n" );
    }

    for( const wxString& subType : includedSubckts )
        formatter.Print( 0, ".include %s.sub\n", TO_UTF8( subType ) );

    if( !includedSubckts.empty() )
        formatter.Print( 0, "\n" );

    formatter.Print( 0, "begin_circuit\n" );

    for( SCH_ITEM* subcircuit_item : rootScreen->Items().OfType( SCH_SHEET_T ) )
    {
        SCH_SHEET* sheet = static_cast<SCH_SHEET*>( subcircuit_item );

        std::map<wxString, wxString> params;

        params.insert( sheet->GetGseimRparmValues().begin(), sheet->GetGseimRparmValues().end() );
        params.insert( sheet->GetGseimIparmValues().begin(), sheet->GetGseimIparmValues().end() );
        params.insert( sheet->GetGseimSparmValues().begin(), sheet->GetGseimSparmValues().end() );

        wxFileName fn( sheet->GetFileName() );

        wxString instanceName = sheet->GetName();
        wxString subcktType   = fn.GetName();

        formatter.Print( 0, "   subckt name=%s type=%s\n", TO_UTF8( instanceName ), TO_UTF8( subcktType ) );

        for( SCH_SHEET_PIN* pin : sheet->GetPins() )
        {
            wxString portName = pin->GetText();
            wxString netName;

            SCH_CONNECTION* conn = pin->Connection( &rootPath );

            if( conn )
                netName = NormalizeNet( conn->Name( true ), groundNets );

            formatter.Print( 0, "+    %s=%s\n", TO_UTF8( portName ), TO_UTF8( netName ) );
        }

        for( const auto& [name, value] : params )
        {
            formatter.Print(
                0,
                "+    %s=%s\n",
                TO_UTF8( name ),
                TO_UTF8( value ) );
        }
    }

    for( const SPICE_ITEM& item : GetItems() )
    {
        if( item.sheetPath.size() != rootPath.size() )
            continue;

        wxString gseimType = GetFieldValue( item, wxT( "Gseim.Type" ) );

            if( gseimType == "gnd" )
                continue;

            if( !gseimType.IsEmpty() )
            {
                const GSEIM_COMPONENT_INFO* info =
                    GSEIM_COMPONENT_DATABASE::Instance().Find( gseimType );
                
            if( !info )
            {
                const GSEIM_XBE_INFO* xbeInfo = GSEIM_XBE_DATABASE::Instance().Find( gseimType );

                if( xbeInfo )
                {
                    ExportXbeElement( item, *xbeInfo, formatter );
                    continue;
                }

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

            for( const wxString& varName : info->xVars )
            {
                wxString val = GetNetForEbeXVar( item, *info, varName );

                if( val.IsEmpty() )
                {
                    auto it = params.find( varName );
                    if( it != params.end() )
                        val = it->second;
                }

                formatter.Print( 0, " %s=%s", TO_UTF8( varName ), TO_UTF8( val ) );
            }

            for( const auto& p : params )
            {
                if( std::find( info->xVars.begin(), info->xVars.end(), p.first ) != info->xVars.end() )
                    continue;

                formatter.Print( 0, " %s=%s", TO_UTF8( p.first ), TO_UTF8( p.second ) );
            }

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

    wxString refNode = groundNets.empty() ? wxString( "0" ) : *groundNets.begin();
    formatter.Print( 0, "   ref_node=%s\n", TO_UTF8( refNode ) );

    // outvar: section
    std::vector<GSEIM_OUTVAR> explicitOutvars =
    m_schematic->GetGseimExplicitOutvars();

    // Add subcircuit output variables
    for( SCH_ITEM* item : rootScreen->Items().OfType( SCH_SHEET_T ) )
    {
        SCH_SHEET* sheet = static_cast<SCH_SHEET*>( item );
        wxString stored = sheet->GetGseimSubcktOutVars();

        if( stored.IsEmpty() )
            continue;

        wxString instance = sheet->GetName();
        wxStringTokenizer tok( stored, " " );

        while( tok.HasMoreTokens() )
        {
            wxString var = tok.GetNextToken();

            GSEIM_OUTVAR ov;
            ov.name = var;
            ov.expr = var + "_of_" + instance;

            explicitOutvars.push_back( ov );
        }
    }

    if( !explicitOutvars.empty() )
    {
        formatter.Print( 0, "   outvar:\n" );
        std::map<wxString, GSEIM_OUTVAR> acMerged;
        for( const GSEIM_OUTVAR& ov : explicitOutvars )
        {
            if( ov.isAc )
            {
                auto it = acMerged.find( ov.baseName );
                if( it == acMerged.end() )
                {
                    acMerged[ ov.baseName ] = ov;
                }
                else
                {
                    if( !ov.name.IsEmpty() )
                        it->second.name = ov.name;

                    if( !ov.expr.IsEmpty() )
                        it->second.expr = ov.expr;
                }

                continue;
            }

            formatter.Print( 0, "+    %s=%s\n", TO_UTF8( ov.name ), TO_UTF8( ov.expr ) );
        }

        for( const auto& [base, ov] : acMerged )
        {
            formatter.Print( 0, "+    %s=%s\n", TO_UTF8( ov.name ), TO_UTF8( ov.expr ) );
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
