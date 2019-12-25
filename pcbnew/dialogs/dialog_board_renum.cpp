/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019 Brian Piccioni brian@documenteddesigns.com
 * Copyright (C) 1992-2019 KiCad Developers, see AUTHORS.txt for contributors.
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

//
// To do
//

#include    <dialog_board_renum.h>				//
#include    <fstream>
#include    <mail_type.h>
#include "renum_type.h"


#define  RenumShowWarning( m )   {s_savedDialogParameters.RenumDialog->ShowWarning( m );}
#define  Swap( a, b ) { int t; t = a; a = b; b = t;}
//
struct DIALOG_BOARD_RENUM_PARAMETERS {
    DIALOG_BOARD_RENUM_PARAMETERS() :
            SortOnModules(true), RemoveFrontPrefix(false),
                RemoveBackPrefix(false), WriteLogFile(false), WriteChangeFile(false),

            FrontStartRefDes(1), BackStartRefDes(0), SortDir(0), SortGrid(1.0), RenumDialog(
                    NULL)

    {
    }
    bool SortOnModules;           //Sort on modules/ref des
    bool RemoveFrontPrefix;
    bool RemoveBackPrefix;
    bool WriteLogFile;
    bool WriteChangeFile;

    unsigned int FrontStartRefDes;        //The starting Front ref des;;
    unsigned int BackStartRefDes;         //The starting Back ref des
    int SortDir;                //The sort code (left to right, etc.)

    double SortGrid;                //The sort grid
    std::string FrontPrefix;             //The Front Prefix std::string
    std::string BackPrefix;              //The Back Prefix std::string
    std::string ExcludeList;              //The Back Prefix std::string
    DIALOG_BOARD_RENUM *RenumDialog;
};

static DIALOG_BOARD_RENUM_PARAMETERS s_savedDialogParameters;

//
// This converts the index into a sort code. Note that Back sort code will have left and right swapped.
//
int FrontDirectionsArray[8] = {
        SORTYFIRST + ASCENDINGFIRST + ASCENDINGSECOND, //"Top to bottom, left to right",  //  100
        SORTYFIRST + ASCENDINGFIRST + DESCENDINGSECOND, //"Top to bottom, right to left",  //  101
        SORTYFIRST + DESCENDINGFIRST + ASCENDINGSECOND, //"Back to Front, left to right",  //  110
        SORTYFIRST + DESCENDINGFIRST + DESCENDINGSECOND, //"Back to Front, right to left",  //  111
        SORTXFIRST + ASCENDINGFIRST + ASCENDINGSECOND, //"Left to right, Front to Back",  //  000
        SORTXFIRST + ASCENDINGFIRST + DESCENDINGSECOND, //"Left to right, Back to Front",  //  001
        SORTXFIRST + DESCENDINGFIRST + ASCENDINGSECOND, //"Right to left, Front to Back",  //  010
        SORTXFIRST + DESCENDINGFIRST + DESCENDINGSECOND //"Right to left, Back to Front",  //  011
};

//
// Back Left/Right is opposite because it is a mirror image (coordinates are from the top)
//
int BackDirectionsArray[8] = {
        SORTYFIRST + ASCENDINGFIRST + DESCENDINGSECOND, //"Top to bottom, left to right",  //  101
        SORTYFIRST + ASCENDINGFIRST + ASCENDINGSECOND, //"Top to bottom, right to left",  //  100
        SORTYFIRST + DESCENDINGFIRST + DESCENDINGSECOND, //"Bottom to top, left to right",  //  111
        SORTYFIRST + DESCENDINGFIRST + ASCENDINGSECOND, //"Bottom to top, right to left",  //  110
        SORTXFIRST + DESCENDINGFIRST + ASCENDINGSECOND, //"Left to right, top to bottom",  //  010
        SORTXFIRST + DESCENDINGFIRST + DESCENDINGSECOND, //"Left to right, bottom to top",  //  011
        SORTXFIRST + ASCENDINGFIRST + ASCENDINGSECOND, //"Right to left, top to bottom",  //  000
        SORTXFIRST + ASCENDINGFIRST + DESCENDINGSECOND //"Right to left, bottom to top",  //  001
};

bool SortYFirst;
bool DescendingFirst;
bool DescendingSecond;

