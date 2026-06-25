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

#include "gseim_outvar.h"

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
        "/home/arsalan/Projects/jupyter_gseim/gseim_aux/ebe" );

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

            std::unordered_map<wxString, wxString> params;
            for( const auto& p : info->rparms )  params[p.first] = p.second;
            for( const auto& p : info->iparms )  params[p.first] = p.second;
            for( const auto& p : info->sparms )  params[p.first] = p.second;
            for( const auto& p : info->stparms ) params[p.first] = p.second;

            for( const SCH_FIELD& field : item.fields )
            {
                wxString fieldName = field.GetName();
                if( !fieldName.StartsWith( "Gseim." ) 
                    || fieldName == "Gseim.Type"
                    || fieldName == "Gseim.OutVars" )
                    continue;
                wxString paramName = fieldName.AfterFirst( '.' );
                wxString value = field.GetText();
                if( !value.IsEmpty() )
                    params[paramName] = value;
            }

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

// bool NETLIST_EXPORTER_SPICE::ReadSchematicAndLibraries( unsigned aNetlistOptions,
//                                                         REPORTER& aReporter )
// {
//     std::set<std::string> refNames; // Set of reference names to check for duplication.
//     int                   ncCounter = 1;
//     wxString              variant = m_schematic->GetCurrentVariant();

//     ReadDirectives( aNetlistOptions );

//     m_nets.clear();
//     m_items.clear();
//     m_referencesAlreadyFound.Clear();
//     m_libParts.clear();

//     wxFileName cacheDir;
//     cacheDir.AssignDir( PATHS::GetUserCachePath() );
//     cacheDir.AppendDir( wxT( "ibis" ) );

//     if( !cacheDir.DirExists() )
//     {
//         cacheDir.Mkdir( wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

//         if( !cacheDir.DirExists() )
//         {
//             wxLogTrace( wxT( "IBIS_CACHE:" ),
//                         wxT( "%s:%s:%d\n * failed to create ibis cache directory '%s'" ),
//                         __FILE__, __FUNCTION__, __LINE__, cacheDir.GetPath() );

//             return false;
//         }
//     }

//     wxDir    dir;
//     wxString dirName = cacheDir.GetFullPath();

//     if( !dir.Open( dirName ) )
//         return false;

//     wxFileName    thisFile;
//     wxArrayString fileList;
//     wxString      fileSpec = wxT( "*.cache" );

//     thisFile.SetPath( dirName ); // Set the base path to the cache folder

//     size_t numFilesFound = wxDir::GetAllFiles( dirName, &fileList, fileSpec );

//     for( size_t ii = 0; ii < numFilesFound; ii++ )
//     {
//         // Completes path to specific file so we can get its "last access" date
//         thisFile.SetFullName( fileList[ii] );
//         wxRemoveFile( thisFile.GetFullPath() );
//     }

//     for( SCH_SHEET_PATH& sheet : BuildSheetList( aNetlistOptions ) )
//     {
//         for( SCH_ITEM* item : sheet.LastScreen()->Items().OfType( SCH_SYMBOL_T ) )
//         {
//             SCH_SYMBOL* symbol = findNextSymbol( item, sheet );

//             if( !symbol || symbol->ResolveExcludedFromSim( &sheet, variant ) )
//                 continue;

//             try
//             {
//                 SPICE_ITEM            spiceItem;
//                 std::vector<PIN_INFO> pins = CreatePinList( symbol, sheet, true );

//                 for( const SCH_FIELD& field : symbol->GetFields() )
//                 {
//                     spiceItem.fields.emplace_back( symbol, FIELD_T::USER, field.GetName() );

//                     if( field.GetId() == FIELD_T::REFERENCE )
//                         spiceItem.fields.back().SetText( symbol->GetRef( &sheet ) );
//                     else
//                         spiceItem.fields.back().SetText( field.GetShownText( &sheet, false, 0, variant ) );
//                 }

//                 readRefName( sheet, *symbol, spiceItem, refNames );
//                 readModel( sheet, *symbol, spiceItem, variant, aReporter );
//                 readPinNumbers( *symbol, spiceItem, pins );
//                 readPinNetNames( *symbol, spiceItem, pins, ncCounter );
//                 readNodePattern( spiceItem );
//                 // TODO: transmission line handling?

//                 m_items.push_back( std::move( spiceItem ) );
//             }
//             catch( IO_ERROR& e )
//             {
//                 aReporter.Report( e.What(), RPT_SEVERITY_ERROR );
//             }
//         }
//     }

//     return !aReporter.HasMessageOfSeverity( RPT_SEVERITY_UNDEFINED | RPT_SEVERITY_ERROR );
// }

 

// void NETLIST_EXPORTER_SPICE::ReadDirectives( unsigned aNetlistOptions )
// {
//     wxString text;

//     m_directives.clear();

//     for( const SCH_SHEET_PATH& sheet : BuildSheetList( aNetlistOptions ) )
//     {
//         for( SCH_ITEM* item : sheet.LastScreen()->Items() )
//         {
//             if( item->ResolveExcludedFromSim() )
//                 continue;

