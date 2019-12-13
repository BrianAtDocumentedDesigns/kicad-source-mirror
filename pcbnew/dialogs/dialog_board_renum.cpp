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
// 1) Translatable
//
#include <dialog_board_renum.h>				//
//
struct DIALOG_BOARD_RENUM_PARAMETERS {
    DIALOG_BOARD_RENUM_PARAMETERS() :
            SortOnModules(true), RemoveFrontPrefix(false), RemoveBackPrefix(
                    false), WriteLogFile(false), WriteChangeFile(false),

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

int Errcount;           //The error count

std::string LogFile;
std::string ChangeFile;
std::vector<RefDesChange> ChangeArray;
std::vector<RefDesInfo> FrontModules;
std::vector<RefDesInfo> BackModules;
std::vector<RefDesTypeStr> RefDesTypes;
std::vector<std::string> ExcludeArray;

DIALOG_BOARD_RENUM::DIALOG_BOARD_RENUM(PCB_EDIT_FRAME* aParentFrame) :
        DIALOG_BOARD_RENUM_BASE(aParentFrame), m_modules(
                aParentFrame->GetBoard()->Modules()) {
    s_savedDialogParameters.RenumDialog = this;
    m_frame = aParentFrame;
    m_board = aParentFrame->GetBoard();
    m_modules = m_board->Modules();
    m_parentFrame = aParentFrame;

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

    wxString tmps = "\nNo Board!\n";
    wxFileName filename;
    NETLIST netlist;
    STRING_FORMATTER stringformatter;

    if (!m_board->IsEmpty()) {
        GetParameters();
        LogFile.clear();                              //Clear the log file

        BuildModuleList(m_modules);

        if (s_savedDialogParameters.WriteLogFile)
            WriteRenumFile("_renumlog", LogFile); //Write out the log file

        if (s_savedDialogParameters.WriteChangeFile)
            WriteRenumFile("_renumchange", ChangeFile); //Write out the change file

//
        for (auto mod : m_modules)                //Update the old with the new.
            mod->SetReference(GetNewRefDes(mod->GetReference()));
//
        for (auto& mod : m_modules)
            netlist.AddComponent(
                    new COMPONENT(mod->GetFPID(), mod->GetReference(),//Populate new component
                            mod->GetValue(), mod->GetPath()));

        netlist.Format("pcb_netlist", &stringformatter, 0,
                CTL_OMIT_FILTERS | CTL_OMIT_NETS | CTL_OMIT_FILTERS);
        std::string payload = stringformatter.GetString();//write netlist back to payload (eeschema will recieve that as payload is sent here as reference)
        m_frame->ExecuteRemoteCommand(payload.c_str());

        m_frame->GetToolManager()->RunAction(ACTIONS::zoomFitScreen, true);
        m_frame->GetCanvas()->ForceRefresh();            //is_changed
        m_frame->OnModify();                             //Need to save file on exist.
        tmps =
                "\nDone!\nNow close this dialog and open the schematic in eeSchema, \n"
                        "Tools->Annotate Schematic, select Back annotate from PCB, \n"
                        "if no errors click Annotate..\n"
                        "Then press F8 to update PCB from schematic \n\n";
    }
    m_MessageWindow->AppendText(tmps);
}

void DIALOG_BOARD_RENUM::OKDone(wxCommandEvent& event) {
    (void) event;
//    m_frame->GetToolManager()->RunAction(ACTIONS::zoomFitScreen, true );
    Close(true);
}

void DIALOG_BOARD_RENUM::SetStyle(const char* Colour) {
    m_MessageWindow->SetDefaultStyle(wxColour(Colour));
}

void DIALOG_BOARD_RENUM::ShowMessage(const char *message) {
    m_MessageWindow->AppendText(message);
}

void DIALOG_BOARD_RENUM::ShowMessage(const wxString &message) {
    m_MessageWindow->AppendText(message);
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

DIALOG_BOARD_RENUM_CONTINUEABORT::DIALOG_BOARD_RENUM_CONTINUEABORT(
        wxWindow* parent, wxWindowID id, const wxString& title,
        const wxPoint& pos, const wxSize& size, long style) :
        DIALOG_BOARD_RENUM_CONTINUEABORT_BASE(parent, id, title, pos, size,
                style) {
}

void DIALOG_BOARD_RENUM_CONTINUEABORT::ErrorContinue(wxCommandEvent& event) {
    Errcount = 0;
    this->Destroy();
    (void) event;
}

void DIALOG_BOARD_RENUM_CONTINUEABORT::ErrorAbort(wxCommandEvent& event) {
    (void) event;
    this->Destroy();
}

DIALOG_BOARD_RENUM_ABORT::DIALOG_BOARD_RENUM_ABORT(wxWindow* parent,
        wxWindowID id, const wxString& title, const wxPoint& pos,
        const wxSize& size, long style) :
        DIALOG_BOARD_RENUM_ABORT_BASE(parent, id, title, pos, size, style) {
}

void DIALOG_BOARD_RENUM_ABORT::ErrorContinue(wxCommandEvent& event) {
    this->Destroy();
    (void) event;
}
//
// Use these to make errors and messages a bit easier
//
void RenumShowMessage(std::string &aMessage) {
    s_savedDialogParameters.RenumDialog->ShowMessage(aMessage);
}

void RenumShowMessage(const char *aMessage) {
    s_savedDialogParameters.RenumDialog->ShowMessage(aMessage);
}

void RenumShowMessage(const char *aMessage, int arg1) {
    s_savedDialogParameters.RenumDialog->ShowMessage(
            wxString::Format(aMessage, arg1));
}

void RenumShowMessage(const char *aMessage, std::string& arg1) {
    s_savedDialogParameters.RenumDialog->ShowMessage(
            wxString::Format(aMessage, arg1.c_str()));
}
//
void RenumShowMessage(const char *aMessage, std::string& arg1, std::string& arg2) {
    s_savedDialogParameters.RenumDialog->ShowMessage(
            wxString::Format(aMessage, arg1.c_str(), arg2.c_str()));
}

void RenumShowWarning(const char *aMessage, int arg1) {
    s_savedDialogParameters.RenumDialog->SetStyle("RED");
    s_savedDialogParameters.RenumDialog->ShowMessage(
            wxString::Format(aMessage, arg1));
    s_savedDialogParameters.RenumDialog->SetStyle("BLACK");
}

void RenumShowWarning(const char *aMessage, std::string& arg1) {
    s_savedDialogParameters.RenumDialog->SetStyle("RED");
    s_savedDialogParameters.RenumDialog->ShowMessage(
            wxString::Format(aMessage, arg1.c_str()));
    s_savedDialogParameters.RenumDialog->SetStyle("BLACK");
}
void RenumShowWarning(const char *aMessage, std::string& arg1, std::string& arg2) {
    s_savedDialogParameters.RenumDialog->SetStyle("RED");
    s_savedDialogParameters.RenumDialog->ShowMessage(
            wxString::Format(aMessage, arg1.c_str(), arg2.c_str()));
    s_savedDialogParameters.RenumDialog->SetStyle("BLACK");
}

static bool ChangeArrayCompare(const RefDesChange &aA, const RefDesChange &aB) {
    return (aA.OldRefDesString < aB.OldRefDesString);
}

//
// Use std::sort() to sort modules. Because it is a structure a compare function is needed
// Returns true if the first coordinate should be before the second coordinate
//
static bool ModuleCompare(const RefDesInfo &aA, const RefDesInfo &aB) {
    int X0 = aA.x, X1 = aB.x, Y0 = aA.y, Y1 = aB.y;
    int tmp;

    if (SortYFirst)  //If sorting by Y then X, swap X and Y
    {
        tmp = X0;
        X0 = Y0;
        Y0 = tmp;
        tmp = X1;
        X1 = Y1;
        Y1 = tmp;
    }

    if (DescendingFirst) {
        tmp = X0;
        X0 = X1;
        X1 = tmp;
    } //If descending, same compare just swap directions
    if (DescendingSecond) {
        tmp = Y0;
        Y0 = Y1;
        Y1 = tmp;
    }

    if (X0 < X1) return (true);               //yes, its smaller
    if (X0 > X1) return (false);              //No its not
    if (Y0 < Y1) return (true);               //same but equal
    return (false);
}

//
//Write the string to filename
//
void WriteRenumFile(const char *aFileType, std::string& aBuffer) {
    wxFileName filename =
            s_savedDialogParameters.RenumDialog->m_board->GetFileName();
    filename.SetName(filename.GetName() + aFileType);
    filename.SetExt("txt");

    std::string FullFileName = filename.GetFullPath().ToStdString();

    std::ofstream tmphandle(FullFileName, std::ios::trunc); //Open the file for writing

    if (tmphandle.is_open())
        tmphandle << aBuffer;           //Write the buffer
    if (!tmphandle.is_open() || tmphandle.bad())           //Error?
            {
        RenumShowWarning("\n\nCan't write ", 0);
        RenumShowWarning(FullFileName.c_str(), 0);
    }
    tmphandle.close();
}

void LogMessage(std::string &aMessage) {
    if (!s_savedDialogParameters.WriteLogFile)
        return;
    LogFile += aMessage;
}

void LogChangeArray(void) {
    if (!s_savedDialogParameters.WriteChangeFile
            && !s_savedDialogParameters.WriteLogFile)
        return;

    ChangeFile = "Change Array\n";
    for (auto Change : ChangeArray)       //Show all the types of refdes
        ChangeFile += Change.OldRefDesString + " -> " + Change.NewRefDes
                + (Change.Ignore ? "* ignored\n" : "\n");
    LogMessage(ChangeFile);              //Include in logfile if logging
}
//
void LogExcludeList(void) {
    if (0 == ExcludeArray.size())
        return;

    std::string Message = "\nExcluding: ";

    for (auto Exclude : ExcludeArray)       //Show the refdes we are excluding
        Message += Exclude + " ";

    Message += " from reannotation\n\n";
    LogMessage(Message);
}

void LogRefDesTypes(void) {
    int i = 1;
    std::string Message = "\n\n\nThere are "
            + std::to_string(RefDesTypes.size())
            + " types of reference designations\n";

    for (auto Type : RefDesTypes)       //Show all the types of refdes
        Message += Type.RefDesType + (0 == (i++ % 16) ? "\n" : " ");

    Message += "\n";
    LogMessage(Message);
}

void LogModules(const char *aMessage, std::vector<RefDesInfo> &aModules) {
    int i = 1;

    std::string Message = aMessage;

    Message +=
            "\n*********** Sort on "
                    + (std::string) (
                            s_savedDialogParameters.SortOnModules ?
                                    "Module" : "Ref Des")
                    + " Coordinates *******************" + "\nSort Code "
                    + std::to_string(s_savedDialogParameters.SortDir);

    for (auto mod : aModules) {
        Message += "\n" + std::to_string(i++) + " " + mod.RefDesString.mb_str()
                + " ";
        Message += "Type: " + mod.RefDesType + " ";
        Message += "# " + std::to_string(mod.RefDesNumber) + " ";
        Message += " X " + std::to_string(mod.xstart) + ", Y "
                + std::to_string(mod.ystart);
        Message += " Rounded: X " + std::to_string(mod.x) + ", Y "
                + std::to_string(mod.y);
    }
    LogMessage(Message);
}

wxString GetNewRefDes(wxString aOldRefDes) {
    for (auto Change : ChangeArray)
        if (aOldRefDes == Change.OldRefDesString)
            return (Change.NewRefDes);

    std::string tmps = "\nNot found: " + aOldRefDes.ToStdString();
    RenumShowWarning((const char *) tmps.c_str(), 0);
    return (aOldRefDes);
}

void BuildExcludeList(void) {
    std::string ExcludeThis = "";
    ExcludeArray.clear();
    for (auto thischar : s_savedDialogParameters.ExcludeList) //Break exclude list into words
    {
        if (thischar == ' ') {
            ExcludeArray.push_back(ExcludeThis);
            ExcludeThis = "";
        } else
            ExcludeThis += thischar;
        if (0 != ExcludeThis.size())
            ExcludeArray.push_back(ExcludeThis);
    }
}

void BuildModuleList(MODULES &aModules) {
    bool SortOnModules = s_savedDialogParameters.SortOnModules;
    int Rounder;
    unsigned int MaxRefDes = 0, iSortGrid = (int) (1000000
            * s_savedDialogParameters.SortGrid);
    size_t FirstNum;

    wxPoint ReferenceCoordinates, ModuleCoordinates;
    wxString ModuleRefDes;

    RefDesInfo ThisModule;
    RefDesTypeStr ThisRefDesType;

    DIALOG_BOARD_RENUM_PARAMETERS RenumDialog = s_savedDialogParameters;

    FrontModules.clear();
    BackModules.clear();

    for (auto mod : aModules) {
        ThisModule.RefDesString = mod->GetReference();
        FirstNum = std::string::npos;	//Assume bad

        if (0 != ThisModule.RefDesString.size())//No blank strings (Done this way in case compile changes directions and causes exceptio
                {
            if (isalpha((int) ThisModule.RefDesString.at(0)))
                if (isdigit((int) ThisModule.RefDesString.Last()))//Ends with a digit
                    FirstNum =
                            ThisModule.RefDesString.ToStdString().find_last_not_of(
                                    "0123456789");
        }
        if (std::string::npos != FirstNum) {
            ++FirstNum;
            ThisModule.RefDesType = ThisModule.RefDesString.substr(0, FirstNum);
            ThisModule.RefDesNumber = wxAtoi(
                    ThisModule.RefDesString.substr(FirstNum));
        } else {
            ThisModule.RefDesType = "";
            ThisModule.RefDesNumber = 0;
        }

        ThisModule.x =
                SortOnModules ?
                        mod->GetPosition().x : mod->Reference().GetPosition().x;
        ThisModule.y =
                SortOnModules ?
                        mod->GetPosition().y : mod->Reference().GetPosition().y;

        ThisModule.xstart = ThisModule.x;
        ThisModule.ystart = ThisModule.y;

        ThisModule.Front = mod->GetLayer() == F_Cu ? true : false;      //

        Rounder = ThisModule.x % iSortGrid;                  //Now round to grid
        ThisModule.x -= Rounder;                     //X coordinate down to grid
        if (abs(Rounder) > (iSortGrid / 2))
            ThisModule.x += (ThisModule.x < 0 ? -iSortGrid : iSortGrid);

        Rounder = ThisModule.y % iSortGrid;
        ThisModule.y -= Rounder;                     //X coordinate down to grid
        if (abs(Rounder) > (iSortGrid / 2))
            ThisModule.y += (ThisModule.y < 0 ? -iSortGrid : iSortGrid);

        if (ThisModule.Front) {
            if (ThisModule.RefDesNumber > MaxRefDes)//Get the highest refdes on the front for error check
                MaxRefDes = ThisModule.RefDesNumber; //Need to know max front refdes for error handling
            FrontModules.push_back(ThisModule);

        } else
            BackModules.push_back(ThisModule);
    }

    SetSortCodes(FrontDirectionsArray, s_savedDialogParameters.SortDir); //Determine the sort order for the front
    sort(FrontModules.begin(), FrontModules.end(), ModuleCompare); //Sort the front modules
    SetSortCodes(BackDirectionsArray, s_savedDialogParameters.SortDir); //Determing the sort order for the back
    sort(BackModules.begin(), BackModules.end(), ModuleCompare); //Sort the back modules

    unsigned int BackStartRefDes = s_savedDialogParameters.BackStartRefDes;
//
// If I'm not restarting from the front check if starting back refdes < MaxRefDes. If so, warn and correct
//
    {
        bool BackPrefixEmpty = s_savedDialogParameters.BackPrefix.empty(); //These are shortcut variables
        bool RemoveFrontPrefix = s_savedDialogParameters.RemoveFrontPrefix;
        bool RemoveBackPrefix = s_savedDialogParameters.RemoveBackPrefix;
        std::string &FrontPrefix = s_savedDialogParameters.FrontPrefix;
        std::string &BackPrefix = s_savedDialogParameters.BackPrefix;

        if ((!BackPrefixEmpty && RemoveBackPrefix) //If removing prefix have an error
        || (!FrontPrefix.empty() && RemoveFrontPrefix)) {
            RenumShowWarning("**** \nRemoving Prefixes takes two passes ****\n",
                    0);
            RenumShowWarning(
                    "**** Overiding and renumbering back sarting at %d ****\n",
                    MaxRefDes);
            RenumShowWarning("**** Rerun again after this completes ********\n",
                    0);

            BackStartRefDes = MaxRefDes + 1;
        } else if ((BackStartRefDes != 0) && (BackStartRefDes <= MaxRefDes)) {
            if ((BackPrefixEmpty)    //Doesn't matter if I'm using a back prefix
            || (BackPrefix == FrontPrefix)    //It does if same as front prefix
                    || RemoveBackPrefix)           //It does if removing it
                    {
                RenumShowWarning(
                        "\nWarning: Back Ref Des Start < highest Front Ref Des\nStarting Back at %d\n",
                        MaxRefDes);
                BackStartRefDes = MaxRefDes + 1;
            }
        }
    }

    RefDesTypes.clear();
    ChangeArray.clear();

    BuildChangeArray("Top Modules", FrontModules,
            s_savedDialogParameters.FrontStartRefDes); //Create the ChangeArray from front

    if (0 != BackStartRefDes)               //If I don't carry on from the front
        for (auto Type : RefDesTypes)
            Type.RefDesCount = BackStartRefDes;     //Back ref des start here
    else
        BackStartRefDes = s_savedDialogParameters.FrontStartRefDes; //Otherwise a continuation from the front

    BuildChangeArray("\n\nBack Modules", BackModules, BackStartRefDes); //Add to the back
    sort(ChangeArray.begin(), ChangeArray.end(), ChangeArrayCompare); //Sort the front modules
    LogRefDesTypes();                              //Show the types of ref deses
    LogExcludeList();
    LogChangeArray();                                    //Show the Change Array
}                                    //void BuildModuleList( MODULES &aModules )

//
// Scan through the module arrays and create the from -> to array
//
void BuildChangeArray(const char*aMessage, std::vector<RefDesInfo> &aModules,
        unsigned int aStartRefDes) {
    size_t i;
    RefDesTypeStr ThisType;
    RefDesChange Change;

//
//Define some shortcuts for readability
//
    bool AddFrontPrefix = !s_savedDialogParameters.RemoveFrontPrefix
            && !s_savedDialogParameters.FrontPrefix.empty();
    bool RemoveFrontPrefix = s_savedDialogParameters.RemoveFrontPrefix
            && !s_savedDialogParameters.FrontPrefix.empty();
    bool AddBackPrefix = !s_savedDialogParameters.RemoveBackPrefix
            && !s_savedDialogParameters.BackPrefix.empty();
    bool RemoveBackPrefix = s_savedDialogParameters.RemoveBackPrefix
            && !s_savedDialogParameters.BackPrefix.empty();

    std::string &FrontPrefix = s_savedDialogParameters.FrontPrefix;
    std::string &BackPrefix = s_savedDialogParameters.BackPrefix;

    LogModules(aMessage, aModules);
    BuildExcludeList();

    for (auto Mod : aModules)          //For each module
    {
        for (i = 0; i < RefDesTypes.size(); i++) //See if it is in the types array
            if (RefDesTypes[i].RefDesType == Mod.RefDesType) //Found it!
                break;

        if (i == RefDesTypes.size())    //Not found in the types array so add it
                {
            ThisType.RefDesType = Mod.RefDesType;
            ThisType.RefDesCount = aStartRefDes;
            RefDesTypes.push_back(ThisType);
        }

        Change.Ignore = (0 == Mod.RefDesString.size()); //Ignore if no digit at end
        Change.NewRefDes = Mod.RefDesType
                + std::to_string(RefDesTypes[i].RefDesCount++); //This is the new refdes

        for (auto exclude : ExcludeArray)
            if (exclude == Mod.RefDesType)
                Change.Ignore = true;     //If exclude R1 and R1

        if (Change.Ignore)
            Change.NewRefDes = Mod.RefDesString;    //Don't change if ignore
        Change.OldRefDesString = Mod.RefDesString; //Keep the old ref des for searching
        Change.Front = Mod.Front;                                //Copy the side
        Change.Found = false;                       //Haven't found this one yet

        if (!Change.Ignore)	//Don't bother with prefixes if ignored
        {
            if (Change.Front) //Now deal with prefixes. This is easy to screw up so I check to make sure no prefix is already there
            {
                if (((Change.NewRefDes.substr(0, FrontPrefix.size())
                        != FrontPrefix) && AddFrontPrefix))
                    Change.NewRefDes.insert(0, FrontPrefix); //No double dipping
                if (RemoveFrontPrefix)
                    Change.NewRefDes.erase(0., FrontPrefix.size());
            } else        //Doing the back
            {
                if (((Change.NewRefDes.substr(0, BackPrefix.size())
                        != BackPrefix) && AddBackPrefix))
                    Change.NewRefDes.insert(0, BackPrefix);
                if (RemoveBackPrefix)
                    Change.NewRefDes.erase(0, BackPrefix.size());
            }
        }
        ChangeArray.push_back(Change);                 //Add to the change array
    }
}