wxString LogFile;
wxString ChangeFile;
std::vector<RefDesChange> ChangeArray;
std::vector<RefDesInfo> FrontModules;
std::vector<RefDesInfo> BackModules;
std::vector<RefDesTypeStr> RefDesTypes;
std::vector<std::string> ExcludeArray;

DIALOG_BOARD_RENUM::DIALOG_BOARD_RENUM(PCB_EDIT_FRAME* aParentFrame) :
        DIALOG_BOARD_RENUM_BASE(aParentFrame), m_modules(aParentFrame->GetBoard()->Modules()) {
    s_savedDialogParameters.RenumDialog = this;
    m_frame = aParentFrame;
    m_board = aParentFrame->GetBoard();
    m_modules = m_board->Modules();

    s_savedDialogParameters.RemoveFrontPrefix &= (0!= s_savedDialogParameters.FrontPrefix.size()); //Don't remove if nothing to remove
    s_savedDialogParameters.RemoveBackPrefix &= (0!= s_savedDialogParameters.BackPrefix.size());

    m_FrontRefDesStart->ChangeValue(
            std::to_string(s_savedDialogParameters.FrontStartRefDes));
    m_FrontPrefix->ChangeValue(s_savedDialogParameters.FrontPrefix);
    m_RemoveFrontPrefix->SetValue(
            0 == s_savedDialogParameters.RemoveFrontPrefix ? false : true);

    if (0 == s_savedDialogParameters.BackStartRefDes)
        m_BackRefDesStart->ChangeValue("");
    else
        m_BackRefDesStart->ChangeValue(std::to_string(s_savedDialogParameters.BackStartRefDes));

    m_BackPrefix->ChangeValue(s_savedDialogParameters.BackPrefix);
    m_RemoveBackPrefix->SetValue(0 == s_savedDialogParameters.RemoveBackPrefix ? false : true);
    m_SortGrid->ChangeValue(std::to_string(s_savedDialogParameters.SortGrid));
    m_WriteChangeFile->SetValue(0 == s_savedDialogParameters.WriteChangeFile ? false : true);
    m_WriteLogFile->SetValue(0 == s_savedDialogParameters.WriteLogFile ? false : true);
    m_SortOnModules->SetValue(s_savedDialogParameters.SortOnModules ? true : false);
    m_SortOnRefDes->SetValue(!s_savedDialogParameters.SortOnModules ? true : false);
    m_SortDir->SetSelection(s_savedDialogParameters.SortDir);
    m_ExcludeList->ChangeValue(s_savedDialogParameters.ExcludeList);
//
// Make labels for dialog to allow translation.
//
}

void DIALOG_BOARD_RENUM::OnRenumberClick(wxCommandEvent& event) {
    (void) event;

wxFileName filename;
NETLIST netlist;
STRING_FORMATTER stringformatter;

    if ( m_board->IsEmpty())
    {    ShowWarning( _( "\nNo Board!\n"));
        return;
    }

    GetParameters();
    LogFile.clear();                              //Clear the log file
    BuildModuleList(m_modules);

    for (auto mod : m_modules) {    // Create a "test" netlist to see it it will work
        netlist.AddComponent(
                    new COMPONENT(mod->GetFPID(), GetNewRefDes( mod->GetReference()), mod->GetValue(), mod->GetPath()));
    }
    netlist.Format("pcb_netlist", &stringformatter, 0, CTL_OMIT_FILTERS | CTL_OMIT_NETS | CTL_OMIT_FILTERS);
//
//write netlist back to payload (eeschema will receive that as payload is sent here as reference)
//
std::string payload = stringformatter.GetString();
bool    attemptrenum = m_frame->RenumberSchematic( payload, MAIL_RENUMBER );

    if(( false == attemptrenum ) || ( payload.size() < sizeof( RENUM_OK ) ))        //Didn't get a valid reply
            ShowWarning( _("\nRenumber failed!\n" ));
    else
    {
        LogMessage( payload );                           //If generating a log file, save the result
        if( 0 == payload.find( RENUM_OK ))
        {
            m_MessageWindow->AppendText( payload );         //Give the result
            for (auto mod : m_modules)
                mod->SetReference( GetNewRefDes( mod->GetReference()));     //Update the PCB reference
            m_MessageWindow->AppendText( _("\nPCB and schematic successfully renumbered\n" ));         //Give the result
        }
        else
            ShowWarning( payload );
    }

    m_frame->GetToolManager()->RunAction(ACTIONS::zoomFitScreen, true);
    m_frame->GetCanvas()->ForceRefresh( );            //is_changed
    m_frame->OnModify();                             //Need to save file on exist.

    if (s_savedDialogParameters.WriteLogFile)
        WriteRenumFile("_renumlog", LogFile ); //Write out the log file

    if (s_savedDialogParameters.WriteChangeFile)
        WriteRenumFile("_renumchange", ChangeFile ); //Write out the change file
}

