/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013-2017 Jean-Pierre Charras, jp.charras@wanadoo.fr
 * Copyright (C) 2013 Wayne Stambaugh <stambaughw@gmail.com>
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

/* Functions relative to the dialog creating the netlist for Pcbnew.  The dialog is a notebook
 * with 7 fixed netlist formats:
 *   Pcbnew
 *   ORCADPCB2
 *   Allegro
 *   CADSTAR
 *   Pads
 *   SPICE
 *   SPICE model
 * and up to CUSTOMPANEL_COUNTMAX user programmable formats.  These external converters are
 * referred to as plugins, but they are really just external binaries.
 */

#include <properties/property.h>
#include <pgm_base.h>
#include <kiface_base.h>
#include <string_utils.h>
#include <gestfich.h>
#include <widgets/wx_html_report_panel.h>
#include <sch_edit_frame.h>
#include <dialogs/dialog_export_netlist.h>
#include <wildcards_and_files_ext.h>
#include <invoke_sch_dialog.h>
#include <netlist_exporters/netlist_exporter_spice.h>
#include <netlist_exporters/gseim_solver_parameter_database.h>
#include <netlist_exporters/netlist_exporter_gseim.h>
#include <../gseim/gseim_ebe_parser.h>
#include "../gseim/gseim_component_db.h"
#include "../gseim/gseim_param_parser.h"
#include <reporter.h>
#include <wx/listbox.h>
#include <unordered_set>
#include <set>

#include <tools/sch_selection_tool.h>
#include <sch_line.h>
#include <sch_label.h>
#include <sch_symbol.h>
#include <sch_connection.h>
#include <tool/tool_manager.h>

#include <paths.h>
#include <jobs/job_export_sch_netlist.h>
#include <kiplatform/ui.h>

#include <wx/grid.h>
#include <wx/checklst.h>
#include <eeschema_id.h>
#include <wx/checkbox.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/process.h>
#include <wx/regex.h>
#include <wx/txtstrm.h>
#include <wx/utils.h>

#include "../gseim/gseim_paths.h"

#include <thread>
#include <vector>
#include <properties/property_mgr.h>


namespace
{
std::vector<wxString> SplitCommandLine( const wxString& aCommand )
{
    std::vector<wxString> args;
    wxString              current;
    bool                  inSingle = false;
    bool                  inDouble = false;
    bool                  argStarted = false;

    for( wxUniChar c : aCommand )
    {
        if( c == '"' && !inSingle )
        {
            inDouble = !inDouble;
            argStarted = true;
            continue;
        }

        if( c == '\'' && !inDouble )
        {
            inSingle = !inSingle;
            argStarted = true;
            continue;
        }

        if( ( c == ' ' || c == '\t' || c == '\n' || c == '\r' ) && !inSingle && !inDouble )
        {
            if( argStarted || !current.IsEmpty() )
            {
                args.emplace_back( current );
                current.clear();
                argStarted = false;
            }

            continue;
        }

        current.Append( c );
        argStarted = true;
    }

    if( argStarted || !current.IsEmpty() )
        args.emplace_back( current );

    return args;
}
} // namespace


#define CUSTOMPANEL_COUNTMAX 8  // Max number of netlist plugins

/*
 * PANEL_NETLIST_INDEX values are used as index in m_PanelNetType[]
 */
enum PANEL_NETLIST_INDEX
{
    PANELPCBNEW = 0,         /* Handle Netlist format Pcbnew */
    PANELORCADPCB2,          /* Handle Netlist format OracdPcb2 */
    PANELALLEGRO,            /* Handle Netlist format Allegro */
    PANELCADSTAR,            /* Handle Netlist format CadStar */
    PANELPADS,               /* Handle Netlist format PADS */
    PANELSPICE,              /* Handle Netlist format Spice */
    PANELGSEIM,              /* Handle Netlist format Gseim */
    PANELGSEIMSUBCKT,        /* Handle Netlist format GSEIM Subcircuit */
    PANELSPICEMODEL,         /* Handle Netlist format Spice Model (subcircuit) */
    DEFINED_NETLISTS_COUNT,

    /* First auxiliary panel (custom netlists).  Subsequent ones use PANELCUSTOMBASE+1,
     * PANELCUSTOMBASE+2, etc., up to PANELCUSTOMBASE+CUSTOMPANEL_COUNTMAX-1 */
    PANELCUSTOMBASE = DEFINED_NETLISTS_COUNT
};


/* wxPanels for creating the NoteBook pages for each netlist format: */
class EXPORT_NETLIST_PAGE : public wxPanel
{
public:
    /**
     * Create a setup page for one netlist format.
     *
     * Used in Netlist format dialog box creation.
     *
     * @param parent is the wxNotebook parent.
     * @param title is the title of the notebook page.
     * @param id_NetType is the netlist ID type.
     */
    EXPORT_NETLIST_PAGE( wxNotebook* aParent, const wxString& aTitle, NETLIST_TYPE_ID aIdNetType, bool aCustom );
    ~EXPORT_NETLIST_PAGE() = default;

    /**
     * @return the name of the netlist format for this page.
     */
    const wxString GetPageNetFmtName() { return m_pageNetFmtName; }

    bool IsCustom() const { return m_custom; }

public:
    NETLIST_TYPE_ID   m_IdNetType;

    // opt to reformat passive component values (e.g. 1M -> 1Meg):
    wxCheckBox*       m_CurSheetAsRoot;
    wxCheckBox*       m_SaveAllVoltages;
    wxCheckBox*       m_SaveAllCurrents;
    wxCheckBox*       m_SaveAllDissipations;
    wxCheckBox*       m_SaveAllEvents;
    wxCheckBox*       m_RunExternalSpiceCommand;
    wxTextCtrl*       m_CommandStringCtrl;
    wxTextCtrl*       m_TitleStringCtrl;

    wxChoice*         m_GseimSolveTypeCtrl;
    wxChoice*         m_GseimInitialSolCtrl;
    // wxChoice*         m_GseimAlgorithmCtrl;
    // wxTextCtrl*       m_GseimTStartCtrl;
    // wxTextCtrl*       m_GseimTEndCtrl;
    // wxTextCtrl*       m_GseimDeltCtrl;
    wxTextCtrl*       m_GseimOutputFileCtrl;
    wxGrid* m_GseimOutvarsGrid = nullptr;

    wxButton* m_GseimAddParameterBtn = nullptr;

    wxChoice*  m_GseimBlockChoiceCtrl = nullptr;
    wxButton*   m_GseimAddBlockBtn    = nullptr;
    wxButton*   m_GseimRemoveBlockBtn = nullptr;
    wxGrid* m_GseimParametersGrid = nullptr;
    wxStaticText* m_GseimBlockSummaryLabel = nullptr;
    wxGrid* m_GseimBlockGrid = nullptr;
    wxButton* m_GseimCopyBlockBtn = nullptr;
    wxButton* m_GseimPasteBlockBtn = nullptr;

    wxButton*   m_GseimRefreshSelectionBtn   = nullptr;
    wxStaticText* m_GseimSelectionStatusLabel = nullptr;

    wxCheckBox* m_GseimFilterBySelectionCtrl = nullptr;

    wxChoice* m_GseimOutputChoiceCtrl = nullptr;
    wxButton* m_GseimAddOutputBtn = nullptr;
    wxButton* m_GseimRemoveOutputBtn = nullptr;

    wxStaticText* m_GseimRparmsLabel = nullptr;
    wxGrid*       m_GseimRparmsGrid  = nullptr;

    wxStaticText* m_GseimIparmsLabel = nullptr;
    wxGrid*       m_GseimIparmsGrid  = nullptr;

    wxStaticText* m_GseimSparmsLabel = nullptr;
    wxGrid*       m_GseimSparmsGrid  = nullptr;

    wxStaticText* m_GseimCCodeLabel = nullptr;
    wxTextCtrl*   m_GseimCCodeCtrl  = nullptr;



    wxStaticText* m_GseimGlobalRparmsLabel = nullptr;
    wxTextCtrl*   m_GseimGlobalRparmsCtrl  = nullptr;

    wxStaticText* m_GseimGlobalIparmsLabel = nullptr;
    wxTextCtrl*   m_GseimGlobalIparmsCtrl  = nullptr;

    wxStaticText* m_GseimGlobalSparmsLabel = nullptr;
    wxTextCtrl*   m_GseimGlobalSparmsCtrl  = nullptr;

    wxStaticText* m_GseimGlobalCCodeLabel = nullptr;
    wxTextCtrl*   m_GseimGlobalCCodeCtrl  = nullptr;

    wxTextCtrl* m_GseimTitleCtrl = nullptr;



    wxStaticText* m_GseimOutputLabel;
    wxStaticText* m_GseimOutvarsLabel;

    wxBoxSizer*       m_LeftBoxSizer;
    wxBoxSizer*       m_RightBoxSizer;
    wxBoxSizer*       m_RightOptionsBoxSizer;
    wxBoxSizer*       m_LowBoxSizer;

private:
    wxString          m_pageNetFmtName;
    bool              m_custom;
};


class NETLIST_DIALOG_ADD_GENERATOR : public NETLIST_DIALOG_ADD_GENERATOR_BASE
{
public:
    NETLIST_DIALOG_ADD_GENERATOR( DIALOG_EXPORT_NETLIST* parent );

    const wxString GetGeneratorTitle()  { return m_textCtrlName->GetValue(); }
    const wxString GetGeneratorTCommandLine() { return m_textCtrlCommand->GetValue(); }

    bool TransferDataFromWindow() override;

private:
    /**
     * Browse plugin files, and set m_CommandStringCtrl field.
     */
    void OnBrowseGenerators( wxCommandEvent& event ) override;

    DIALOG_EXPORT_NETLIST* m_Parent;
};


/* Event id for notebook page buttons: */
enum id_netlist {
    ID_CREATE_NETLIST = ID_END_EESCHEMA_ID_LIST + 1,
    ID_CUR_SHEET_AS_ROOT,
    ID_SAVE_ALL_VOLTAGES,
    ID_SAVE_ALL_CURRENTS,
    ID_SAVE_ALL_DISSIPATIONS,
    ID_SAVE_ALL_EVENTS,
    ID_RUN_SIMULATOR
};


EXPORT_NETLIST_PAGE::EXPORT_NETLIST_PAGE( wxNotebook* aParent, const wxString& aTitle,
                                          NETLIST_TYPE_ID aIdNetType, bool aCustom ) :
        wxPanel( aParent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL ),
        m_IdNetType( aIdNetType ),
        m_CurSheetAsRoot( nullptr ),
        m_SaveAllVoltages( nullptr ),
        m_SaveAllCurrents( nullptr ),
        m_SaveAllDissipations( nullptr ),
        m_SaveAllEvents( nullptr ),
        m_RunExternalSpiceCommand( nullptr ),
        m_CommandStringCtrl( nullptr ),
        m_TitleStringCtrl( nullptr ),
        m_GseimSolveTypeCtrl( nullptr ),
        m_GseimInitialSolCtrl( nullptr ),


        // m_GseimAlgorithmCtrl( nullptr ),

        // m_GseimTStartCtrl( nullptr ),
        // m_GseimTEndCtrl( nullptr ),
        // m_GseimDeltCtrl( nullptr ),

        m_GseimOutputFileCtrl( nullptr ),
        m_pageNetFmtName( aTitle ),
        m_custom( aCustom )
{
    aParent->AddPage( this, aTitle, false );

    wxBoxSizer* MainBoxSizer = new wxBoxSizer( wxVERTICAL );
    SetSizer( MainBoxSizer );
    wxBoxSizer* UpperBoxSizer = new wxBoxSizer( wxHORIZONTAL );
    m_LowBoxSizer = new wxBoxSizer( wxVERTICAL );
    MainBoxSizer->Add( UpperBoxSizer, 1, wxEXPAND | wxALL, 5 );
    MainBoxSizer->Add( m_LowBoxSizer, 0, wxEXPAND | wxALL, 5 );

    m_LeftBoxSizer  = new wxBoxSizer( wxVERTICAL );
    m_RightBoxSizer = new wxBoxSizer( wxVERTICAL );
    m_RightOptionsBoxSizer = new wxBoxSizer( wxVERTICAL );
    UpperBoxSizer->Add( m_LeftBoxSizer, 0, wxEXPAND | wxALL, 5 );
    UpperBoxSizer->Add( m_RightBoxSizer, 1, wxEXPAND | wxALL, 5 );
    UpperBoxSizer->Add( m_RightOptionsBoxSizer, 1, wxEXPAND | wxALL, 5 );
}


DIALOG_EXPORT_NETLIST::DIALOG_EXPORT_NETLIST( SCH_EDIT_FRAME* aEditFrame ) :
        DIALOG_EXPORT_NETLIST( aEditFrame, aEditFrame )
{
}


