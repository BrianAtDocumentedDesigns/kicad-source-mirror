/*
 * DIALOBOARD_RENUMGUI.h
 *
 *  Created on: Mar. 29, 2019
 *      Author: BrianP
 */

#ifndef DIALOG_BOARD_RENUM_H_
#define DIALOG_BOARD_RENUM_H_

#include    <wx/wx.h>
#include    "dialog_board_renum_base.h"
#include    <class_board.h>
#include    <class_module.h>
#include    <class_text_mod.h>
#include    <layers_id_colors_and_visibility.h>
#include    <stdint.h>

#include    <pcb_base_frame.h>
#include    <pcb_edit_frame.h>
#include    <project.h>
#include    <fstream>
#include    <netlist_reader/pcb_netlist.h>

#include    <fctsys.h>
#include    <tool/actions.h>
#include    <frame_type.h>
#include    <tool/tool_manager.h>
#include    <unistd.h>

#define SORTXFIRST          0b000       //Sort on X
#define SORTYFIRST          0b100       //Sort on Y
#define ASCENDINGFIRST      0b000       //Sort low to high
#define DESCENDINGFIRST     0b010       //Sort high to low
#define ASCENDINGSECOND     0b000       //Sort low to high
#define DESCENDINGSECOND    0b001       //Sort high to low

#define MINGRID             0.001
#define DEFAULT_GRID        1.000

#define LOGFILENAME         "RenumberLog.txt"

#define SetSortCodes( DirArray, Code ) { \
    SortYFirst = (( DirArray[Code] & SORTYFIRST ) != 0 ); \
    DescendingFirst = (( DirArray[Code] & DESCENDINGFIRST ) != 0 ); \
    DescendingSecond = (( DirArray[Code] & DESCENDINGSECOND ) != 0 ); \
 }


struct  RefDesChange
{
        wxString        NewRefDes;          //The new reference designation (F_U21)
        wxString        OldRefDesString;    //What the old refdes preamble + number was
        bool            Ignore;             //Used to skip (if #, etc)
        bool            Found;              //Found the ref des in the schematic
        bool            Front;              //True if on the front of the board
};

struct  RefDesInfo{
        bool            Front;              //True if on the front of the board
        wxString        RefDesString;       //What its refdes is
        int             xstart, ystart;
        int             x, y;               //The coordinates.
        wxString        RefDesType;         //Keep the type for renumbering.
        unsigned int    RefDesNumber;       //Used for prefixes and checking starting Back refdes
};

struct  RefDesTypeStr {
        wxString        RefDesType;
        unsigned int    RefDesCount;
};

class DIALOG_BOARD_RENUM: public DIALOG_BOARD_RENUM_BASE
{
public:

    DIALOG_BOARD_RENUM( PCB_EDIT_FRAME* aParentFrame );
    ~DIALOG_BOARD_RENUM();

    void ShowMessage( const char *message );
    void ShowMessage( const wxString &message );
    void FatalError( const wxString &message );
    void ShowError( const wxString &message );
    void SetStyle( const char* Colour );
    void GetParameters( void );

    int         LoadPCBFile( struct KiCadFile &Schematic );
    void        RenumKiCadPCB(void);
    void        WriteLogFile(void);
    PCB_EDIT_FRAME* m_frame;
    BOARD*      m_board;
    MODULES     m_modules;



private:
    PCB_EDIT_FRAME* m_parentFrame;

//    int             m_boardWidth;
//    int             m_boardHeight;
//    double          m_boardArea;
//    bool m_hasOutline;

    void OnRenumberClick( wxCommandEvent& event )override;
    void OKDone( wxCommandEvent& event )override;
//    void MainSize(wxSizeEvent& event) override;

};


class DIALOG_BOARD_RENUM_CONTINUEABORT: public DIALOG_BOARD_RENUM_CONTINUEABORT_BASE
{
public:
	DIALOG_BOARD_RENUM_CONTINUEABORT( wxWindow* parent, wxWindowID id = wxID_ANY,
        const wxString& title = wxEmptyString, const wxPoint& pos = wxDefaultPosition, 
            const wxSize& size = wxSize( 362,127 ), 
                long style = wxCAPTION|wxDEFAULT_DIALOG_STYLE | wxFULL_REPAINT_ON_RESIZE);

private:
    void ErrorContinue( wxCommandEvent& event ) override;
    void ErrorAbort( wxCommandEvent& event ) override;
};


class DIALOG_BOARD_RENUM_ABORT: public DIALOG_BOARD_RENUM_ABORT_BASE
{
public:
	DIALOG_BOARD_RENUM_ABORT( wxWindow* parent, wxWindowID id = wxID_ANY,
           const wxString& title = _("Back Start Ref Des too Low"),
               const wxPoint& pos = wxDefaultPosition, 
                    const wxSize& size = wxSize( 244,127 ), 
                        long style = wxCAPTION | wxFULL_REPAINT_ON_RESIZE);

private:
    void ErrorContinue( wxCommandEvent& event ) override;
};

class MyApp : public wxApp
{
  public:
    virtual bool OnInit() override;
};


void    RenumShowMessage( const char *message );
void    RenumShowMessage( std::string &message );
void    RenumShowMessage( const char *message, int arg1  );
void    RenumShowMessage( const char *message, std::string& arg1  );
void    RenumShowMessage( const char *message, std::string& arg1, std::string& arg2 );

void    RenumShowWarning( const char *message, int arg1  );
void    RenumShowWarning( const char *message, std::string& arg1  );
void    RenumShowWarning( const char *message, std::string& arg1, std::string& arg2  );

void    WriteRenumFile( const char *aFileType, std::string& aBuffer);
void    LogMessage(std::string &aMessage );
void    LogModules( const char *amessage, std::vector <RefDesInfo> &aModules );
void    LogRefDesTypes( void );
void    LogChangeArray( void );

void    BuildModuleList( MODULES &m_modules );
void    BuildChangeArray( const char*aMessage, std::vector <RefDesInfo> &aModules, unsigned int aStartRefDes );
wxString    GetNewRefDes( wxString aOldRef );


#endif /* DIALOG_BOARD_RENUMCLASSES_H_ */