void DIALOG_BOARD_RENUM::OKDone(wxCommandEvent& event) {
    (void) event;
    Close(true);
}

void DIALOG_BOARD_RENUM::ShowWarning(const wxString &aMessage) {
    m_MessageWindow->SetDefaultStyle(wxColour("RED"));
    m_MessageWindow->AppendText(aMessage);
    m_MessageWindow->SetDefaultStyle(wxColour("BLACK"));
}


DIALOG_BOARD_RENUM::~DIALOG_BOARD_RENUM() {
}

void DIALOG_BOARD_RENUM::GetParameters(void) {
    s_savedDialogParameters.SortDir = m_SortDir->GetSelection();
    s_savedDialogParameters.FrontStartRefDes = atoi(
            m_FrontRefDesStart->GetValue().c_str());
    s_savedDialogParameters.BackStartRefDes = atoi(
            m_BackRefDesStart->GetValue().c_str());
    s_savedDialogParameters.RemoveFrontPrefix = m_RemoveFrontPrefix->GetValue();
    s_savedDialogParameters.RemoveBackPrefix = m_RemoveBackPrefix->GetValue();
    s_savedDialogParameters.FrontPrefix = m_FrontPrefix->GetValue();
    s_savedDialogParameters.BackPrefix = m_BackPrefix->GetValue();
    s_savedDialogParameters.ExcludeList = m_ExcludeList->GetValue();

    s_savedDialogParameters.WriteChangeFile = m_WriteChangeFile->GetValue();
    s_savedDialogParameters.WriteLogFile = m_WriteLogFile->GetValue();
    s_savedDialogParameters.SortOnModules = m_SortOnModules->GetValue();

    if (!m_SortGrid->GetValue().ToDouble(&s_savedDialogParameters.SortGrid))
        s_savedDialogParameters.SortGrid = DEFAULT_GRID;
    s_savedDialogParameters.SortGrid =
            s_savedDialogParameters.SortGrid < MINGRID ?
                    MINGRID : s_savedDialogParameters.SortGrid;
}

//
// Used to compare ChangeArray element for sort
//
static bool ChangeArrayCompare(const RefDesChange &aA, const RefDesChange &aB) {
    return (aA.OldRefDesString < aB.OldRefDesString);
}

//
// Use std::sort() to sort modules. Because it is a structure a compare function is needed
// Returns true if the first coordinate should be before the second coordinate
//

static bool ModuleCompare(const RefDesInfo &aA, const RefDesInfo &aB) {
    int X0 = aA.x, X1 = aB.x, Y0 = aA.y, Y1 = aB.y;

    if (SortYFirst)  //If sorting by Y then X, swap X and Y
    {
        Swap( X0, Y0 );
        Swap( X1, Y1 );
    }

    if (DescendingFirst) Swap( X0, X1 ); //If descending, same compare just swap directions
    if (DescendingSecond) Swap( Y0, Y1);

    if (X0 < X1) return (true);               //yes, its smaller
    if (X0 > X1) return (false);              //No its not
    if (Y0 < Y1) return (true);               //same but equal
    return (false);
}

//
//Write the string to filename
//
void WriteRenumFile(const char *aFileType, wxString& aBuffer) {
wxFileName filename = s_savedDialogParameters.RenumDialog->m_board->GetFileName();

    filename.SetName(filename.GetName() + aFileType);
    filename.SetExt("txt");

std::string fullfilename = filename.GetFullPath().ToStdString();
std::ofstream tmphandle(fullfilename, std::ios::trunc); //Open the file for writing

    if (tmphandle.is_open())
        tmphandle << aBuffer;           //Write the buffer
    if (!tmphandle.is_open() || tmphandle.bad()) {           //Error?
        RenumShowWarning( _( "\n\nCan't write ") + fullfilename );
    }
    tmphandle.close();
}