DIALOG_EXPORT_NETLIST::DIALOG_EXPORT_NETLIST( SCH_EDIT_FRAME* aEditFrame, wxWindow* aParent,
                                              JOB_EXPORT_SCH_NETLIST* aJob ) :
        DIALOG_EXPORT_NETLIST_BASE( aParent ),
        m_job( aJob )
{
    m_editFrame = aEditFrame;

    // Initialize the array of netlist pages
    m_PanelNetType.resize( DEFINED_NETLISTS_COUNT + CUSTOMPANEL_COUNTMAX, nullptr );

    // Add notebook pages:
    EXPORT_NETLIST_PAGE* page = nullptr;
    wxStaticText*        label = nullptr;

    page = new EXPORT_NETLIST_PAGE( m_NoteBook, wxT( "KiCad" ), NET_TYPE_PCBNEW, false );
    label = new wxStaticText( page, wxID_ANY, _( "Export netlist in KiCad format" ) );
    page->m_LeftBoxSizer->Add( label, 0, wxBOTTOM, 10 );
    m_PanelNetType[PANELPCBNEW] = page;

    page = new EXPORT_NETLIST_PAGE( m_NoteBook, wxT( "OrcadPCB2" ), NET_TYPE_ORCADPCB2, false );
    label = new wxStaticText( page, wxID_ANY, _( "Export netlist in OrcadPCB2 format" ) );
    page->m_LeftBoxSizer->Add( label, 0, wxBOTTOM, 10 );
    m_PanelNetType[PANELORCADPCB2] = page;

    page = new EXPORT_NETLIST_PAGE( m_NoteBook, wxT( "Allegro" ), NET_TYPE_ALLEGRO, false );
    label = new wxStaticText( page, wxID_ANY, _( "Export netlist in Allegro format" ) );
    page->m_LeftBoxSizer->Add( label, 0, wxBOTTOM, 10 );
    m_PanelNetType[PANELALLEGRO] = page;

    page = new EXPORT_NETLIST_PAGE( m_NoteBook, wxT( "CadStar" ), NET_TYPE_CADSTAR, false );
    label = new wxStaticText( page, wxID_ANY, _( "Export netlist in CadStar format" ) );
    page->m_LeftBoxSizer->Add( label, 0, wxBOTTOM, 10 );
    m_PanelNetType[PANELCADSTAR] = page;

    page = new EXPORT_NETLIST_PAGE( m_NoteBook, wxT( "PADS" ), NET_TYPE_PADS, false );
    label = new wxStaticText( page, wxID_ANY, _( "Export netlist in PADS format" ) );
    page->m_LeftBoxSizer->Add( label, 0, wxBOTTOM, 10 );
    m_PanelNetType[PANELPADS] = page;

    InstallPageSpice();
    InstallPageGseim();
    InstallPageGseimSubckt();
    InstallPageSpiceModel();

    if( !m_job )
    {
        m_outputPath->Hide();
        m_staticTextOutputPath->Hide();
        InstallCustomPages();

        SetupStandardButtons( { { wxID_OK,     _( "Export Netlist" ) },
                                { wxID_CANCEL, _( "Close" )          } } );
    }
    else
    {
        SetTitle( m_job->GetSettingsDialogTitle() );

        m_MessagesBox->Hide();
        m_outputPath->SetValue( m_job->GetConfiguredOutputPath() );

        SetupStandardButtons();

        // custom netlist (external invokes, not supported)
        for( int ii = 0; ii < DEFINED_NETLISTS_COUNT + CUSTOMPANEL_COUNTMAX; ++ii )
        {
            if( EXPORT_NETLIST_PAGE* candidate = m_PanelNetType[ii] )
            {
                if( candidate->GetPageNetFmtName() == JOB_EXPORT_SCH_NETLIST::GetFormatNameMap()[m_job->format] )
                {
                    m_NoteBook->ChangeSelection( ii );
                    break;
                }
            }
        }

        m_buttonAddGenerator->Hide();
        m_buttonDelGenerator->Hide();
    }

    // DIALOG_SHIM needs a unique hash_key because classname will be the same for both job and
    // non-job versions.
    m_hash_key = std::string( TO_UTF8( GetTitle() ) ) + "_v2"; 

    // Now all widgets have the size fixed, call FinishDialogSettings
    finishDialogSettings();

    SetMinSize( wxSize( 1300, 950 ) );
    SetSize( wxSize( 1300, 950 ) );
    Layout();

    updateGeneratorButtons();
}


static std::vector<GSEIM_OUTVAR> GetGseimOutvars( SCH_EDIT_FRAME* aEditFrame )
{
    std::vector<GSEIM_OUTVAR> outvars;
    std::unordered_set<wxString> seen;

    SCH_SHEET_LIST hierarchy = aEditFrame->Schematic().Hierarchy();

    for( const SCH_SHEET_PATH& sheet : hierarchy )
    {
        bool isSubckt = sheet.size() > 1;
        wxString instanceName = isSubckt ? sheet.Last()->GetName() : wxString();

        // for( SCH_ITEM* item : sheet.LastScreen()->Items().OfType( SCH_SYMBOL_T ) )
        // {
        //     SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );
        //     wxString ref = symbol->GetRef( &sheet );

        for( SCH_ITEM* item : sheet.LastScreen()->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );
            wxString ref = symbol->GetRef( &sheet );

            if( isSubckt )
            {
                wxString stored = symbol->GetGseimOutVarsForPath( sheet.Path() );

                if( stored.IsEmpty() )
                    continue;

                wxStringTokenizer tok( stored, " " );

                while( tok.HasMoreTokens() )
                {
                    wxString var = tok.GetNextToken();

                    if( var.IsEmpty() )
                        continue;

                    wxString dedupKey = var + "@" + instanceName;

                    if( seen.count( dedupKey ) )
                        continue;

                    seen.insert( dedupKey );

                    GSEIM_OUTVAR ov;
                    ov.name = var + "_" + instanceName;
                    ov.expr = var + "_of_" + instanceName;
                    outvars.push_back( ov );
                }

                continue;   // subsheet symbols never read Gseim.OutVars/NonElecVars fields
            }

            for( const wxString& fieldName : { wxString( "Gseim.OutVars" ), wxString( "Gseim.NonElecVars" ) } )
            {
                SCH_FIELD* varsField = symbol->GetField( fieldName );

                if( !varsField )
                    continue;

                wxString stored = varsField->GetText();

                if( stored.IsEmpty() )
                    continue;

                wxStringTokenizer tok( stored, " " );

                while( tok.HasMoreTokens() )
                {
                    wxString var = tok.GetNextToken();

                    if( var.IsEmpty() )
                        continue;

                    wxString dedupKey = isSubckt ? ( var + "@" + instanceName ) : var;

                    if( seen.count( dedupKey ) )
                        continue;

                    seen.insert( dedupKey );

                    GSEIM_OUTVAR ov;
                    ov.name = var;

                    if( isSubckt )
                    {
                        ov.name = var + "_" + instanceName;   // e.g. "v_out_OA1", "v_out_OA2" — unique by default
                        ov.expr = var + "_of_" + instanceName;
                        outvars.push_back( ov );
                        continue;
                    }

                    if( var.StartsWith( "mag_of_" ) )
                    {
                        ov.isAc = true;
                        ov.isMagnitude = true;

                        ov.baseName = var.Mid( 7 );   // remove "mag_of_"

                        wxString tmp = ov.baseName;

                        if( tmp.EndsWith( "_ac" ) )
                            tmp.RemoveLast( 3 );

                        int pos = tmp.Find( '_' );

                        if( pos != wxNOT_FOUND )
                        {
                            wxString refName = tmp.Left( pos );
                            wxString outparm = tmp.Mid( pos + 1 );

                            if( outparm == "S" )
                            {
                                ov.expr = "S_of_" + refName;
                            }
                            else
                            {
                                ov.expr = outparm + "_ac_of_" + refName;
                            }
                        }
                    }
                    else if( var.StartsWith( "phase_of_" ) )
                    {
                        ov.isAc = true;
                        ov.isPhase = true;

                        ov.baseName = var.Mid( 9 );   // remove "phase_of_"

                        wxString tmp = ov.baseName;

                        if( tmp.EndsWith( "_ac" ) )
                            tmp.RemoveLast( 3 );

                        int pos = tmp.Find( '_' );

                        if( pos != wxNOT_FOUND )
                        {
                            wxString refName = tmp.Left( pos );
                            wxString outparm = tmp.Mid( pos + 1 );

                            if( outparm == "S" )
                            {
                                ov.expr = "S_of_" + refName;
                            }
                            else
                            {
                                ov.expr = outparm + "_ac_of_" + refName;
                            }
                        }
                    }
                    // Element output (R1_i, R1_v, VM1_v_fb, etc.)
                    else if( var.StartsWith( ref + "_" ) )
                    {
                        wxString outparm = var.Mid( ref.Length() + 1 );
                        ov.expr = outparm + "_of_" + ref;
                    }
                    // Node voltage (vb, vc, vout, ...)
                    else if( var.StartsWith( "v" ) )
                    {
                        wxString net = var.Mid( 1 );

                        if( var.EndsWith( "_ac" ) )
                        {
                            ov.isAc = true;
                            ov.baseName = var;
                            net.RemoveLast( 3 );

                            ov.expr = "nodev_ac_of_" + net;
                        }
                        else
                        {
                            ov.expr = "nodev_of_" + net;
                        }
                    }
                    // XBE output (x1, x2, y, ...)
                    else
                    {
                        ov.expr = "xvar_of_" + var;
                    }

                    outvars.push_back( ov );
                }
            }
        }
    }

    return outvars;
}

static std::vector<wxString> GetGseimOutvarsForSelection( SCH_EDIT_FRAME* aEditFrame, const std::vector<wxString>& aAllVars )
{
    SCH_SELECTION_TOOL* selTool = aEditFrame->GetToolManager()->GetTool<SCH_SELECTION_TOOL>();
    if( !selTool )
        return aAllVars;

    SCH_SELECTION& sel = selTool->GetSelection();
    if( sel.Empty() )
        return aAllVars;

    const SCH_SHEET_PATH& sheet = aEditFrame->GetCurrentSheet();

    std::unordered_set<wxString> allowedNets;   // bare net names (no leading /)
    std::unordered_set<wxString> allowedRefs;   // symbol references e.g. "R1"

    for( EDA_ITEM* item : sel )
    {
        if( item->Type() == SCH_SYMBOL_T )
        {
            SCH_SYMBOL* sym = static_cast<SCH_SYMBOL*>( item );
            allowedRefs.insert( sym->GetRef( &sheet ) );

            for( SCH_PIN* pin : sym->GetPins( &sheet ) )
            {
                SCH_CONNECTION* conn = pin->Connection( &sheet );
                if( !conn )
                    continue;
                wxString net = conn->GetNetName();
                if( net.StartsWith( "/" ) )
                    net = net.Mid( 1 );
                if( !net.IsEmpty() )
                    allowedNets.insert( net );
            }
        }
        else if( item->Type() == SCH_LINE_T )
        {
            SCH_LINE* line = static_cast<SCH_LINE*>( item );
            if( line->GetLayer() != LAYER_WIRE )
                continue;
            SCH_CONNECTION* conn = line->Connection( &sheet );
            if( !conn )
                continue;
            wxString net = conn->GetNetName();
            if( net.StartsWith( "/" ) )
                net = net.Mid( 1 );
            if( !net.IsEmpty() )
                allowedNets.insert( net );
        }
        else if( item->Type() == SCH_LABEL_T
              || item->Type() == SCH_GLOBAL_LABEL_T
              || item->Type() == SCH_HIER_LABEL_T )
        {
            SCH_CONNECTION* conn = static_cast<SCH_ITEM*>( item )->Connection( &sheet );
            if( !conn )
                continue;
            wxString net = conn->GetNetName();
            if( net.StartsWith( "/" ) )
                net = net.Mid( 1 );
            if( !net.IsEmpty() )
                allowedNets.insert( net );
        }
    }

    // Build the allowed V<NET> set from net names
    std::unordered_set<wxString> allowedVNets;
    for( const wxString& net : allowedNets )
    {
        wxString varName = net.Upper();
        for( size_t i = 0; i < varName.Length(); ++i )
        {
            wxChar c = varName[i];
            if( !( wxIsalnum( c ) || c == '_' ) )
                varName[i] = '_';
        }
        allowedVNets.insert( "V" + varName );
    }

    std::vector<wxString> result;
    for( const wxString& var : aAllVars )
    {
        // V<NET> voltage variable
        if( allowedVNets.count( var ) )
        {
            result.push_back( var );
            continue;
        }
        // REF_outparm device variable — prefix before first '_' is the ref
        wxString prefix = var.BeforeFirst( '_' );
        if( !prefix.IsEmpty() && allowedRefs.count( prefix ) )
        {
            result.push_back( var );
        }
    }
    return result;
}

void DIALOG_EXPORT_NETLIST::PopulateGseimOutvars()
{
    PopulateGseimOutvars( m_PanelNetType[PANELGSEIM] );
}