//             if( item->Type() == SCH_TEXT_T )
//                 text = static_cast<SCH_TEXT*>( item )->GetShownText( &sheet, false );
//             else if( item->Type() == SCH_TEXTBOX_T )
//                 text = static_cast<SCH_TEXTBOX*>( item )->GetShownText( nullptr, &sheet, false );
//             else
//                 continue;

//             // Send anything that contains directives to SPICE
//             wxStringTokenizer tokenizer( text, "\r\n", wxTOKEN_STRTOK );
//             bool              foundDirective = false;

//             auto isDirective =
//                     []( const wxString& line, const wxString& dir )
//                     {
//                         return line == dir || line.StartsWith( dir + wxS( " " ) );
//                     };

//             while( tokenizer.HasMoreTokens() )
//             {
//                 wxString line = tokenizer.GetNextToken().Upper();

//                 if( line.StartsWith( wxT( "." ) ) )
//                 {
//                     if(    isDirective( line, wxS( ".AC" ) )
//                         || isDirective( line, wxS( ".CONTROL" ) )
//                         || isDirective( line, wxS( ".CSPARAM" ) )
//                         || isDirective( line, wxS( ".DISTO" ) )
//                         || isDirective( line, wxS( ".DC" ) )
//                         || isDirective( line, wxS( ".ELSE" ) )
//                         || isDirective( line, wxS( ".ELSEIF" ) )
//                         || isDirective( line, wxS( ".END" ) )
//                         || isDirective( line, wxS( ".ENDC" ) )
//                         || isDirective( line, wxS( ".ENDIF" ) )
//                         || isDirective( line, wxS( ".ENDS" ) )
//                         || isDirective( line, wxS( ".FOUR" ) )
//                         || isDirective( line, wxS( ".FUNC" ) )
//                         || isDirective( line, wxS( ".GLOBAL" ) )
//                         || isDirective( line, wxS( ".IC" ) )
//                         || isDirective( line, wxS( ".IF" ) )
//                         || isDirective( line, wxS( ".INCLUDE" ) )
//                         || isDirective( line, wxS( ".LIB" ) )
//                         || isDirective( line, wxS( ".MEAS" ) )
//                         || isDirective( line, wxS( ".MODEL" ) )
//                         || isDirective( line, wxS( ".NODESET" ) )
//                         || isDirective( line, wxS( ".NOISE" ) )
//                         || isDirective( line, wxS( ".OP" ) )
//                         || isDirective( line, wxS( ".OPTIONS" ) )
//                         || isDirective( line, wxS( ".PARAM" ) )
//                         || isDirective( line, wxS( ".PLOT" ) )
//                         || isDirective( line, wxS( ".PRINT" ) )
//                         || isDirective( line, wxS( ".PROBE" ) )
//                         || isDirective( line, wxS( ".PZ" ) )
//                         || isDirective( line, wxS( ".SAVE" ) )
//                         || isDirective( line, wxS( ".SENS" ) )
//                         || isDirective( line, wxS( ".SP" ) )
//                         || isDirective( line, wxS( ".SUBCKT" ) )
//                         || isDirective( line, wxS( ".TEMP" ) )
//                         || isDirective( line, wxS( ".TF" ) )
//                         || isDirective( line, wxS( ".TITLE" ) )
//                         || isDirective( line, wxS( ".TRAN" ) )
//                         || isDirective( line, wxS( ".WIDTH" ) ) )
//                     {
//                         foundDirective = true;
//                         break;
//                     }
//                 }
//                 else if( line.StartsWith( wxT( "K" ) ) )
//                 {
//                     // Check for mutual inductor declaration
//                     wxStringTokenizer line_t( line, " \t", wxTOKEN_STRTOK );

//                     // Coupling ID
//                     if( !line_t.HasMoreTokens() || !line_t.GetNextToken().StartsWith( wxT( "K" ) ) )
//                         continue;

//                     // Inductor 1 ID
//                     if( !line_t.HasMoreTokens() || !line_t.GetNextToken().StartsWith( wxT( "L" ) ) )
//                         continue;

//                     // Inductor 2 ID
//                     if( !line_t.HasMoreTokens() || !line_t.GetNextToken().StartsWith( wxT( "L" ) ) )
//                         continue;

//                     // That's probably distinctive enough not to bother trying to parse the
//                     // coupling value.  If there's anything else, assume it's the value.
//                     if( line_t.HasMoreTokens() )
//                     {
//                         foundDirective = true;
//                         break;
//                     }
//                 }
//             }

//             if( foundDirective )
//                 m_directives.emplace_back( text );
//         }
//     }
// }

 
// void NETLIST_EXPORTER_SPICE::writeItems( OUTPUTFORMATTER& aFormatter )
// {
//     for( const SPICE_ITEM& item : m_items )
//     {
//         if( !item.model->IsEnabled() )
//             continue;

//         aFormatter.Print( 0, "%s", item.model->SpiceGenerator().ItemLine( item ).c_str() );
//     }
// }