void LogMessage(std::string &aMessage) {
    if (!s_savedDialogParameters.WriteLogFile)
        return;
    LogFile += wxString (aMessage );
}

void LogMessage(wxString &aMessage) {
    if (!s_savedDialogParameters.WriteLogFile)
        return;
    LogFile += aMessage;
}

void LogChangeArray(void) {
    if (!s_savedDialogParameters.WriteChangeFile
            && !s_savedDialogParameters.WriteLogFile)
                return;

    ChangeFile = _( "Change Array\n");
    for (auto Change : ChangeArray)       //Show all the types of refdes
        ChangeFile += Change.OldRefDesString + " -> " + Change.NewRefDes
                + (Change.Ignore ? _( "* ignored\n") : "\n");
    LogMessage( ChangeFile );              //Include in logfile if logging
}
//
void LogExcludeList(void) {
    if (0 == ExcludeArray.size()) return;

wxString message = _( "\nExcluding: ");

    for (auto Exclude : ExcludeArray)       //Show the refdes we are excluding
        message += Exclude + " ";

    message += _( " from reannotation\n\n");
    LogMessage( message );
}

void LogRefDesTypes(void) {
int i = 1;
wxString message = _( "\n\n\nThere are " ) + std::to_string(RefDesTypes.size());

            message += ( " types of reference designations\n" );

    for (auto Type : RefDesTypes)       //Show all the types of refdes
        message += Type.RefDesType + (0 == (i++ % 16) ? "\n" : " ");

    message += "\n";
    LogMessage(message);
}

void LogModules( wxString &aMessage, std::vector<RefDesInfo> &aModules) {

int i = 1;
wxString message = aMessage
              + _( "\n*********** Sort on " )
              + (std::string) ( s_savedDialogParameters.SortOnModules ? _( "Module" ) : _( "Ref Des" ))
              + _( " Coordinates *******************" ) + "\nSort Code "
              + std::to_string(s_savedDialogParameters.SortDir);

    for (auto mod : aModules) {
        message += "\n" + std::to_string(i++) + " " + mod.RefDesString.mb_str() + " ";
        message += _( "Type: ") + mod.RefDesType + " ";
        message += "# " + std::to_string(mod.RefDesNumber) + " ";
        message += " X " + std::to_string(mod.xstart) + ", Y " + std::to_string(mod.ystart);
        message += _( " Rounded: X " )+ std::to_string(mod.x) + ", Y " + std::to_string(mod.y);
    }
    LogMessage(message);
}

wxString& GetNewRefDes( const wxString &aOldRefDes) {
size_t  i;

    for( i = 0; i < ChangeArray.size(); i++ )
        if (aOldRefDes == ChangeArray[i].OldRefDesString) break;    //Found it

    if( i < ChangeArray.size() )        //Found
        return( ChangeArray[i].NewRefDes );

    wxString warning = _( "\nNot found: " ) + aOldRefDes.ToStdString();
    RenumShowWarning( warning );
    return ((wxString &) aOldRefDes);
}

void BuildExcludeList(void) {
std::string excludethis = "";
    ExcludeArray.clear();
    for (auto thischar : s_savedDialogParameters.ExcludeList) //Break exclude list into words
    {
        if ((' ' == thischar ) || ( ',' == thischar )) {
            ExcludeArray.push_back(excludethis);
            excludethis.clear();
        } else
            excludethis += thischar;
        if (0 != excludethis.size())
            ExcludeArray.push_back(excludethis);
    }
}