void DIALOG_EXPORT_NETLIST::PopulateGseimOutvars( EXPORT_NETLIST_PAGE* pg )
{
    if( !pg || !pg->m_GseimOutvarsGrid )
        return;

    // Collect currently checked var names to preserve across repopulate
    std::unordered_set<wxString> checked;
    std::unordered_map<wxString, wxString> userNames;   // original name -> user-edited name
    std::unordered_map<wxString, wxString> userDescs;   // original name -> user-edited desc

    wxGrid* grid = pg->m_GseimOutvarsGrid;

    bool hasSolveBlocks = !m_GseimSolveBlocks.empty();

    for( int row = 0; row < grid->GetNumberRows(); ++row )
    {
        wxString origName = grid->GetCellValue( row, 3 ); // hidden original name col
        wxString editName = grid->GetCellValue( row, 1 );
        wxString editDesc = grid->GetCellValue( row, 2 );
        if( grid->GetCellValue( row, 0 ) == "1" )
            checked.insert( origName );
        userNames[origName] = editName;
        userDescs[origName] = editDesc;
    }

    bool filterActive = pg->m_GseimFilterBySelectionCtrl
                        && pg->m_GseimFilterBySelectionCtrl->IsChecked();

    std::vector<GSEIM_OUTVAR> vars;
    wxString statusText;

    if( filterActive )
    {
        SCH_SELECTION_TOOL* selTool =
            m_editFrame->GetToolManager()->GetTool<SCH_SELECTION_TOOL>();
        bool selectionEmpty = !selTool || selTool->GetSelection().Empty();

        // Filter by selection: keep only vars whose name appears in selection-filtered names
        std::vector<wxString> flatAll;
        for( const GSEIM_OUTVAR& ov : m_GseimAllOutvars )
            flatAll.push_back( ov.name );

        std::vector<wxString> flatFiltered =
            GetGseimOutvarsForSelection( m_editFrame, flatAll );

        std::unordered_set<wxString> filteredSet( flatFiltered.begin(), flatFiltered.end() );

        for( const GSEIM_OUTVAR& ov : m_GseimAllOutvars )
        {
            if( filteredSet.count( ov.name ) )
                vars.push_back( ov );
        }

        if( selectionEmpty )
            statusText = _( "No selection -- showing all" );
        else
            statusText = wxString::Format( _( "%d var(s) from selection" ), (int)vars.size() );
    }
    else
    {
        vars = m_GseimAllOutvars;
    }

    if( pg->m_GseimSelectionStatusLabel )
        pg->m_GseimSelectionStatusLabel->SetLabel( statusText );

    if( grid->GetNumberRows() > 0 )
        grid->DeleteRows( 0, grid->GetNumberRows() );

    grid->AppendRows( (int)vars.size() );

    for( int row = 0; row < (int)vars.size(); ++row )
    {
        const GSEIM_OUTVAR& ov = vars[row];

        // Col 0: checkbox
        grid->SetCellValue( row, 0, checked.count( ov.name ) ? "1" : "" );
        grid->SetCellEditor( row, 0, new wxGridCellBoolEditor() );
        grid->SetCellRenderer( row, 0, new wxGridCellBoolRenderer() );
        grid->SetReadOnly( row, 0, !hasSolveBlocks );

        // Col 1: var name (editable, pre-fill with user edit or default)
        wxString editName = userNames.count( ov.name ) ? userNames[ov.name] : ov.name;
        grid->SetCellValue( row, 1, editName );


        // Col 2: description (editable, pre-fill with user edit or default)
        wxString editDesc = userDescs.count( ov.name ) ? userDescs[ov.name] : ov.expr;
        grid->SetCellValue( row, 2, editDesc );

        grid->SetReadOnly( row, 1, false );
        grid->SetReadOnly( row, 2, false );

        // Col 3: hidden original name for tracking identity across edits
        grid->SetCellValue( row, 3, ov.name );
        grid->SetReadOnly( row, 3, true );
    }

    grid->SetColSize( 0, 30 );
    grid->SetColSize( 1, 140 );
    grid->SetColSize( 2, 180 );
    grid->HideCol( 3 );
}

void DIALOG_EXPORT_NETLIST::InstallPageSpice()
{
    EXPORT_NETLIST_PAGE* pg = new EXPORT_NETLIST_PAGE( m_NoteBook, wxT( "SPICE" ), NET_TYPE_SPICE, false );

    wxStaticText* label = new wxStaticText( pg, wxID_ANY, _( "Export netlist in SPICE format" ) );
    pg->m_LeftBoxSizer->Add( label, 0, wxBOTTOM, 10 );

    pg->m_CurSheetAsRoot = new wxCheckBox( pg, ID_CUR_SHEET_AS_ROOT, _( "Use current sheet as root" ) );
    pg->m_CurSheetAsRoot->SetToolTip( _( "Export netlist only for the current sheet" ) );
    pg->m_LeftBoxSizer->Add( pg->m_CurSheetAsRoot, 0, wxGROW | wxBOTTOM | wxRIGHT, 5 );

    pg->m_SaveAllVoltages = new wxCheckBox( pg, ID_SAVE_ALL_VOLTAGES, _( "Save all voltages" ) );
    pg->m_SaveAllVoltages->SetToolTip( _( "Write a directive to save all voltages (.save all)" ) );
    pg->m_LeftBoxSizer->Add( pg->m_SaveAllVoltages, 0, wxBOTTOM | wxRIGHT, 5 );

    pg->m_SaveAllCurrents = new wxCheckBox( pg, ID_SAVE_ALL_CURRENTS, _( "Save all currents" ) );
    pg->m_SaveAllCurrents->SetToolTip( _( "Write a directive to save all currents (.probe alli)" ) );
    pg->m_LeftBoxSizer->Add( pg->m_SaveAllCurrents, 0, wxBOTTOM | wxRIGHT, 5 );

    pg->m_SaveAllDissipations = new wxCheckBox( pg, ID_SAVE_ALL_DISSIPATIONS, _( "Save all power dissipations" ) );
    pg->m_SaveAllDissipations->SetToolTip( _( "Write directives to save power dissipation of all items "
                                              "(.probe p(<item>))" ) );
    pg->m_LeftBoxSizer->Add( pg->m_SaveAllDissipations, 0, wxBOTTOM | wxRIGHT, 5 );

    pg->m_SaveAllEvents = new wxCheckBox( pg, ID_SAVE_ALL_EVENTS, _( "Save all digital event data" ) );
    pg->m_SaveAllEvents->SetToolTip( _( "If not set, write a directive to prevent the saving of digital event data "
                                        "(esave none)" ) );
    pg->m_LeftBoxSizer->Add( pg->m_SaveAllEvents, 0, wxBOTTOM | wxRIGHT, 5 );


    pg->m_RunExternalSpiceCommand = new wxCheckBox( pg, ID_RUN_SIMULATOR, _( "Run external simulator command:" ) );
    pg->m_RunExternalSpiceCommand->SetToolTip( _( "Enter the command line to run SPICE\n"
                                                  "Usually '<path to SPICE binary> \"%I\"'\n"
                                                  "%I will be replaced by the netlist filepath" ) );
    pg->m_LowBoxSizer->Add( pg->m_RunExternalSpiceCommand, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5 );

    pg->m_CommandStringCtrl = new wxTextCtrl( pg, wxID_ANY, wxT( "spice \"%I\"" ) );

    pg->m_CommandStringCtrl->SetInsertionPoint( 1 );
    pg->m_LowBoxSizer->Add( pg->m_CommandStringCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5 );

    m_PanelNetType[PANELSPICE] = pg;
}

