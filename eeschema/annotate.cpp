/**
 * @file annotate.cpp
 * @brief Component annotation.
 */

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2004-2017 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <algorithm>
#include <boost/property_tree/ptree.hpp>
#include <class_library.h>
#include <confirm.h>
#include <fctsys.h>
#include <ptree.h>
#include <reporter.h>
#include <sch_draw_panel.h>
#include <sch_edit_frame.h>
#include <sch_reference_list.h>

void mapExistingAnnotation( std::map<timestamp_t, wxString>& aMap )
{
    SCH_SHEET_LIST     sheets( g_RootSheet );
    SCH_REFERENCE_LIST references;

    sheets.GetComponents( references );

    for( size_t i = 0; i < references.GetCount(); i++ )
    {
        SCH_COMPONENT* comp = references[ i ].GetComp();
        wxString       ref = comp->GetField( REFERENCE )->GetFullyQualifiedText();

        if( !ref.Contains( wxT( "?" ) ) )
            aMap[ comp->GetTimeStamp() ] = ref;
    }
}


void SCH_EDIT_FRAME::DeleteAnnotation( bool aCurrentSheetOnly )
{
    if( aCurrentSheetOnly )
    {
        SCH_SCREEN* screen = GetScreen();
        wxCHECK_RET( screen != NULL, wxT( "Attempt to clear annotation of a NULL screen." ) );
        screen->ClearAnnotation( g_CurrentSheet );
    }
    else
    {
        SCH_SCREENS ScreenList;
        ScreenList.ClearAnnotation();
    }

    // Update the references for the sheet that is currently being displayed.
    g_CurrentSheet->UpdateAllScreenReferences();

    SyncView();
    GetCanvas()->Refresh();
    OnModify();
}

// used locally in back-annotation
struct PCB_MODULE_DATA
{
    wxString ref;
    wxString value;
};

using PCB_MODULES      = std::map<wxString, PCB_MODULE_DATA>;
using PCB_MODULES_ITEM = std::pair<wxString, PCB_MODULE_DATA>;
using CHANGELIST_ITEM  = std::pair<SCH_REFERENCE, PCB_MODULE_DATA>;
using CHANGELIST       = std::deque<CHANGELIST_ITEM>;

int getPcbModulesFromCPTREE( const CPTREE& tree, REPORTER& aReporter, PCB_MODULES& aModules )
{
    int errors = 0;
    aModules.clear();
    for( auto& item : tree )
    {
        wxString path, value;
        wxASSERT( item.first == "ref" );
        wxString ref = (UTF8&) item.second.front().first;
        try
        {
            path = (UTF8&) item.second.get_child( "timestamp" ).front().first;
            value = (UTF8&) item.second.get_child( "value" ).front().first;
        }
        catch( boost::property_tree::ptree_bad_path& e )
        {
            wxASSERT_MSG( true, "Cannot parse PCB netlist for back-annotation" );
        }

        auto nearestItem = aModules.lower_bound( path );
        if( nearestItem != aModules.end() && nearestItem->first == path )
        {
            wxString msg;
            msg.Printf( _( "Pcb footprints %s and %s linked to same component" ),
                    nearestItem->second.ref, ref );
            aReporter.ReportHead( msg, REPORTER::RPT_ERROR );
            ++errors;
        }
        else
        {
            PCB_MODULE_DATA data{ ref, value };
            aModules.insert( nearestItem, PCB_MODULES_ITEM( path, data ) );
        }
    }
    return errors;
}