void BuildModuleList(MODULES &aModules) {
bool sortonmodules = s_savedDialogParameters.SortOnModules;
int rounder;
unsigned int maxrefdes = 0, sortgrid = (int) (1000000 * s_savedDialogParameters.SortGrid);
size_t firstnum;

wxPoint referencecoordinates, modulecoordinates;
wxString modulerefdes;

RefDesInfo thismodule;
RefDesTypeStr thisrefdestype;

DIALOG_BOARD_RENUM_PARAMETERS renumdialog = s_savedDialogParameters;

    FrontModules.clear();
    BackModules.clear();

    for (auto mod : aModules) {
        thismodule.RefDesString = mod->GetReference();
        firstnum = std::string::npos;	//Assume bad

        if (0 != thismodule.RefDesString.size()) { //No blank strings (Done this way in case compile changes directions and causes exception)
            if (isalpha((int) thismodule.RefDesString.at(0)))
                if (isdigit((int) thismodule.RefDesString.Last()))//Ends with a digit
                    firstnum = thismodule.RefDesString.ToStdString().find_last_not_of("0123456789");
        }
        if (std::string::npos != firstnum) {
            ++firstnum;
            thismodule.RefDesType = thismodule.RefDesString.substr(0, firstnum);
            thismodule.RefDesNumber = wxAtoi(thismodule.RefDesString.substr(firstnum));
        } else {
            thismodule.RefDesType = "";
            thismodule.RefDesNumber = 0;
        }

        thismodule.x = sortonmodules ? mod->GetPosition().x : mod->Reference().GetPosition().x;
        thismodule.y = sortonmodules ? mod->GetPosition().y : mod->Reference().GetPosition().y;

        thismodule.xstart = thismodule.x;
        thismodule.ystart = thismodule.y;

        thismodule.Front = mod->GetLayer() == F_Cu ? true : false;      //

        rounder = thismodule.x % sortgrid;                     //Now round to grid
        thismodule.x -= rounder;                                //X coordinate down to grid
        if (abs(rounder) > (sortgrid / 2))
            thismodule.x += (thismodule.x < 0 ? -sortgrid : sortgrid);

        rounder = thismodule.y % sortgrid;
        thismodule.y -= rounder;                                //X coordinate down to grid
        if (abs(rounder) > (sortgrid / 2))
            thismodule.y += (thismodule.y < 0 ? -sortgrid : sortgrid);

        if (thismodule.Front) {
            if (thismodule.RefDesNumber > maxrefdes)        //Get the highest refdes on the front for error check
                maxrefdes = thismodule.RefDesNumber;        //Need to know max front refdes for error handling
            FrontModules.push_back(thismodule);

        } else
            BackModules.push_back(thismodule);
    }

    SetSortCodes(FrontDirectionsArray, s_savedDialogParameters.SortDir);    //Determine the sort order for the front
    sort(FrontModules.begin(), FrontModules.end(), ModuleCompare);          //Sort the front modules
    SetSortCodes(BackDirectionsArray, s_savedDialogParameters.SortDir);     //Determing the sort order for the back
    sort(BackModules.begin(), BackModules.end(), ModuleCompare);            //Sort the back modules

    unsigned int BackStartRefDes = s_savedDialogParameters.BackStartRefDes;
//
// If I'm not restarting from the front check if starting back refdes < MaxRefDes. If so, warn and correct
//
    {
bool backprefixempty = s_savedDialogParameters.BackPrefix.empty(); //These are shortcut variables
bool removefrontprefix = s_savedDialogParameters.RemoveFrontPrefix;
bool removebackprefix = s_savedDialogParameters.RemoveBackPrefix;
std::string &FrontPrefix = s_savedDialogParameters.FrontPrefix;
std::string &BackPrefix = s_savedDialogParameters.BackPrefix;
std::string warning;

        if ((!backprefixempty && removebackprefix) //If removing prefix have an error
                || (!FrontPrefix.empty() && removefrontprefix)) {

            warning = _( "**** \nRemoving Prefixes takes two passes ****\n"
                          "**** Overiding and renumbering back starting at %d ****\n" )
                         + std::to_string( maxrefdes )
                         +  _( "**** Rerun again after this completes ********\n" );
            RenumShowWarning( warning );

            BackStartRefDes = maxrefdes + 1;
        } else if ((BackStartRefDes != 0) && (BackStartRefDes <= maxrefdes)) {
            if ((backprefixempty)    //Doesn't matter if I'm using a back prefix
                    || (BackPrefix == FrontPrefix)    //It does if same as front prefix
                        || removebackprefix) {           //It does if removing it

                warning = _( "\nWarning: Back Ref Des Start < highest Front Ref Des\nStarting Back at %d\n") + std::to_string(maxrefdes);
                RenumShowWarning( warning );
                BackStartRefDes = maxrefdes + 1;
            }
        }
    }

    RefDesTypes.clear();
    ChangeArray.clear();
    BuildExcludeList();                                 //Exclude these modules from renumbering

    BuildChangeArray( FrontModules, s_savedDialogParameters.FrontStartRefDes); //Create the ChangeArray from front

    if (0 != BackStartRefDes)               //If I don't carry on from the front
        for (auto Type : RefDesTypes)
            Type.RefDesCount = BackStartRefDes;     //Back ref des start here
    else
        BackStartRefDes = s_savedDialogParameters.FrontStartRefDes; //Otherwise a continuation from the front

    BuildChangeArray( BackModules, BackStartRefDes);    //Add to the back
    sort(ChangeArray.begin(), ChangeArray.end(), ChangeArrayCompare); //Sort the front modules
    LogRefDesTypes();                                   //Show the types of ref deses
    LogExcludeList();
    LogChangeArray();                                    //Show the Change Array
}                                    //void BuildModuleList( MODULES &aModules )