void DIALOG_EXPORT_NETLIST::InstallPageGseim()
{
    EXPORT_NETLIST_PAGE* pg = new EXPORT_NETLIST_PAGE( m_NoteBook, wxT( "GSEIM" ), NET_TYPE_GSEIM, false );


    wxStaticText* titleLabel = new wxStaticText( pg, wxID_ANY, "Title" );
    pg->m_RightOptionsBoxSizer->Add( titleLabel, 0, wxBOTTOM, 5 );
    pg->m_GseimTitleCtrl = new wxTextCtrl( pg, wxID_ANY, m_editFrame->Schematic().GetGseimTitle() );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimTitleCtrl, 0, wxEXPAND | wxBOTTOM, 10 );


    // --- Solve Block selector ---
    wxStaticText* blockLabel = new wxStaticText( pg, wxID_ANY, _( "Solve Block" ) );
    pg->m_RightBoxSizer->Add( blockLabel, 0, wxBOTTOM, 3 );


    wxBoxSizer* blockRow = new wxBoxSizer( wxHORIZONTAL );
    pg->m_GseimBlockChoiceCtrl = new wxChoice( pg, wxID_ANY );
    blockRow->Add( pg->m_GseimBlockChoiceCtrl, 1, wxRIGHT, 5 );
    pg->m_GseimCopyBlockBtn   = new wxButton( pg, wxID_ANY, "Copy" );
    pg->m_GseimPasteBlockBtn  = new wxButton( pg, wxID_ANY, "Paste" );
    pg->m_GseimAddBlockBtn    = new wxButton( pg, wxID_ANY, "Add" );
    pg->m_GseimRemoveBlockBtn = new wxButton( pg, wxID_ANY, "Remove" );
    blockRow->Add( pg->m_GseimCopyBlockBtn,   0, wxLEFT, 5 );
    blockRow->Add( pg->m_GseimPasteBlockBtn,  0, wxLEFT, 5 );
    blockRow->Add( pg->m_GseimAddBlockBtn,    0, wxLEFT, 5 );
    blockRow->Add( pg->m_GseimRemoveBlockBtn, 0, wxLEFT, 5 );
    pg->m_RightBoxSizer->Add( blockRow, 0, wxEXPAND | wxBOTTOM, 10 );

    pg->m_GseimBlockGrid = new wxGrid( pg, wxID_ANY );
    pg->m_GseimBlockGrid->CreateGrid( 0, 2 );
    pg->m_GseimBlockGrid->SetColLabelValue( 0, "#" );
    pg->m_GseimBlockGrid->SetColLabelValue( 1, "Type" );
    pg->m_GseimBlockGrid->SetRowLabelSize( 0 );
    pg->m_GseimBlockGrid->EnableEditing( false );
    pg->m_RightBoxSizer->Add( pg->m_GseimBlockGrid, 0, wxEXPAND | wxBOTTOM, 8 );

    // --- Solve Type ---
    wxStaticText* simTypeLabel = new wxStaticText( pg, wxID_ANY, _( "Solve Type" ) );
    pg->m_RightBoxSizer->Add( simTypeLabel, 0, wxBOTTOM, 3 );
    pg->m_GseimSolveTypeCtrl = new wxChoice( pg, wxID_ANY );
    pg->m_GseimSolveTypeCtrl->Append( "startup" );
    pg->m_GseimSolveTypeCtrl->Append( "dc" );
    pg->m_GseimSolveTypeCtrl->Append( "trns" );
    pg->m_GseimSolveTypeCtrl->Append( "ac" );
    pg->m_GseimSolveTypeCtrl->Append( "sss" );
    pg->m_GseimSolveTypeCtrl->SetSelection( 2 );
    pg->m_GseimSolveTypeCtrl->Bind( wxEVT_CHOICE, &DIALOG_EXPORT_NETLIST::OnGseimSolveTypeChanged, this );
    pg->m_RightBoxSizer->Add( pg->m_GseimSolveTypeCtrl, 0, wxEXPAND | wxBOTTOM, 8 );

    // --- Initial Solution ---
    wxStaticText* initLabel = new wxStaticText( pg, wxID_ANY, _( "Initial Solution" ) );
    pg->m_RightBoxSizer->Add( initLabel, 0, wxBOTTOM, 3 );
    pg->m_GseimInitialSolCtrl = new wxChoice( pg, wxID_ANY );
    pg->m_GseimInitialSolCtrl->Append( "initialize" );
    pg->m_GseimInitialSolCtrl->Append( "previous" );
    pg->m_GseimInitialSolCtrl->SetSelection( 1 );
    pg->m_RightBoxSizer->Add( pg->m_GseimInitialSolCtrl, 0, wxEXPAND | wxBOTTOM, 8 );

    // --- Output ---
    wxStaticText* outputLabel = new wxStaticText( pg, wxID_ANY, _( "Output" ) );
    pg->m_RightBoxSizer->Add( outputLabel, 0, wxBOTTOM, 3 );
    pg->m_GseimOutputChoiceCtrl = new wxChoice( pg, wxID_ANY );
    pg->m_GseimAddOutputBtn = new wxButton( pg, wxID_ANY, "+" );
    pg->m_GseimRemoveOutputBtn = new wxButton( pg, wxID_ANY, "-" );
    wxBoxSizer* outputSizer = new wxBoxSizer( wxHORIZONTAL );
    outputSizer->Add( pg->m_GseimOutputChoiceCtrl, 1, wxRIGHT, 5 );
    outputSizer->Add( pg->m_GseimAddOutputBtn, 0, wxRIGHT, 2 );
    outputSizer->Add( pg->m_GseimRemoveOutputBtn, 0 );
    pg->m_RightBoxSizer->Add( outputSizer, 0, wxEXPAND | wxBOTTOM, 8 );

    // --- Output File ---
    pg->m_GseimOutputLabel = new wxStaticText( pg, wxID_ANY, _( "Output File" ) );
    pg->m_RightBoxSizer->Add( pg->m_GseimOutputLabel, 0, wxBOTTOM, 3 );
    pg->m_GseimOutputFileCtrl = new wxTextCtrl( pg, wxID_ANY, "output_file.dat" );
    pg->m_RightBoxSizer->Add( pg->m_GseimOutputFileCtrl, 0, wxEXPAND | wxBOTTOM, 8 );

    // --- Add Parameter button ---
    m_GseimParameterDb.Load( GetGseimSolverParameterPath() );
    pg->m_GseimAddParameterBtn = new wxButton( pg, wxID_ANY, "Add Parameter" );
    pg->m_RightBoxSizer->Add( pg->m_GseimAddParameterBtn, 0, wxEXPAND | wxTOP, 5 );
    pg->m_GseimAddParameterBtn->Bind( wxEVT_BUTTON, &DIALOG_EXPORT_NETLIST::OnGseimAddParameter, this );

    pg->m_GseimGlobalCCodeLabel = new wxStaticText( pg, wxID_ANY, "C Block" );
    pg->m_RightBoxSizer->Add( pg->m_GseimGlobalCCodeLabel, 0, wxTOP | wxBOTTOM, 8 );
    pg->m_GseimGlobalCCodeCtrl = new wxTextCtrl( pg, wxID_ANY, m_editFrame->Schematic().GetGseimGparmCCode(), wxDefaultPosition, wxSize( -1, 70 ), wxTE_MULTILINE ); 
    pg->m_RightBoxSizer->Add( pg->m_GseimGlobalCCodeCtrl, 0, wxEXPAND | wxBOTTOM, 10 );

    pg->m_GseimGlobalRparmsLabel = new wxStaticText( pg, wxID_ANY, "Rparms" );
    pg->m_GseimGlobalRparmsCtrl = new wxTextCtrl( pg, wxID_ANY, m_editFrame->Schematic().GetGseimGlobalRparms() );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimGlobalRparmsLabel, 0, wxBOTTOM, 5 );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimGlobalRparmsCtrl, 0, wxEXPAND | wxBOTTOM, 10 );


    pg->m_GseimGlobalSparmsLabel = new wxStaticText( pg, wxID_ANY, "Sparms" );
    pg->m_GseimGlobalSparmsCtrl = new wxTextCtrl( pg, wxID_ANY, m_editFrame->Schematic().GetGseimGlobalSparms() );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimGlobalSparmsLabel, 0, wxBOTTOM, 5 );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimGlobalSparmsCtrl, 0, wxEXPAND | wxBOTTOM, 10 );


    pg->m_GseimGlobalIparmsLabel = new wxStaticText( pg, wxID_ANY, "Iparms" );
    pg->m_GseimGlobalIparmsCtrl = new wxTextCtrl( pg, wxID_ANY, m_editFrame->Schematic().GetGseimGlobalIparms() );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimGlobalIparmsLabel, 0, wxBOTTOM, 5 );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimGlobalIparmsCtrl, 0, wxEXPAND | wxBOTTOM, 10 );


    // --- Parameters grid ---
    pg->m_GseimParametersGrid = new wxGrid( pg, wxID_ANY );
    pg->m_GseimParametersGrid->CreateGrid( 0, 2 );
    pg->m_GseimParametersGrid->SetColLabelValue( 0, "Parameter" );
    pg->m_GseimParametersGrid->SetColLabelValue( 1, "Value" );
    pg->m_GseimParametersGrid->SetColSize( 0, 180 );
    pg->m_GseimParametersGrid->SetColSize( 1, 180 );
    pg->m_GseimParametersGrid->SetMinSize( wxSize( -1, 180 ) );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimParametersGrid, 0, wxEXPAND | wxBOTTOM, 10 );
 
    // --- Output Variables ---
    pg->m_GseimOutvarsLabel = new wxStaticText( pg, wxID_ANY, _( "Output Variables:" ) );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimOutvarsLabel, 0, wxBOTTOM, 5 );

    pg->m_GseimOutvarsGrid = new wxGrid( pg, wxID_ANY, wxDefaultPosition, wxSize( 360, 120 ) );
    pg->m_GseimOutvarsGrid->CreateGrid( 0, 4 );
    pg->m_GseimOutvarsGrid->SetColLabelValue( 0, "" );
    pg->m_GseimOutvarsGrid->SetColLabelValue( 1, "Variable" );
    pg->m_GseimOutvarsGrid->SetColLabelValue( 2, "Description" );
    pg->m_GseimOutvarsGrid->SetColLabelValue( 3, "_orig" );
    pg->m_GseimOutvarsGrid->SetRowLabelSize( 0 );
    pg->m_GseimOutvarsGrid->HideCol( 3 );
    // pg->m_GseimOutvarsGrid->SetMinSize( wxSize( -1, 220 ) );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimOutvarsGrid, 0, wxEXPAND | wxBOTTOM, 5 );
    pg->m_GseimOutvarsGrid->Bind( wxEVT_GRID_CELL_LEFT_CLICK, [this]( wxGridEvent& event )
    {
        EXPORT_NETLIST_PAGE* gseimPg = m_PanelNetType[PANELGSEIM];
        wxGrid* grid = gseimPg->m_GseimOutvarsGrid;
        if( event.GetCol() == 0 )
        {
            wxString current = grid->GetCellValue( event.GetRow(), 0 );
            grid->SetCellValue( event.GetRow(), 0, current == "1" ? "" : "1" );
            event.Skip( false );
        }
        else
        {
            event.Skip();
        }
    } );

    // Filter checkbox + Refresh button on one row
    pg->m_GseimFilterBySelectionCtrl = new wxCheckBox( pg, wxID_ANY, _( "Restrict to current selection" ) );
    pg->m_GseimRefreshSelectionBtn   = new wxButton( pg, wxID_ANY, _( "Refresh" ),
                                                     wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT );
    wxBoxSizer* filterRow = new wxBoxSizer( wxHORIZONTAL );
    filterRow->Add( pg->m_GseimFilterBySelectionCtrl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8 );
    filterRow->Add( pg->m_GseimRefreshSelectionBtn,   0, wxALIGN_CENTER_VERTICAL );
    pg->m_RightOptionsBoxSizer->Add( filterRow, 0, wxEXPAND | wxBOTTOM, 2 );

    // Status label (shows "No selection -- showing all" or "N var(s) from selection")
    pg->m_GseimSelectionStatusLabel = new wxStaticText( pg, wxID_ANY, wxEmptyString );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimSelectionStatusLabel, 0, wxBOTTOM, 8 );

    // Bind filter controls
    pg->m_GseimFilterBySelectionCtrl->Bind( wxEVT_CHECKBOX, [this, pg]( wxCommandEvent& )
        {
            PopulateGseimOutvars( pg );
        } );

    pg->m_GseimRefreshSelectionBtn->Bind( wxEVT_BUTTON, [this, pg]( wxCommandEvent& )
        {
            PopulateGseimOutvars( pg );
        } );

    // Populate all outvars and seed the listbox
    m_GseimAllOutvars = GetGseimOutvars( m_editFrame );
    

    m_PanelNetType[PANELGSEIM] = pg;

    // Wire block-management events
    BindGseimChangeHandlers( true );

    pg->m_GseimBlockChoiceCtrl->Bind( wxEVT_CHOICE,  &DIALOG_EXPORT_NETLIST::OnGseimBlockSelected,  this );
    pg->m_GseimAddBlockBtn->Bind(     wxEVT_BUTTON,  &DIALOG_EXPORT_NETLIST::OnGseimAddBlock,       this );
    pg->m_GseimRemoveBlockBtn->Bind(  wxEVT_BUTTON,  &DIALOG_EXPORT_NETLIST::OnGseimRemoveBlock,    this );
    pg->m_GseimCopyBlockBtn->Bind(    wxEVT_BUTTON,  &DIALOG_EXPORT_NETLIST::OnGseimCopyBlock,      this );
    pg->m_GseimPasteBlockBtn->Bind(   wxEVT_BUTTON,  &DIALOG_EXPORT_NETLIST::OnGseimPasteBlock,     this );
    pg->m_GseimOutputChoiceCtrl->Bind( wxEVT_CHOICE, &DIALOG_EXPORT_NETLIST::OnGseimOutputSelected, this );
    pg->m_GseimAddOutputBtn->Bind( wxEVT_BUTTON,     &DIALOG_EXPORT_NETLIST::OnGseimAddOutput,      this );
    pg->m_GseimRemoveOutputBtn->Bind( wxEVT_BUTTON,  &DIALOG_EXPORT_NETLIST::OnGseimRemoveOutput,   this );

    m_GseimSolveBlocks = m_editFrame->Schematic().GetGseimSolveBlocks();

    if( !m_GseimSolveBlocks.empty() )
        m_GseimSelectedBlock = 0;
    else
        m_GseimSelectedBlock = -1;

    RefreshGseimBlockList();

    if( m_GseimSelectedBlock >= 0 )
        PopulateGseimControls( m_GseimSelectedBlock );
    PopulateGseimOutvars( pg );
    UpdateGseimControls();
}

void DIALOG_EXPORT_NETLIST::InstallPageGseimSubckt()
{
    EXPORT_NETLIST_PAGE* pg = new EXPORT_NETLIST_PAGE( m_NoteBook, "GSEIM Subcircuit", NET_TYPE_GSEIM_SUBCKT, false );

    pg->m_GseimCCodeLabel = new wxStaticText( pg, wxID_ANY, "C Block" );
    pg->m_RightBoxSizer->Add( pg->m_GseimCCodeLabel, 0, wxTOP | wxBOTTOM, 8 );
    pg->m_GseimCCodeCtrl = new wxTextCtrl( pg, wxID_ANY, m_editFrame->Schematic().GetGseimSubcktCCode(), wxDefaultPosition, wxSize( -1, 70 ), wxTE_MULTILINE );
    pg->m_RightBoxSizer->Add( pg->m_GseimCCodeCtrl, 0, wxEXPAND | wxBOTTOM, 10 );

    pg->m_GseimRparmsLabel = new wxStaticText( pg, wxID_ANY, _( "Rparms:" ) );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimRparmsLabel, 0, wxBOTTOM, 5 );
    // pg->m_GseimRparmsGrid = new wxGrid( pg, wxID_ANY, wxDefaultPosition, wxSize( 300, 180 ) );
    pg->m_GseimRparmsGrid = new wxGrid( pg, wxID_ANY );
    pg->m_GseimRparmsGrid->CreateGrid( 0, 2 );
    pg->m_GseimRparmsGrid->SetColLabelValue( 0, "Parameter" );
    pg->m_GseimRparmsGrid->SetColLabelValue( 1, "Default Value" );
    pg->m_GseimRparmsGrid->SetRowLabelSize( 0 );
    pg->m_GseimRparmsGrid->SetColSize( 0, 180 );
    pg->m_GseimRparmsGrid->SetColSize( 1, 180 );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimRparmsGrid, 0, wxEXPAND | wxBOTTOM, 10 );


    pg->m_GseimIparmsLabel = new wxStaticText( pg, wxID_ANY, "Iparms" );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimIparmsLabel, 0, wxBOTTOM, 5 );
    pg->m_GseimIparmsGrid = new wxGrid( pg, wxID_ANY );
    pg->m_GseimIparmsGrid->CreateGrid( 0, 2 );
    pg->m_GseimIparmsGrid->SetColLabelValue( 0, "Parameter" );
    pg->m_GseimIparmsGrid->SetColLabelValue( 1, "Value" );
    pg->m_GseimIparmsGrid->SetRowLabelSize( 0 );
    pg->m_GseimIparmsGrid->SetColSize( 0, 180 );
    pg->m_GseimIparmsGrid->SetColSize( 1, 180 );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimIparmsGrid, 0, wxEXPAND | wxBOTTOM, 10 );


    pg->m_GseimSparmsLabel = new wxStaticText( pg, wxID_ANY, "Sparms" );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimSparmsLabel, 0, wxBOTTOM, 5 );
    pg->m_GseimSparmsGrid = new wxGrid( pg, wxID_ANY );
    pg->m_GseimSparmsGrid->CreateGrid( 0, 2 );
    pg->m_GseimSparmsGrid->SetColLabelValue( 0, "Parameter" );
    pg->m_GseimSparmsGrid->SetColLabelValue( 1, "Value" );
    pg->m_GseimSparmsGrid->SetRowLabelSize( 0 );
    pg->m_GseimSparmsGrid->SetColSize( 0, 180 );
    pg->m_GseimSparmsGrid->SetColSize( 1, 180 );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimSparmsGrid, 0, wxEXPAND | wxBOTTOM, 10 );



    // --- Output Variables ---
    pg->m_GseimOutvarsLabel = new wxStaticText( pg, wxID_ANY, _( "Output Variables:" ) );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimOutvarsLabel, 0, wxBOTTOM, 5 );

    pg->m_GseimOutvarsGrid = new wxGrid( pg, wxID_ANY, wxDefaultPosition, wxSize( 360, 120 ) );
    pg->m_GseimOutvarsGrid->CreateGrid( 0, 4 );
    pg->m_GseimOutvarsGrid->SetColLabelValue( 0, "" );
    pg->m_GseimOutvarsGrid->SetColLabelValue( 1, "Variable" );
    pg->m_GseimOutvarsGrid->SetColLabelValue( 2, "Description" );
    pg->m_GseimOutvarsGrid->SetColLabelValue( 3, "_orig" );
    pg->m_GseimOutvarsGrid->SetRowLabelSize( 0 );
    pg->m_GseimOutvarsGrid->HideCol( 3 );
    // pg->m_GseimOutvarsGrid->SetMinSize( wxSize( -1, 220 ) );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimOutvarsGrid, 0, wxEXPAND | wxBOTTOM, 5 );
    pg->m_GseimOutvarsGrid->Bind( wxEVT_GRID_CELL_LEFT_CLICK, [this]( wxGridEvent& event )
    {
        EXPORT_NETLIST_PAGE* gseimPg = m_PanelNetType[PANELGSEIMSUBCKT];
        wxGrid* grid = gseimPg->m_GseimOutvarsGrid;
        if( event.GetCol() == 0 )
        {
            wxString current = grid->GetCellValue( event.GetRow(), 0 );
            grid->SetCellValue( event.GetRow(), 0, current == "1" ? "" : "1" );
            event.Skip( false );
        }
        else
        {
            event.Skip();
        }
    } );
 

    // Filter checkbox + Refresh button on one row
    pg->m_GseimFilterBySelectionCtrl = new wxCheckBox( pg, wxID_ANY, _( "Restrict to current selection" ) );
    pg->m_GseimRefreshSelectionBtn   = new wxButton( pg, wxID_ANY, _( "Refresh" ),
                                                     wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT );
    wxBoxSizer* filterRow = new wxBoxSizer( wxHORIZONTAL );
    filterRow->Add( pg->m_GseimFilterBySelectionCtrl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8 );
    filterRow->Add( pg->m_GseimRefreshSelectionBtn,   0, wxALIGN_CENTER_VERTICAL );
    pg->m_RightOptionsBoxSizer->Add( filterRow, 0, wxEXPAND | wxBOTTOM, 2 );

    // Status label (shows "No selection -- showing all" or "N var(s) from selection")
    pg->m_GseimSelectionStatusLabel = new wxStaticText( pg, wxID_ANY, wxEmptyString );
    pg->m_RightOptionsBoxSizer->Add( pg->m_GseimSelectionStatusLabel, 0, wxBOTTOM, 8 );


      pg->m_GseimFilterBySelectionCtrl->Bind( wxEVT_CHECKBOX, [this, pg]( wxCommandEvent& )
        {
            PopulateGseimOutvars( pg );
            PopulateGseimSubcktParameters();
        } );

    pg->m_GseimRefreshSelectionBtn->Bind( wxEVT_BUTTON, [this, pg]( wxCommandEvent& )
        {
            PopulateGseimOutvars( pg );
            PopulateGseimSubcktParameters();
        } );

    // Populate all outvars and seed the listbox
    m_GseimAllOutvars = GetGseimOutvars( m_editFrame );
    

    m_PanelNetType[PANELGSEIMSUBCKT] = pg;

    // Wire block-management events
    BindGseimChangeHandlers( true );

    PopulateGseimSubcktParameters();
    PopulateGseimOutvars( pg );
}