bool SCH_EDIT_FRAME::BackAnnotateComponents(
        const std::string& aNetlist, REPORTER& aReporter, bool aDryRun )
{
    wxString msg;
    //Read incoming std::string from KiwayMailIn to the property tree
    DSNLEXER lexer( aNetlist, FROM_UTF8( __func__ ) );
    PTREE    doc;
    Scan( &doc, &lexer );
    CPTREE&     back_anno = doc.get_child( "pcb_netlist" );
    PCB_MODULES pcbModules;
    int         errorsFound = getPcbModulesFromCPTREE( back_anno, aReporter, pcbModules );

    // Build the sheet list and get all used components
    SCH_SHEET_LIST     sheets( g_RootSheet );
    SCH_REFERENCE_LIST refs;
    sheets.GetComponents( refs );

    refs.SortByTimeStamp();
    errorsFound += refs.checkForDuplicatedElements( aReporter );

    // Get all multi units components, such as U1A, U1B, etc...
    SCH_MULTI_UNIT_REFERENCE_MAP schMultiunits;
    sheets.GetMultiUnitComponents( schMultiunits );
    // We will store here pcb components references, which has no representation in shematic
    std::deque<UTF8> pcbUnconnected;

    // Remove multi units and power symbols here
    for( size_t i = 0; i < refs.GetCount(); ++i )
    {
        if( refs[i].GetComp()->GetUnitCount() > 1
                or !refs[i].GetComp()->GetPartRef().lock()->IsNormal() )
            refs.RemoveItem( i-- );
    }

    // Find links between PCB footprints and schematic components. Once any link found,
    // component will be removed from corresponding list.
    CHANGELIST changeList;
    for( auto& module : pcbModules )
    {
        // Data retrieved from PCB
        const wxString& pcbPath = module.first;
        const wxString& pcbRef = module.second.ref;
        const wxString& pcbValue = module.second.value;
        bool            foundInMultiunit = false;

        for( SCH_MULTI_UNIT_REFERENCE_MAP::iterator part = schMultiunits.begin();
                part != schMultiunits.end(); ++part )
        {
            SCH_REFERENCE_LIST& partRefs = part->second;

            // If pcb unit found in multi unit components, process all units straight away
            if( partRefs.FindRefByPath( pcbPath ) >= 0 )
            {
                foundInMultiunit = true;
                for( size_t i = 0; i < partRefs.GetCount(); ++i )
                {
                    SCH_REFERENCE& schRef = partRefs[i];
                    if( schRef.GetRef() != pcbRef )
                        changeList.push_back( CHANGELIST_ITEM( schRef, module.second ) );
                }
                schMultiunits.erase( part );
                break;
            }
        }
        if( foundInMultiunit )
            // We deleted all multi units from references list, so we don't need to go further
            continue;

        // Process simple components
        int refIdx = refs.FindRefByPath( pcbPath );
        if( refIdx >= 0 )
        {
            SCH_REFERENCE& schRef = refs[refIdx];
            if( schRef.GetRef() != pcbRef )
                changeList.push_back( CHANGELIST_ITEM( schRef, module.second ) );
            refs.RemoveItem( refIdx );
        }
        else
        {
            // We haven't found link between footprint and common units or multi-units
            msg.Printf( _( "Cannot find component for %s footprint" ), pcbRef );
            ++errorsFound;
            aReporter.ReportTail( msg, REPORTER::RPT_ERROR );
        }
    }


    // Report
    for( size_t i = 0; i < refs.GetCount(); ++i )
    {
        msg.Printf( _( "Cannot find footprint for %s component" ), refs[i].GetRef() );
        aReporter.ReportTail( msg, REPORTER::RPT_ERROR );
        ++errorsFound;
    }

    for( auto& comp : schMultiunits )
    {
        auto& refList = comp.second;
        for( size_t i = 0; i < refList.GetCount(); ++i )
        {
            msg.Printf( _( "Cannot find footprint for %s%s component" ), refs[i].GetRef(),
                    LIB_PART::SubReference( refs[i].GetUnit(), false ) );
            aReporter.ReportTail( msg, REPORTER::RPT_ERROR );
        }
        ++errorsFound;
    }

    // Apply changes from change list
    for( auto& item : changeList )
    {
        SCH_REFERENCE&   ref = item.first;
        PCB_MODULE_DATA& module = item.second;
        if( ref.GetComp()->GetUnitCount() <= 1 )
            msg.Printf( _( "Change %s -> %s" ), ref.GetRef(), module.ref );
        else
        {
            wxString unit = LIB_PART::SubReference( ref.GetUnit() );
            msg.Printf( _( "Change %s%s -> %s%s" ), ref.GetRef(), unit, module.ref, unit );
        }
        aReporter.ReportHead( msg, aDryRun ? REPORTER::RPT_INFO : REPORTER::RPT_ACTION );
        if( !aDryRun )
            item.first.GetComp()->SetRef( &item.first.GetSheetPath(), item.second.ref );
    }

    // Report
    if( !errorsFound )
    {
        if( !aDryRun )
        {
            aReporter.ReportTail( _( "Schematic is back-annotated." ), REPORTER::RPT_ACTION );
            g_CurrentSheet->UpdateAllScreenReferences();
            SetSheetNumberAndCount();

            SyncView();
            OnModify();
            GetCanvas()->Refresh();
        }
        else
            aReporter.ReportTail(
                    _( "No errors  during dry run. Ready to go." ), REPORTER::RPT_ACTION );
    }
    else
    {
        msg.Printf( _( "Found %d errors. Fix them and run back annotation again." ), errorsFound );
        aReporter.ReportTail( msg, REPORTER::RPT_ERROR );
    }


    if( errorsFound )
        return false;
    else
        return true;
}


