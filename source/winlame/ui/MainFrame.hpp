//
// winLAME - a frontend for the LAME encoding engine
// Copyright (c) 2000-2012 Michael Fink
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
/// \file MainFrame.hpp
/// \brief Main frame window
//
#pragma once

// includes
#include "TasksView.hpp"

// forward references
class TaskManager;

namespace UI
{

/// \brief application main frame
/// \details uses ribbon for commands
/// \see http://www.codeproject.com/Articles/54116/Relook-your-Old-and-New-Native-Applications-with-a
class MainFrame :
   public CRibbonFrameWindowImpl<MainFrame>,
   public CMessageFilter,
   public CIdleHandler
{
   /// base class typedef
   typedef CRibbonFrameWindowImpl<MainFrame> BaseClass;

public:
   /// ctor
   MainFrame(TaskManager& taskManager) throw()
      :m_taskManager(taskManager),
       m_view(taskManager),
       m_bRefreshActive(false),
       m_isAppModeChanged(false)
   {
   }

   /// returns if the dialog has been closed to change the app mode to classic mode
   bool IsAppModeChanged() const throw() { return m_isAppModeChanged; }

   DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

   /// command bar
   CCommandBarCtrl m_CmdBar;

   virtual BOOL PreTranslateMessage(MSG* pMsg);
   virtual BOOL OnIdle();

   BEGIN_UPDATE_UI_MAP(MainFrame)
      UPDATE_ELEMENT(ID_VIEW_RIBBON, UPDUI_MENUPOPUP)
      UPDATE_ELEMENT(ID_TASKS_STOP_ALL, UPDUI_MENUPOPUP | UPDUI_RIBBON | UPDUI_TOOLBAR)
      UPDATE_ELEMENT(ID_TASKS_REMOVE_COMPLETED, UPDUI_MENUPOPUP | UPDUI_RIBBON | UPDUI_TOOLBAR)
   END_UPDATE_UI_MAP()

   BEGIN_MSG_MAP(MainFrame)
      MESSAGE_HANDLER(WM_CREATE, OnCreate)
      MESSAGE_HANDLER(WM_CLOSE, OnClose)
      MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
      MESSAGE_HANDLER(WM_TIMER, OnTimer)
      MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles)
      COMMAND_ID_HANDLER(ID_APP_EXIT, OnAppExit)
      COMMAND_ID_HANDLER(ID_ENCODE_FILES, OnEncodeFiles)
      COMMAND_ID_HANDLER(ID_ENCODE_CD, OnEncodeCD)
      COMMAND_ID_HANDLER(ID_TASKS_STOP_ALL, OnTasksStopAll)
      COMMAND_ID_HANDLER(ID_TASKS_REMOVE_COMPLETED, OnTasksRemoveCompleted)
      COMMAND_ID_HANDLER(ID_SETTINGS_GENERAL, OnSettingsGeneral)
      COMMAND_ID_HANDLER(ID_SETTINGS_CDREAD, OnSettingsCDRead)
      COMMAND_ID_HANDLER(ID_VIEW_RIBBON, OnToggleRibbon)
      COMMAND_ID_HANDLER(ID_VIEW_SWITCH_CLASSIC, OnViewSwitchToClassic)
      COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
      CHAIN_MSG_MAP(BaseClass)
   END_MSG_MAP()

private:
// Handler prototypes (uncomment arguments if needed):
// LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
// LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
// LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

   LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
   LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
   LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
   LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
   LRESULT OnDropFiles(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
   LRESULT OnAppExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
   LRESULT OnEncodeFiles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
   LRESULT OnEncodeCD(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
   LRESULT OnTasksStopAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
   LRESULT OnTasksRemoveCompleted(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
   LRESULT OnSettingsGeneral(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
   LRESULT OnSettingsCDRead(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
   LRESULT OnToggleRibbon(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
   LRESULT OnViewSwitchToClassic(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
   LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
   /// enables tasks list refresh
   void EnableRefresh(bool bEnable = true);

private:
   /// ref to task manager
   TaskManager& m_taskManager;

   /// tasks view
   TasksView m_view;

   /// indicates if tasks list refresh is active
   bool m_bRefreshActive;

   /// indicates if the dialog has been closed to change the app mode to classic mode
   bool m_isAppModeChanged;
};

} // namespace UI