void DIALOG_EXPORT_NETLIST::PopulateGseimSubcktParameters()
{
    EXPORT_NETLIST_PAGE* pg = m_PanelNetType[PANELGSEIMSUBCKT];

    if( !pg )
        return;

    SCH_SHEET_LIST hierarchy = m_editFrame->Schematic().Hierarchy();

    if( hierarchy.empty() )
        return;


    std::set<wxString> rparms;
    std::set<wxString> iparms;
    std::set<wxString> sparms;
    GSEIM_COMPONENT_DATABASE::Instance().Load( GetGseimEbePath() );
    for( const SCH_SHEET_PATH& path : hierarchy )
    {        
        if( path.size() == 1 )
            continue;        

        SCH_SCREEN* screen = path.LastScreen();

        
        for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL* sym = static_cast<SCH_SYMBOL*>( item );

            wxString gseimType;
            wxString paramText;

            for( SCH_FIELD& field : sym->GetFields() )
            {
                if( field.GetName() == "Gseim.Type" )
                    gseimType = field.GetText();
                else if( field.GetName() == "Gseim.Params" )
                    paramText = field.GetText();
            }

            if( gseimType.IsEmpty() || gseimType == "gnd" )
                continue;

            const GSEIM_COMPONENT_INFO* info = GSEIM_COMPONENT_DATABASE::Instance().Find( gseimType );

            if( !info )
                continue;

            auto params = ParseGseimParams( paramText );

        
            for( const auto& [key, value] : params )
            {
                if( info->rparms.count( key ) )
                    rparms.insert( value );

                if( info->iparms.count( key ) )
                    iparms.insert( value );

                if( info->sparms.count( key ) )
                    sparms.insert( value );
            }
        }
    }

    auto PopulateGrid = []( wxGrid* grid, const std::set<wxString>& names, const std::map<wxString, wxString>& saved )
    {
        if( !grid )
            return;

        if( grid->GetNumberRows() )
            grid->DeleteRows( 0, grid->GetNumberRows() );

        grid->AppendRows( names.size() );

        int row = 0;

        for( const wxString& name : names )
        {
            grid->SetCellValue( row, 0, name );
            grid->SetReadOnly( row, 0, true );
            auto it = saved.find( name );
            if( it != saved.end() )
                grid->SetCellValue( row, 1, it->second );

            ++row;
        }
    };

    PopulateGrid( pg->m_GseimRparmsGrid, rparms, m_editFrame->Schematic().GetGseimSubcktRparmValues() );
    PopulateGrid( pg->m_GseimIparmsGrid, iparms, m_editFrame->Schematic().GetGseimSubcktIparmValues() );
    PopulateGrid( pg->m_GseimSparmsGrid, sparms, m_editFrame->Schematic().GetGseimSubcktSparmValues() );

}

void DIALOG_EXPORT_NETLIST::OnGseimRemoveOutput( wxCommandEvent& )
{
    if( m_GseimSelectedBlock < 0 )
        return;

    GSEIM_SOLVE_BLOCK& blk =
        m_GseimSolveBlocks[m_GseimSelectedBlock];

    if( blk.outputs.size() <= 1 )
        return;

    CommitGseimControls( m_GseimSelectedBlock );

    blk.outputs.erase(
        blk.outputs.begin() + m_GseimSelectedOutput );

    if( m_GseimSelectedOutput >= (int) blk.outputs.size() )
        m_GseimSelectedOutput = blk.outputs.size() - 1;

    RefreshGseimOutputList();
    PopulateGseimControls( m_GseimSelectedBlock );
}

void DIALOG_EXPORT_NETLIST::RefreshGseimOutputList()
{
    EXPORT_NETLIST_PAGE* pg = m_PanelNetType[PANELGSEIM];

    pg->m_GseimOutputChoiceCtrl->Clear();

    if( m_GseimSelectedBlock < 0 )
        return;

    GSEIM_SOLVE_BLOCK& blk = m_GseimSolveBlocks[m_GseimSelectedBlock];

    for( size_t i = 0; i < blk.outputs.size(); ++i )
        pg->m_GseimOutputChoiceCtrl->Append(
            wxString::Format( "Output %d", int( i + 1 ) ) );

    if( blk.outputs.empty() )
    {
        m_GseimSelectedOutput = -1;
        return;
    }

    if( m_GseimSelectedOutput >= (int) blk.outputs.size() )
        m_GseimSelectedOutput = blk.outputs.size() - 1;

    if( m_GseimSelectedOutput < 0 )
        m_GseimSelectedOutput = 0;

    pg->m_GseimOutputChoiceCtrl->SetSelection( m_GseimSelectedOutput );
}

void DIALOG_EXPORT_NETLIST::OnGseimOutputSelected( wxCommandEvent& )
{
    if( m_GseimSelectedBlock < 0 )
        return;

    CommitGseimControls( m_GseimSelectedBlock );

    m_GseimSelectedOutput =
        m_PanelNetType[PANELGSEIM]
            ->m_GseimOutputChoiceCtrl
            ->GetSelection();

    PopulateGseimControls( m_GseimSelectedBlock );
}

void DIALOG_EXPORT_NETLIST::OnGseimAddOutput( wxCommandEvent& )
{
    if( m_GseimSelectedBlock < 0 )
        return;

    CommitGseimControls( m_GseimSelectedBlock );

    GSEIM_OUTPUT_BLOCK output;

    m_GseimSolveBlocks[m_GseimSelectedBlock]
        .outputs.push_back( output );

    m_GseimSelectedOutput =
        m_GseimSolveBlocks[m_GseimSelectedBlock]
            .outputs.size() - 1;

    RefreshGseimOutputList();
    PopulateGseimControls( m_GseimSelectedBlock );
}

void DIALOG_EXPORT_NETLIST::OnGseimPasteBlock( wxCommandEvent& event )
{
    if( !m_GseimClipboard )
        return;

    CommitGseimControls( m_GseimSelectedBlock );

    m_GseimSolveBlocks.push_back(
        *m_GseimClipboard );

    m_GseimSelectedBlock =
        m_GseimSolveBlocks.size() - 1;

    RefreshGseimBlockList();
    PopulateGseimControls( m_GseimSelectedBlock );
    PopulateGseimOutvars( );      
}

void DIALOG_EXPORT_NETLIST::OnGseimCopyBlock( wxCommandEvent& event )
{
    if( m_GseimSelectedBlock < 0 )
        return;

    CommitGseimControls( m_GseimSelectedBlock );

    m_GseimClipboard =
        m_GseimSolveBlocks[m_GseimSelectedBlock];
}


void DIALOG_EXPORT_NETLIST::ApplySolveTypePolicy(
    GSEIM_SOLVE_BLOCK& blk )
{
    if( blk.solveType == "trns" )
    {
        blk.parameters.clear();

        blk.parameters["algorithm_trns"] = "backward_euler";
        blk.parameters["t_start"] = "0";
        blk.parameters["t_end"]   = "0";
        blk.parameters["delt"]    = "1u";
    }
    else if( blk.solveType == "ac" )
    {
        blk.parameters.clear();

        blk.parameters["set_freq"] = "50";
    }
    else
    {
        blk.parameters.clear();
    }
}


void DIALOG_EXPORT_NETLIST::PopulateGseimParameterGrid( const GSEIM_SOLVE_BLOCK& blk )
{
    EXPORT_NETLIST_PAGE* pg = m_PanelNetType[PANELGSEIM];

    wxGrid* grid = pg->m_GseimParametersGrid;

    if( !grid )
        return;

    if( grid->GetNumberRows() > 0 )
    {
        grid->DeleteRows( 0, grid->GetNumberRows() );
    }

    int row = 0;

    for( const auto& [key, value] : blk.parameters )
    {
        grid->AppendRows( 1 );

        grid->SetCellValue( row, 0, key );
        grid->SetCellValue( row, 1, value );
        grid->SetReadOnly( row, 0, true );

        const GSEIM_PARAMETER_INFO* info =
            m_GseimParameterDb.Find( key );

        if( info &&
            !info->options.empty() &&
            !( info->options.size() == 1 &&
            info->options[0] == "none" ) )
        {
            wxArrayString choices;

            for( const wxString& option : info->options )
                choices.Add( option );

            grid->SetCellEditor( row, 1, new wxGridCellChoiceEditor( choices ) );
        }

        row++;
    }
}

void DIALOG_EXPORT_NETLIST::OnGseimAddParameter( wxCommandEvent& event )
{
    if( m_GseimSelectedBlock < 0 )
        return;

    CommitGseimControls( m_GseimSelectedBlock );

    GSEIM_SOLVE_BLOCK& block = m_GseimSolveBlocks[m_GseimSelectedBlock];
    if( block.solveType == "ac" )
        return;
    wxArrayString choices;

    for( const auto& [keyword, info] : m_GseimParameterDb.GetParameters() )
    {
        choices.Add( keyword );
    }

    wxSingleChoiceDialog dlg( this, "Select parameter", "GSEIM Parameter", choices );

    if( dlg.ShowModal() != wxID_OK )
        return;

    wxString selectedKeyword = dlg.GetStringSelection();

    const GSEIM_PARAMETER_INFO* info = m_GseimParameterDb.Find( selectedKeyword );

    if( !info )
        return;

    block.parameters[ info->keyword ] = info->defaultValue;
    PopulateGseimControls( m_GseimSelectedBlock );
}

void DIALOG_EXPORT_NETLIST::PopulateGseimControls( int index )
{   
    if( index < 0 || index >= (int)m_GseimSolveBlocks.size() )
        return;

    m_GseimUpdating = true;

    EXPORT_NETLIST_PAGE* pg = m_PanelNetType[PANELGSEIM];
    const GSEIM_SOLVE_BLOCK& blk = m_GseimSolveBlocks[index];
    const GSEIM_OUTPUT_BLOCK& output = blk.outputs[m_GseimSelectedOutput];

    BindGseimChangeHandlers( false );

    auto setChoice = []( wxChoice* ctrl, const wxString& val ) {
        int idx = ctrl->FindString( val );
        ctrl->SetSelection( idx >= 0 ? idx : 0 );
    };

    setChoice( pg->m_GseimSolveTypeCtrl,  blk.solveType  );
    setChoice( pg->m_GseimInitialSolCtrl, blk.initialSol );
    pg->m_GseimOutputFileCtrl->ChangeValue( output.outputFile );

    wxGrid* grid = pg->m_GseimOutvarsGrid;
    std::unordered_set<wxString> checked( output.outputVars.begin(), output.outputVars.end() );

    for( int row = 0; row < grid->GetNumberRows(); ++row )
    {
        wxString origName = grid->GetCellValue( row, 3 );
        grid->SetCellValue( row, 0, checked.count( origName ) ? "1" : "" );
    }

    BindGseimChangeHandlers( true );

    PopulateGseimParameterGrid( blk );
    UpdateGseimControls();

    m_GseimUpdating = false;
}