//
// Scan through the module arrays and create the from -> to array
//
void BuildChangeArray( std::vector<RefDesInfo> &aModules, unsigned int aStartRefDes) {
size_t i;
RefDesTypeStr thistype;
RefDesChange change;
//
//Define some shortcuts for readability
//
bool addfrontprefix = !s_savedDialogParameters.RemoveFrontPrefix && !s_savedDialogParameters.FrontPrefix.empty();
bool removefrontprefix = s_savedDialogParameters.RemoveFrontPrefix && !s_savedDialogParameters.FrontPrefix.empty();
bool addbackprefix = !s_savedDialogParameters.RemoveBackPrefix && !s_savedDialogParameters.BackPrefix.empty();
bool removebackprefix = s_savedDialogParameters.RemoveBackPrefix && !s_savedDialogParameters.BackPrefix.empty();

std::string &frontprefix = s_savedDialogParameters.FrontPrefix;
std::string &backprefix = s_savedDialogParameters.BackPrefix;

    if( 0 == aModules.size()) return;           //Nothing to do

wxString header ;
    header = ( aModules.front().Front ) ? _("\n\nFront Modules") : _("\n\nBack Modules") ;
    LogModules( header, aModules);

    for (auto Mod : aModules)          //For each module
    {
        for (i = 0; i < RefDesTypes.size(); i++) //See if it is in the types array
            if (RefDesTypes[i].RefDesType == Mod.RefDesType) //Found it!
                break;

        if (i == RefDesTypes.size())    //Not found in the types array so add it
                {
            thistype.RefDesType = Mod.RefDesType;
            thistype.RefDesCount = aStartRefDes;
            RefDesTypes.push_back(thistype);
        }

        change.Ignore = (0 == Mod.RefDesString.size()); //Ignore if no digit at end
        change.NewRefDes = Mod.RefDesType
                + std::to_string(RefDesTypes[i].RefDesCount++); //This is the new refdes

        for (auto exclude : ExcludeArray)
            if (exclude == Mod.RefDesType)
                change.Ignore = true;     //If exclude R1 and R1

        if (change.Ignore)
            change.NewRefDes = Mod.RefDesString;    //Don't change if ignore
        change.OldRefDesString = Mod.RefDesString; //Keep the old ref des for searching
        change.Front = Mod.Front;                                //Copy the side
        change.Found = false;                       //Haven't found this one yet

        if (!change.Ignore)	//Don't bother with prefixes if ignored
        {
            if (change.Front) //Now deal with prefixes. This is easy to screw up so I check to make sure no prefix is already there
            {
                if (((change.NewRefDes.substr(0, frontprefix.size())
                        != frontprefix) && addfrontprefix))
                    change.NewRefDes.insert(0, frontprefix); //No double dipping
                if (removefrontprefix)
                    change.NewRefDes.erase(0., frontprefix.size());
            } else        //Doing the back
            {
                if (((change.NewRefDes.substr(0, backprefix.size())
                        != backprefix) && addbackprefix))
                    change.NewRefDes.insert(0, backprefix);
                if (removebackprefix)
                    change.NewRefDes.erase(0, backprefix.size());
            }
        }
        ChangeArray.push_back(change);                 //Add to the change array
    }
}