void SCH_EDIT_FRAME::AnnotateComponents( bool              aAnnotateSchematic,
                                         ANNOTATE_ORDER_T  aSortOption,
                                         ANNOTATE_OPTION_T aAlgoOption,
                                         int               aStartNumber,
                                         bool              aResetAnnotation,
                                         bool              aRepairTimestamps,
                                         bool              aLockUnits,
                                         REPORTER&         aReporter )
{
    SCH_REFERENCE_LIST references;

    SCH_SCREENS screens;

    // Build the sheet list.
    SCH_SHEET_LIST sheets( g_RootSheet );

    // Map of locked components
    SCH_MULTI_UNIT_REFERENCE_MAP lockedComponents;

    // Map of previous annotation for building info messages
    std::map<timestamp_t, wxString> previousAnnotation;

    // Test for and replace duplicate time stamps in components and sheets.  Duplicate
    // time stamps can happen with old schematics, schematic conversions, or manual
    // editing of files.
    if( aRepairTimestamps )
    {
        int count = screens.ReplaceDuplicateTimeStamps();

        if( count )
        {
            wxString msg;
            msg.Printf( _( "%d duplicate time stamps were found and replaced." ), count );
            aReporter.ReportTail( msg, REPORTER::RPT_WARNING );
        }
    }

    // If units must be locked, collect all the sets that must be annotated together.
    if( aLockUnits )
    {
        if( aAnnotateSchematic )
        {
            sheets.GetMultiUnitComponents( lockedComponents );
        }
        else
        {
            g_CurrentSheet->GetMultiUnitComponents( lockedComponents );
        }
    }

    // Store previous annotations for building info messages
    mapExistingAnnotation( previousAnnotation );

    // If it is an annotation for all the components, reset previous annotation.
    if( aResetAnnotation )
        DeleteAnnotation( !aAnnotateSchematic );

    // Set sheet number and number of sheets.
    SetSheetNumberAndCount();

    // Build component list
    if( aAnnotateSchematic )
    {
        sheets.GetComponents( references );
    }
    else
    {
        g_CurrentSheet->GetComponents( references );
    }

    // Break full components reference in name (prefix) and number:
    // example: IC1 become IC, and 1
    references.SplitReferences();

    switch( aSortOption )
    {
    default:
    case SORT_BY_X_POSITION:
        references.SortByXCoordinate();
        break;

    case SORT_BY_Y_POSITION:
        references.SortByYCoordinate();
        break;
    }

    bool useSheetNum = false;
    int idStep = 100;

    switch( aAlgoOption )
    {
    default:
    case INCREMENTAL_BY_REF:
        break;

    case SHEET_NUMBER_X_100:
        useSheetNum = true;
        break;

    case SHEET_NUMBER_X_1000:
        useSheetNum = true;
        idStep = 1000;
        break;
    }

    // Recalculate and update reference numbers in schematic
    references.Annotate( useSheetNum, idStep, aStartNumber, lockedComponents );
    references.UpdateAnnotation();

    for( size_t i = 0; i < references.GetCount(); i++ )
    {
        SCH_COMPONENT* comp = references[ i ].GetComp();
        wxString       prevRef = previousAnnotation[ comp->GetTimeStamp() ];
        wxString       newRef  = comp->GetField( REFERENCE )->GetFullyQualifiedText();
        wxString       msg;

        if( prevRef.Length() )
        {
            if( newRef == prevRef )
                continue;

            if( comp->GetUnitCount() > 1 )
                msg.Printf( _( "Updated %s (unit %s) from %s to %s" ),
                            GetChars( comp->GetField( VALUE )->GetShownText() ),
                            LIB_PART::SubReference( comp->GetUnit(), false ),
                            GetChars( prevRef ),
                            GetChars( newRef ) );
            else
                msg.Printf( _( "Updated %s from %s to %s" ),
                            GetChars( comp->GetField( VALUE )->GetShownText() ),
                            GetChars( prevRef ),
                            GetChars( newRef ) );
        }
        else
        {
            if( comp->GetUnitCount() > 1 )
                msg.Printf( _( "Annotated %s (unit %s) as %s" ),
                            GetChars( comp->GetField( VALUE )->GetShownText() ),
                            LIB_PART::SubReference( comp->GetUnit(), false ),
                            GetChars( newRef ) );
            else
                msg.Printf( _( "Annotated %s as %s" ),
                            GetChars( comp->GetField( VALUE )->GetShownText() ),
                            GetChars( newRef ) );
        }

        aReporter.Report( msg, REPORTER::RPT_ACTION );
    }

    // Final control (just in case ... ).
    if( !CheckAnnotate( aReporter, !aAnnotateSchematic ) )
        aReporter.ReportTail( _( "Annotation complete." ), REPORTER::RPT_ACTION );

    // Update on screen references, that can be modified by previous calculations:
    g_CurrentSheet->UpdateAllScreenReferences();
    SetSheetNumberAndCount();

    SyncView();
    GetCanvas()->Refresh();
    OnModify();
}


int SCH_EDIT_FRAME::CheckAnnotate( REPORTER& aReporter, bool aOneSheetOnly )
{
    // build the screen list
    SCH_SHEET_LIST      sheetList( g_RootSheet );
    SCH_REFERENCE_LIST  componentsList;

    // Build the list of components
    if( !aOneSheetOnly )
        sheetList.GetComponents( componentsList );
    else
        g_CurrentSheet->GetComponents( componentsList );

    return componentsList.CheckAnnotation( aReporter );
}