void DIALOG_EXPORT_NETLIST::BindGseimChangeHandlers( bool bind )
{
    EXPORT_NETLIST_PAGE* pg = m_PanelNetType[PANELGSEIM];

    auto act = [&]( auto* ctrl, auto eventType ) {
        if( bind )
            ctrl->Bind( eventType, &DIALOG_EXPORT_NETLIST::OnGseimControlChanged, this );
        else
            ctrl->Unbind( eventType, &DIALOG_EXPORT_NETLIST::OnGseimControlChanged, this );
    };

    act( pg->m_GseimSolveTypeCtrl,  wxEVT_CHOICE      );
    act( pg->m_GseimInitialSolCtrl, wxEVT_CHOICE      );
    // act( pg->m_GseimAlgorithmCtrl,  wxEVT_CHOICE      );
    // act( pg->m_GseimTStartCtrl,     wxEVT_TEXT        );
    // act( pg->m_GseimTEndCtrl,       wxEVT_TEXT        );
    // act( pg->m_GseimDeltCtrl,       wxEVT_TEXT        );
    act( pg->m_GseimOutputFileCtrl, wxEVT_TEXT        );
}

void DIALOG_EXPORT_NETLIST::CommitGseimControls( int index )
{
    if( index < 0 || index >= (int)m_GseimSolveBlocks.size() )
        return;

    EXPORT_NETLIST_PAGE* pg = m_PanelNetType[PANELGSEIM];
    GSEIM_SOLVE_BLOCK& blk = m_GseimSolveBlocks[index];

    blk.solveType  = pg->m_GseimSolveTypeCtrl->GetStringSelection();
    blk.initialSol = pg->m_GseimInitialSolCtrl->GetStringSelection();
    GSEIM_OUTPUT_BLOCK& output = blk.outputs[m_GseimSelectedOutput];
    output.outputFile = pg->m_GseimOutputFileCtrl->GetValue();

    wxGrid* grid = pg->m_GseimParametersGrid;
    blk.parameters.clear();
    for( int row = 0; row < grid->GetNumberRows(); ++row )
    {
        wxString key   = grid->GetCellValue( row, 0 );
        wxString value = grid->GetCellValue( row, 1 );
        if( !key.IsEmpty() )
            blk.parameters[key] = value;
    }

    output.outputVars.clear();
    wxGrid* ovGrid = pg->m_GseimOutvarsGrid;
    ovGrid->SaveEditControlValue();
    ovGrid->DisableCellEditControl();
    for( int i = 0; i < ovGrid->GetNumberRows(); ++i )
    {
        if( ovGrid->GetCellValue( i, 0 ) == "1" )
            output.outputVars.push_back( ovGrid->GetCellValue( i, 3 ) );   // was column 1
    }

    m_editFrame->Schematic().SetGseimSolveBlocks( m_GseimSolveBlocks );
}

void DIALOG_EXPORT_NETLIST::OnGseimControlChanged( wxCommandEvent& event )
{   
    if( m_GseimUpdating )
    {
        event.Skip();
        return;
    }

    if( m_GseimSelectedBlock >= 0 )
    {
        CommitGseimControls( m_GseimSelectedBlock );
        ApplySolveTypePolicy( m_GseimSolveBlocks[m_GseimSelectedBlock] );
        PopulateGseimControls( m_GseimSelectedBlock );
        RefreshGseimBlockList();
        UpdateGseimControls();
    }
    event.Skip();
}

void DIALOG_EXPORT_NETLIST::RefreshGseimBlockList()
{
    m_GseimUpdating = true;

    EXPORT_NETLIST_PAGE* pg = m_PanelNetType[PANELGSEIM];

    wxChoice* choice = pg->m_GseimBlockChoiceCtrl;
    choice->Clear();

    for( size_t i = 0; i < m_GseimSolveBlocks.size(); ++i )
        choice->Append( m_GseimSolveBlocks[i].solveType.Upper() );

    if( m_GseimSelectedBlock >= 0 && m_GseimSelectedBlock < (int)m_GseimSolveBlocks.size() )
    {
        choice->SetSelection( m_GseimSelectedBlock );
    }
    else
    {
        choice->SetSelection( wxNOT_FOUND );
    }

    wxGrid* grid = pg->m_GseimBlockGrid;

    if( grid->GetNumberRows() > 0 )
        grid->DeleteRows( 0, grid->GetNumberRows() );

    if( !m_GseimSolveBlocks.empty() )
        grid->AppendRows( m_GseimSolveBlocks.size() );

    for( size_t i = 0; i < m_GseimSolveBlocks.size(); ++i )
    {
        grid->SetCellValue(
            i, 0,
            wxString::Format( "%d", int( i + 1 ) ) );

        grid->SetCellValue(
            i, 1,
            m_GseimSolveBlocks[i].solveType.Upper() );
    }

    pg->m_GseimRemoveBlockBtn->Enable(
        m_GseimSolveBlocks.size() > 0 );
    
    RefreshGseimOutputList();
    m_GseimUpdating = false;
    UpdateGseimBlockEditor();
}

void DIALOG_EXPORT_NETLIST::OnGseimBlockSelected( wxCommandEvent& event )
{
    if( m_GseimUpdating )
        return;

    int newSel = event.GetSelection();

    if( newSel == wxNOT_FOUND )
        return;

    if( m_GseimSelectedBlock >= 0 &&
        m_GseimSelectedBlock < (int)m_GseimSolveBlocks.size() )
    {
        CommitGseimControls( m_GseimSelectedBlock );
    }

    m_GseimSelectedBlock = newSel;
    PopulateGseimControls( m_GseimSelectedBlock );

    UpdateGseimBlockEditor();
}

void DIALOG_EXPORT_NETLIST::OnGseimAddBlock( wxCommandEvent& event )
{
    // Commit current first
    if( m_GseimSelectedBlock >= 0 )
        CommitGseimControls( m_GseimSelectedBlock );

    m_GseimSolveBlocks.emplace_back();

    ApplySolveTypePolicy( m_GseimSolveBlocks.back() );   // default block

    m_GseimSelectedBlock = (int)m_GseimSolveBlocks.size() - 1;

    RefreshGseimBlockList();
    PopulateGseimControls( m_GseimSelectedBlock );
    PopulateGseimOutvars( );  
    UpdateGseimBlockEditor();
}

void DIALOG_EXPORT_NETLIST::OnGseimRemoveBlock( wxCommandEvent& event )
{
    if( m_GseimSolveBlocks.empty() )
        return;

    m_GseimSolveBlocks.erase( m_GseimSolveBlocks.begin() + m_GseimSelectedBlock );

    if( m_GseimSolveBlocks.empty() )
    {
        m_GseimSelectedBlock = -1;
    }
    else if( m_GseimSelectedBlock >= (int)m_GseimSolveBlocks.size() )
    {
        m_GseimSelectedBlock = (int)m_GseimSolveBlocks.size() - 1;
    }

    RefreshGseimBlockList();

    if( m_GseimSelectedBlock >= 0 )
        PopulateGseimControls( m_GseimSelectedBlock );

    PopulateGseimOutvars( );

    m_editFrame->Schematic().SetGseimSolveBlocks( m_GseimSolveBlocks );
    UpdateGseimBlockEditor();
}

void DIALOG_EXPORT_NETLIST::OnGseimSolveTypeChanged( wxCommandEvent& event )
{
    if( m_GseimSelectedBlock < 0 )
        return;

    CommitGseimControls( m_GseimSelectedBlock );

    ApplySolveTypePolicy(
        m_GseimSolveBlocks[m_GseimSelectedBlock] );

    PopulateGseimControls(
        m_GseimSelectedBlock );
}


void DIALOG_EXPORT_NETLIST::UpdateGseimBlockEditor()
{
    EXPORT_NETLIST_PAGE* pg = m_PanelNetType[PANELGSEIM];

    bool enabled =
        m_GseimSelectedBlock >= 0 &&
        m_GseimSelectedBlock < (int)m_GseimSolveBlocks.size();

    pg->m_GseimSolveTypeCtrl->Enable( enabled );
    pg->m_GseimInitialSolCtrl->Enable( enabled );
    pg->m_GseimOutputFileCtrl->Enable( enabled );
    pg->m_GseimParametersGrid->Enable( enabled );
}

void DIALOG_EXPORT_NETLIST::UpdateGseimControls()
{
    EXPORT_NETLIST_PAGE* pg = m_PanelNetType[PANELGSEIM];

    if( !pg )
        return;

    

    wxString solveType =
        pg->m_GseimSolveTypeCtrl->GetStringSelection();

    bool isAc   = ( solveType == "ac" );
    bool isTrns = ( solveType == "trns" );
    bool isDc   = ( solveType == "dc" );
    bool isSss   = ( solveType == "sss" );

    bool hasOutput = isTrns || isDc || isAc || isSss;

    pg->m_GseimOutputLabel->Show( hasOutput );
    pg->m_GseimOutputFileCtrl->Show( hasOutput );

    pg->m_GseimOutvarsLabel->Show( hasOutput );
    pg->m_GseimOutvarsGrid->Show( hasOutput );

    if( pg->m_GseimFilterBySelectionCtrl )
        pg->m_GseimFilterBySelectionCtrl->Show( hasOutput );

    if( pg->m_GseimRefreshSelectionBtn )
        pg->m_GseimRefreshSelectionBtn->Show( hasOutput );

    if( pg->m_GseimSelectionStatusLabel )
        pg->m_GseimSelectionStatusLabel->Show( hasOutput );

    if( pg->m_GseimAddParameterBtn )
        pg->m_GseimAddParameterBtn->Show( !isAc );

    if( pg->m_GseimParametersGrid )
        pg->m_GseimParametersGrid->Show( true );

    pg->m_RightOptionsBoxSizer->Layout();
    pg->m_RightBoxSizer->Layout();
    pg->m_LowBoxSizer->Layout();

    pg->Layout();

    m_NoteBook->Layout();

    Layout();


    // pg->Layout();
    // pg->GetSizer()->Layout();
}

void DIALOG_EXPORT_NETLIST::InstallPageSpiceModel()
{
    auto* pg = new EXPORT_NETLIST_PAGE( m_NoteBook, wxT( "SPICE Model" ), NET_TYPE_SPICE_MODEL, false );

    wxStaticText* label = new wxStaticText( pg, wxID_ANY, _( "Export netlist as a SPICE .subckt model" ) );
    pg->m_LeftBoxSizer->Add( label, 0, wxBOTTOM, 10 );

    pg->m_CurSheetAsRoot = new wxCheckBox( pg, ID_CUR_SHEET_AS_ROOT, _( "Use current sheet as root" ) );
    pg->m_CurSheetAsRoot->SetToolTip( _( "Export netlist only for the current sheet" ) );
    pg->m_LeftBoxSizer->Add( pg->m_CurSheetAsRoot, 0, wxEXPAND | wxBOTTOM | wxRIGHT, 5 );

    m_PanelNetType[PANELSPICEMODEL] = pg;
}


void DIALOG_EXPORT_NETLIST::InstallCustomPages()
{
    EXPORT_NETLIST_PAGE* currPage;
    EESCHEMA_SETTINGS*   cfg = dynamic_cast<EESCHEMA_SETTINGS*>( Kiface().KifaceSettings() );
    wxCHECK( cfg, /* void */ );

    for( size_t i = 0; i < CUSTOMPANEL_COUNTMAX && i < cfg->m_NetlistPanel.plugins.size(); i++ )
    {
        // pairs of (title, command) are stored
        currPage = AddOneCustomPage( cfg->m_NetlistPanel.plugins[i].name,
                                     cfg->m_NetlistPanel.plugins[i].command,
                                     static_cast<NETLIST_TYPE_ID>( NET_TYPE_CUSTOM1 + i ) );

        m_PanelNetType[PANELCUSTOMBASE + i] = currPage;
    }
}


EXPORT_NETLIST_PAGE* DIALOG_EXPORT_NETLIST::AddOneCustomPage( const wxString& aTitle,
                                                              const wxString& aCommandString,
                                                              NETLIST_TYPE_ID aNetTypeId )
{
    EXPORT_NETLIST_PAGE* pg = new EXPORT_NETLIST_PAGE( m_NoteBook, aTitle, aNetTypeId, true );

    pg->m_LowBoxSizer->Add( new wxStaticText( pg, wxID_ANY, _( "Title:" ) ), 0,
                            wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5 );

    pg->m_LowBoxSizer->AddSpacer( 2 );

    pg->m_TitleStringCtrl = new wxTextCtrl( pg, wxID_ANY, aTitle );

    pg->m_TitleStringCtrl->SetInsertionPoint( 1 );
    pg->m_LowBoxSizer->Add( pg->m_TitleStringCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT, 5 );

    pg->m_LowBoxSizer->AddSpacer( 10 );

    pg->m_LowBoxSizer->Add( new wxStaticText( pg, wxID_ANY, _( "Netlist command:" ) ), 0,
                            wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5 );

    pg->m_LowBoxSizer->AddSpacer( 2 );

    pg->m_CommandStringCtrl = new wxTextCtrl( pg, wxID_ANY, aCommandString );

    pg->m_CommandStringCtrl->SetInsertionPoint( 1 );
    pg->m_LowBoxSizer->Add( pg->m_CommandStringCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT, 5 );

    return pg;
}


void DIALOG_EXPORT_NETLIST::OnNetlistTypeSelection( wxNotebookEvent& event )
{
    updateGeneratorButtons();
}


bool DIALOG_EXPORT_NETLIST::TransferDataFromWindow()
{
    wxFileName  fn;
    wxString    fileWildcard;
    wxString    fileExt;
    wxString    title = _( "Save Netlist File" );

    if( m_job )
    {
        for( const auto& [format, name] : JOB_EXPORT_SCH_NETLIST::GetFormatNameMap() )
        {
            if( name == m_PanelNetType[m_NoteBook->GetSelection()]->GetPageNetFmtName() )
            {
                m_job->format = format;
                break;
            }
        }

        m_job->SetConfiguredOutputPath( m_outputPath->GetValue() );
        m_job->m_spiceSaveAllVoltages = m_PanelNetType[ PANELSPICE ]->m_SaveAllVoltages->IsChecked();
        m_job->m_spiceSaveAllCurrents = m_PanelNetType[ PANELSPICE ]->m_SaveAllCurrents->IsChecked();
        m_job->m_spiceSaveAllDissipations = m_PanelNetType[ PANELSPICE ]->m_SaveAllDissipations->IsChecked();
        m_job->m_spiceSaveAllEvents = m_PanelNetType[ PANELSPICE ]->m_SaveAllEvents->IsChecked();

        return true;
    }

    EXPORT_NETLIST_PAGE* currPage;
    currPage = (EXPORT_NETLIST_PAGE*) m_NoteBook->GetCurrentPage();

    bool     runExternalSpiceCommand = false;
    unsigned netlist_opt = 0;

    // Calculate the netlist filename
    fn = m_editFrame->Schematic().GetFileName();
    FilenamePrms( currPage->m_IdNetType, &fileExt, &fileWildcard );

    // Set some parameters
    switch( currPage->m_IdNetType )
    {
    case NET_TYPE_SPICE:
        // Set spice netlist options:
        netlist_opt |= NETLIST_EXPORTER_SPICE::OPTION_SIM_COMMAND;

        if( currPage->m_SaveAllVoltages->GetValue() )
            netlist_opt |= NETLIST_EXPORTER_SPICE::OPTION_SAVE_ALL_VOLTAGES;

        if( currPage->m_SaveAllCurrents->GetValue() )
            netlist_opt |= NETLIST_EXPORTER_SPICE::OPTION_SAVE_ALL_CURRENTS;

        if( currPage->m_SaveAllDissipations->GetValue() )
            netlist_opt |= NETLIST_EXPORTER_SPICE::OPTION_SAVE_ALL_DISSIPATIONS;

        if( currPage->m_SaveAllEvents->GetValue() )
            netlist_opt |= NETLIST_EXPORTER_SPICE::OPTION_SAVE_ALL_EVENTS;

        if( currPage->m_CurSheetAsRoot->GetValue() )
            netlist_opt |= NETLIST_EXPORTER_SPICE::OPTION_CUR_SHEET_AS_ROOT;

        runExternalSpiceCommand = currPage->m_RunExternalSpiceCommand->GetValue();
        break;

    case NET_TYPE_GSEIM:
    {
        EXPORT_NETLIST_PAGE* gseimPg = m_PanelNetType[PANELGSEIM];

        if( m_GseimSelectedBlock >= 0 )
            CommitGseimControls( m_GseimSelectedBlock );

        m_editFrame->Schematic().SetGseimTitle( gseimPg->m_GseimTitleCtrl->GetValue() );

        std::unordered_map<wxString, wxString> origToRenamed;

        if( gseimPg && gseimPg->m_GseimOutvarsGrid )
        {
            wxGrid* ovGrid = gseimPg->m_GseimOutvarsGrid;

            for( int row = 0; row < ovGrid->GetNumberRows(); ++row )
            {
                wxString origName = ovGrid->GetCellValue( row, 3 );
                wxString curName  = ovGrid->GetCellValue( row, 1 );

                if( !origName.IsEmpty() )
                    origToRenamed[origName] = curName;
            }
        }

        wxString solveText;

        for( const auto& block : m_GseimSolveBlocks )
        {
            solveText += "begin_solve\n";
            solveText += "   solve_type=" + block.solveType + "\n";
            solveText += "   initial_sol=" + block.initialSol + "\n";

            if( block.solveType == "ac" )
            {
                solveText += "   set_freq val=" + block.parameters.at( "set_freq" ) + "\n";
            }
            else
            {
                for( const auto& [key, value] : block.parameters )
                    solveText += "   method: " + key + "=" + value + "\n";
            }

            solveText += "\n";

            for( const auto& output : block.outputs )
            {
                solveText += "   begin_output\n";
                solveText += "      filename=" + output.outputFile + "\n";

                if( !output.outputVars.empty() )
                {
                    solveText += "      variables:\n";

                    for( const auto& var : output.outputVars )
                    {
                        wxString display = var;
                        auto it = origToRenamed.find( var );

                        if( it != origToRenamed.end() && !it->second.IsEmpty() )
                            display = it->second;

                        solveText += "+         " + display + "\n";
                    }
                }

                solveText += "   end_output\n";
            }

            solveText += "end_solve\n\n";
        }

        m_editFrame->Schematic().SetGseimSolveBlock( solveText );

        std::vector<GSEIM_OUTVAR> explicitOutvars;

        if( gseimPg && gseimPg->m_GseimOutvarsGrid )
        {
            wxGrid* ovGrid = gseimPg->m_GseimOutvarsGrid;

            ovGrid->SaveEditControlValue();
            ovGrid->DisableCellEditControl();

            bool restrictSelection =
                gseimPg->m_GseimFilterBySelectionCtrl &&
                gseimPg->m_GseimFilterBySelectionCtrl->IsChecked();

            for( int row = 0; row < ovGrid->GetNumberRows(); ++row )
            {
                if( restrictSelection &&
                    ovGrid->GetCellValue( row, 0 ) != "1" )
                {
                    continue;
                }

                wxString origName = ovGrid->GetCellValue( row, 3 );

                auto it = std::find_if(
                    m_GseimAllOutvars.begin(),
                    m_GseimAllOutvars.end(),
                    [&]( const GSEIM_OUTVAR& x )
                    {
                        return x.name == origName;
                    } );

                if( it == m_GseimAllOutvars.end() )
                    continue;

                GSEIM_OUTVAR ov = *it;

                ov.name = ovGrid->GetCellValue( row, 1 );

                if( ov.name.Contains( "=" ) )
                {
                    // XBE output: "y=x2" -> user variable is "x2"
                    ov.name = ov.name.AfterFirst( '=' );
                    ov.expr = "xvar_of_" + ov.name;
                }
                else
                {
                    ov.expr = ovGrid->GetCellValue( row, 2 );
                }

                explicitOutvars.push_back( ov );

            }
            m_editFrame->Schematic().SetGseimExplicitOutvars( explicitOutvars );
        }
        m_editFrame->Schematic().SetGseimGlobalRparms( gseimPg->m_GseimGlobalRparmsCtrl->GetValue() );
        m_editFrame->Schematic().SetGseimGlobalIparms( gseimPg->m_GseimGlobalIparmsCtrl->GetValue() );
        m_editFrame->Schematic().SetGseimGlobalSparms( gseimPg->m_GseimGlobalSparmsCtrl->GetValue() );
        m_editFrame->Schematic().SetGseimGparmCCode( gseimPg->m_GseimGlobalCCodeCtrl->GetValue() );
        break;
    }

    case NET_TYPE_GSEIM_SUBCKT:
    {
        EXPORT_NETLIST_PAGE* gseimPg = m_PanelNetType[PANELGSEIMSUBCKT];

        std::vector<GSEIM_OUTVAR> explicitOutvars;

        if( gseimPg && gseimPg->m_GseimOutvarsGrid )
        {
            wxGrid* ovGrid = gseimPg->m_GseimOutvarsGrid;

            ovGrid->SaveEditControlValue();
            ovGrid->DisableCellEditControl();

            bool restrictSelection = gseimPg->m_GseimFilterBySelectionCtrl && gseimPg->m_GseimFilterBySelectionCtrl->IsChecked();

            for( int row = 0; row < ovGrid->GetNumberRows(); ++row )
            {
                if( restrictSelection &&
                    ovGrid->GetCellValue( row, 0 ) != "1" )
                {
                    continue;
                }
                GSEIM_OUTVAR ov;

                ov.name = ovGrid->GetCellValue( row, 1 );
                ov.expr = ovGrid->GetCellValue( row, 2 );

                explicitOutvars.push_back( ov );
            }
        }

        m_editFrame->Schematic().SetGseimExplicitOutvars( explicitOutvars );

        auto ReadParameterGrid =
            [this]( wxGrid* aGrid,
                    std::map<wxString, wxString>& aMap,
                    const wxString& aType ) -> bool
        {
            if( !aGrid )
                return true;

            aGrid->SaveEditControlValue();
            aGrid->DisableCellEditControl();

            for( int row = 0; row < aGrid->GetNumberRows(); ++row )
            {
                wxString name  = aGrid->GetCellValue( row, 0 );
                wxString value = aGrid->GetCellValue( row, 1 );

                value.Trim( true ).Trim( false );

                if( value.IsEmpty() )
                {
                    wxMessageBox(
                        wxString::Format(
                            _( "Please enter a default value for %s '%s'." ),
                            aType,
                            name ),
                        _( "Missing Parameter Value" ),
                        wxOK | wxICON_ERROR,
                        this );

                    return false;
                }

                aMap[name] = value;
            }

            return true;
        };

        std::map<wxString, wxString> rparms;
        std::map<wxString, wxString> iparms;
        std::map<wxString, wxString> sparms;

        if( gseimPg )
        {
            if( !ReadParameterGrid( gseimPg->m_GseimRparmsGrid, rparms, "rparm" ) )
                return false;

            if( !ReadParameterGrid( gseimPg->m_GseimIparmsGrid, iparms, "iparm" ) )
                return false;

            if( !ReadParameterGrid( gseimPg->m_GseimSparmsGrid, sparms, "sparm" ) )
                return false;

            if( gseimPg->m_GseimCCodeCtrl )
                m_editFrame->Schematic().SetGseimSubcktCCode(
                    gseimPg->m_GseimCCodeCtrl->GetValue() );
        }

        m_editFrame->Schematic().SetGseimSubcktRparmValues( rparms );
        m_editFrame->Schematic().SetGseimSubcktIparmValues( iparms );
        m_editFrame->Schematic().SetGseimSubcktSparmValues( sparms );
        m_editFrame->Schematic().SetGseimSubcktCCode( gseimPg->m_GseimCCodeCtrl->GetValue() );

        break;
    }

    case NET_TYPE_SPICE_MODEL:
        if( currPage->m_CurSheetAsRoot->GetValue() )
            netlist_opt |= NETLIST_EXPORTER_SPICE::OPTION_CUR_SHEET_AS_ROOT;

        break;

    case NET_TYPE_CADSTAR:
        break;

    case NET_TYPE_PCBNEW:
        break;

    case NET_TYPE_ORCADPCB2:
        break;

    case NET_TYPE_ALLEGRO:
        break;

    case NET_TYPE_PADS:
        break;

    default:    // custom, NET_TYPE_CUSTOM1 and greater
        title.Printf( _( "%s Export" ), currPage->m_TitleStringCtrl->GetValue() );
        break;
    }

    wxString fullpath;

    bool isSubcktExport = ( currPage->m_IdNetType == NET_TYPE_GSEIM_SUBCKT );

    if( isSubcktExport )
    {
        wxFileName subDir( GetGseimSubPath(), wxEmptyString );

        if( !subDir.DirExists() )
            subDir.Mkdir( wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

        wxString defaultName = wxFileName( m_editFrame->Schematic().GetFileName() ).GetName();

    wxFileDialog dlg( this, _( "Save Subcircuit" ), wxEmptyString, defaultName + ".sub",
        "GSEIM Subcircuits (*.sub)|*.sub", wxFD_SAVE | wxFD_OVERWRITE_PROMPT );

    if( dlg.ShowModal() == wxID_CANCEL )
        return false;

    fullpath = dlg.GetPath();
    fn.Assign( fullpath );
    wxString subcktName = fn.GetName();
    subcktName.Trim( true ).Trim( false );

    if( subcktName.IsEmpty() )
    {
        wxMessageBox( _( "Subcircuit name cannot be empty." ) );
        return false;
    }

    m_editFrame->Schematic().SetGseimSubcktName( subcktName );
    }
    else if( runExternalSpiceCommand )
    {
        fn.SetExt( FILEEXT::SpiceFileExtension );
        fullpath = fn.GetFullPath();
    }
    else
    {
        fn.SetExt( fileExt );

        if( fn.GetPath().IsEmpty() )
           fn.SetPath( wxPathOnly( Prj().GetProjectFullName() ) );

        wxString fullname = fn.GetFullName();
        wxString path     = fn.GetPath();

        // full name does not and should not include the path, per wx docs.
        wxFileDialog dlg( this, title, path, fullname, fileWildcard, wxFD_SAVE );

        KIPLATFORM::UI::AllowNetworkFileSystems( &dlg );

        if( dlg.ShowModal() == wxID_CANCEL )
            return false;

        fullpath = dlg.GetPath();   // directory + filename
    }

    m_editFrame->ClearMsgPanel();
    REPORTER& reporter = m_MessagesBox->Reporter();

    if( currPage->m_CommandStringCtrl )
        m_editFrame->SetNetListerCommand( currPage->m_CommandStringCtrl->GetValue() );
    else
        m_editFrame->SetNetListerCommand( wxEmptyString );

    if( !m_editFrame->ReadyToNetlist( _( "Exporting netlist requires a fully annotated schematic." ) ) )
        return false;

    m_editFrame->WriteNetListFile( currPage->m_IdNetType, fullpath, netlist_opt, &reporter );
    if( isSubcktExport )
    {
        wxFileName subDir( GetGseimSubPath(), wxEmptyString );

        if( !subDir.DirExists() )
            subDir.Mkdir( wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

        wxString internalPath = subDir.GetPathWithSep() + m_editFrame->Schematic().GetGseimSubcktName() + ".sub";

        if( fullpath != internalPath )
            wxCopyFile( fullpath, internalPath, true );
    }

    if( runExternalSpiceCommand )
    {
        // Build the command line
        wxString commandLine = m_PanelNetType[ PANELSPICE ]->m_CommandStringCtrl->GetValue();
        commandLine.Replace( wxS( "%I" ), fullpath, true );
        commandLine.Trim( true ).Trim( false );

        if( !commandLine.IsEmpty() )
        {
            std::vector<wxString> argsStrings = SplitCommandLine( commandLine );

            if( !argsStrings.empty() )
            {
                std::vector<const wxChar*> argv;
                argv.reserve( argsStrings.size() + 1 );

                for( wxString& arg : argsStrings )
                    argv.emplace_back( arg.wc_str() );

                argv.emplace_back( nullptr );

                wxExecuteEnv env;
                wxGetEnvMap( &env.env );

                const ENV_VAR_MAP& envVars = Pgm().GetLocalEnvVariables();

                for( const auto& [key, value] : envVars )
                {
                    if( !value.GetValue().IsEmpty() )
                        env.env[key] = value.GetValue();
                }

                wxFileName netlistFile( fullpath );

                if( !netlistFile.IsAbsolute() )
                    netlistFile.MakeAbsolute( Prj().GetProjectPath() );
                else
                    netlistFile.MakeAbsolute();

                wxString cwd = netlistFile.GetPath();

                if( cwd.IsEmpty() )
                    cwd = Prj().GetProjectPath();

                env.cwd = cwd;

                wxProcess* process = new wxProcess( GetEventHandler(), wxID_ANY );
                process->Redirect();

                long pid = wxExecute( argv.data(), wxEXEC_ASYNC, process, &env );

                reporter.ReportHead( commandLine, RPT_SEVERITY_ACTION );

                if( pid <= 0 )
                {
                    reporter.Report( _( "external simulator not found" ), RPT_SEVERITY_ERROR );
                    reporter.Report( _( "Note: command line is usually: "
                                        "<tt>&lt;path to SPICE binary&gt; \"%I\"</tt>" ),
                                     RPT_SEVERITY_INFO );
                    delete process;
                }
                else
                {
                    process->Activate();

                    std::this_thread::sleep_for( std::chrono::seconds( 1 ) ); // give the process time to start and output any data or errors

                    if( process->IsInputAvailable() )
                    {
                        wxInputStream* in = process->GetInputStream();
                        wxTextInputStream textstream( *in );

                        while( in->CanRead() )
                        {
                            wxString line = textstream.ReadLine();

                            if( !line.IsEmpty() )
                                reporter.Report( line, RPT_SEVERITY_INFO );
                        }
                    }

                    if( process->IsErrorAvailable() )
                    {
                        wxInputStream* err = process->GetErrorStream();
                        wxTextInputStream textstream( *err );

                        while( err->CanRead() )
                        {
                            wxString line = textstream.ReadLine();

                            if( !line.IsEmpty() )
                            {
                                if( line.EndsWith( wxS( "failed with error 2!" ) ) )      // ENOENT
                                {
                                    reporter.Report( _( "external simulator not found" ), RPT_SEVERITY_ERROR );
                                    reporter.Report( _( "Note: command line is usually: "
                                                        "<tt>&lt;path to SPICE binary&gt; \"%I\"</tt>" ),
                                                     RPT_SEVERITY_INFO );
                                }
                                else if( line.EndsWith( wxS( "failed with error 8!" ) ) ) // ENOEXEC
                                {
                                    reporter.Report( _( "external simulator has the wrong format or "
                                                        "architecture" ), RPT_SEVERITY_ERROR );
                                }
                                else if( line.EndsWith( "failed with error 13!" ) ) // EACCES
                                {
                                    reporter.Report( _( "permission denied" ), RPT_SEVERITY_ERROR );
                                }
                                else
                                {
                                    reporter.Report( line, RPT_SEVERITY_ERROR );
                                }
                            }
                        }
                    }

                    process->CloseOutput();
                    process->Detach();

                    // Do not delete process, it will delete itself when it terminates
                }
            }
        }
    }

    WriteCurrentNetlistSetup();

    return !runExternalSpiceCommand;
}


bool DIALOG_EXPORT_NETLIST::FilenamePrms( NETLIST_TYPE_ID aType, wxString * aExt, wxString * aWildCard )
{
    wxString fileExt;
    wxString fileWildcard;
    bool     ret = true;

    switch( aType )
    {
    case NET_TYPE_SPICE:
        fileExt = FILEEXT::SpiceFileExtension;
        fileWildcard = FILEEXT::SpiceNetlistFileWildcard();
        break;

    case NET_TYPE_GSEIM:
        fileExt = "cir";
        fileWildcard = "GSEIM Netlist (*.cir)|*.cir";
        break;

    case NET_TYPE_CADSTAR:
        fileExt = FILEEXT::CadstarNetlistFileExtension;
        fileWildcard = FILEEXT::CadstarNetlistFileWildcard();
        break;

    case NET_TYPE_ORCADPCB2:
        fileExt = FILEEXT::OrCadPcb2NetlistFileExtension;
        fileWildcard = FILEEXT::OrCadPcb2NetlistFileWildcard();
        break;

    case NET_TYPE_PCBNEW:
        fileExt = FILEEXT::NetlistFileExtension;
        fileWildcard = FILEEXT::NetlistFileWildcard();
        break;

    case NET_TYPE_ALLEGRO:
        fileExt = FILEEXT::AllegroNetlistFileExtension;
        fileWildcard = FILEEXT::AllegroNetlistFileWildcard();
        break;

    case NET_TYPE_PADS:
        fileExt = FILEEXT::PADSNetlistFileExtension;
        fileWildcard = FILEEXT::PADSNetlistFileWildcard();
        break;


    default:    // custom, NET_TYPE_CUSTOM1 and greater
        fileWildcard = FILEEXT::AllFilesWildcard();
        ret = false;
    }

    if( aExt )
        *aExt = fileExt;

    if( aWildCard )
        *aWildCard = fileWildcard;

    return ret;
}


void DIALOG_EXPORT_NETLIST::WriteCurrentNetlistSetup()
{
    EESCHEMA_SETTINGS* cfg = dynamic_cast<EESCHEMA_SETTINGS*>( Kiface().KifaceSettings() );
    wxCHECK( cfg, /* void */ );

    cfg->m_NetlistPanel.plugins.clear();

    // Update existing custom pages
    for( int ii = PANELCUSTOMBASE; ii < PANELCUSTOMBASE + CUSTOMPANEL_COUNTMAX; ++ii )
    {
        if( EXPORT_NETLIST_PAGE* currPage = m_PanelNetType[ii] )
        {
            wxString title = currPage->m_TitleStringCtrl->GetValue();
            wxString command = currPage->m_CommandStringCtrl->GetValue();

            if( title.IsEmpty() || command.IsEmpty() )
                continue;

            cfg->m_NetlistPanel.plugins.emplace_back( title, wxEmptyString );
            cfg->m_NetlistPanel.plugins.back().command = command;
        }
    }
}


void DIALOG_EXPORT_NETLIST::OnDelGenerator( wxCommandEvent& event )
{
    EXPORT_NETLIST_PAGE* currPage = (EXPORT_NETLIST_PAGE*) m_NoteBook->GetCurrentPage();

    if( !currPage->IsCustom() )
        return;

    currPage->m_CommandStringCtrl->SetValue( wxEmptyString );
    currPage->m_TitleStringCtrl->SetValue( wxEmptyString );

    WriteCurrentNetlistSetup();

    if( IsQuasiModal() )
        EndQuasiModal( NET_PLUGIN_CHANGE );
    else
        EndDialog( NET_PLUGIN_CHANGE );
}


void DIALOG_EXPORT_NETLIST::OnAddGenerator( wxCommandEvent& event )
{
    NETLIST_DIALOG_ADD_GENERATOR dlg( this );

    if( dlg.ShowModal() != wxID_OK )
        return;

    wxString title = dlg.GetGeneratorTitle();
    wxString cmd = dlg.GetGeneratorTCommandLine();

    // Verify it does not exists
    for( int ii = PANELCUSTOMBASE; ii < PANELCUSTOMBASE + CUSTOMPANEL_COUNTMAX; ++ii )
    {
        if( m_PanelNetType[ii] && m_PanelNetType[ii]->GetPageNetFmtName() == title )
        {
            wxMessageBox( _( "This plugin already exists." ) );
            return;
        }
    }

    // Find the first empty slot
    int netTypeId = PANELCUSTOMBASE;

    while( m_PanelNetType[netTypeId] )
    {
        netTypeId++;

        if( netTypeId == PANELCUSTOMBASE + CUSTOMPANEL_COUNTMAX )
        {
            wxMessageBox( _( "Maximum number of plugins already added to dialog." ) );
            return;
        }
    }

    m_PanelNetType[netTypeId] = AddOneCustomPage( title, cmd, (NETLIST_TYPE_ID)netTypeId );

    WriteCurrentNetlistSetup();

    if( IsQuasiModal() )
        EndQuasiModal( NET_PLUGIN_CHANGE );
    else
        EndDialog( NET_PLUGIN_CHANGE );
}


NETLIST_DIALOG_ADD_GENERATOR::NETLIST_DIALOG_ADD_GENERATOR( DIALOG_EXPORT_NETLIST* parent ) :
    NETLIST_DIALOG_ADD_GENERATOR_BASE( parent )
{
    m_Parent = parent;
    m_initialFocusTarget = m_textCtrlName;

    SetupStandardButtons();
    finishDialogSettings();
}


bool NETLIST_DIALOG_ADD_GENERATOR::TransferDataFromWindow()
{
    if( !wxDialog::TransferDataFromWindow() )
        return false;

    if( m_textCtrlName->GetValue() == wxEmptyString )
    {
        wxMessageBox( _( "You must provide a netlist generator title" ) );
        return false;
    }

    return true;
}


void NETLIST_DIALOG_ADD_GENERATOR::OnBrowseGenerators( wxCommandEvent& event )
{
    wxString FullFileName, Path;

#ifndef __WXMAC__
    Path = Pgm().GetExecutablePath();
#else
    Path = PATHS::GetOSXKicadDataDir() + wxT( "/plugins" );
#endif

    FullFileName = wxFileSelector( _( "Generator File" ), Path, FullFileName, wxEmptyString,
                                   wxFileSelectorDefaultWildcardStr, wxFD_OPEN, this );

    if( FullFileName.IsEmpty() )
        return;

    // Creates a default command line, suitable for external tool xslproc or python, based on
    // the plugin extension ("xsl" or "exe" or "py")
    wxString cmdLine;
    wxFileName fn( FullFileName );
    wxString ext = fn.GetExt();

    if( ext == wxT( "xsl" ) )
        cmdLine.Printf( wxT( "xsltproc -o \"%%O\" \"%s\" \"%%I\"" ), FullFileName );
    else if( ext == wxT( "exe" ) || ext.IsEmpty() )
        cmdLine.Printf( wxT( "\"%s\" > \"%%O\" < \"%%I\"" ), FullFileName );
    else if( ext == wxT( "py" ) || ext.IsEmpty() )
        cmdLine.Printf( wxT( "python \"%s\" \"%%I\" \"%%O\"" ), FullFileName );
    else
        cmdLine.Printf( wxT( "\"%s\"" ), FullFileName );

    m_textCtrlCommand->SetValue( cmdLine );

    // We need a title for this panel
    // Propose a default value if empty ( i.e. the short filename of the script)
    if( m_textCtrlName->GetValue().IsEmpty() )
        m_textCtrlName->SetValue( fn.GetName() );
}


void DIALOG_EXPORT_NETLIST::updateGeneratorButtons()
{
    EXPORT_NETLIST_PAGE* currPage = (EXPORT_NETLIST_PAGE*) m_NoteBook->GetCurrentPage();

    if( currPage == nullptr )
        return;

    m_buttonDelGenerator->Enable( currPage->IsCustom() );
}


// int InvokeDialogNetList( SCH_EDIT_FRAME* aCaller )
// {
//     DIALOG_EXPORT_NETLIST dlg( aCaller );

//     int ret = dlg.ShowModal();
//     aCaller->SaveProjectLocalSettings();

//     return ret;
// }

int InvokeDialogNetList( SCH_EDIT_FRAME* aCaller )
{
    DIALOG_EXPORT_NETLIST dlg( aCaller );
    int ret = dlg.ShowQuasiModal();   // was ShowModal()
    aCaller->SaveProjectLocalSettings();
    return ret;
}