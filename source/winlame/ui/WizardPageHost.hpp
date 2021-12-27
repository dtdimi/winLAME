//
// winLAME - a frontend for the LAME encoding engine
// Copyright (c) 2000-2018 Michael Fink
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
/// \file WizardPageHost.hpp
/// \brief Wizard page host window
//
#pragma once

#include "resource.h"
#include "WizardPage.hpp"
#include "HtmlHelper.hpp"

namespace UI
{
   /// host window for wizard page
   class WizardPageHost :
      public CDialogImpl<WizardPageHost>,
      public CWinDataExchange<WizardPageHost>,
      public CDialogResize<WizardPageHost>,
      private CMessageLoop
   {
   public:
      /// ctor
      explicit WizardPageHost(bool isClassicMode = false);
      /// dtor
      ~WizardPageHost() {}

      /// sets wizard page
      void SetWizardPage(std::shared_ptr<WizardPage> spCurrentPage);

      /// \brief runs the wizard pages until the pages return
      /// \retval IDOK ok or finish button was pressed
      /// \retval IDCANCEL cancel button was pressed or window was closed with X
      /// \retval ID_WIZBACK back button was pressed
      int Run(HWND hWndParent);

      /// returns if wizard page host is currently operating in classic mode
      bool IsClassicMode() const { return m_isClassicMode; }

      /// returns if the dialog has been closed to change the app mode to modern mode
      bool IsAppModeChanged() const { return m_isAppModeChanged; }

      /// dialog id
      enum { IDD = IDD_WIZARDPAGE_HOST };

      // resize map
      BEGIN_DLGRESIZE_MAP(WizardPageHost)
         DLGRESIZE_CONTROL(IDC_WIZARDPAGE_STATIC_FRAME, DLSZ_SIZE_X | DLSZ_SIZE_Y)
         DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
         DLGRESIZE_CONTROL(IDC_WIZARDPAGE_BUTTON_ACTION1, DLSZ_MOVE_X | DLSZ_MOVE_Y)
         DLGRESIZE_CONTROL(IDC_WIZARDPAGE_BUTTON_ACTION2, DLSZ_MOVE_X | DLSZ_MOVE_Y)
         DLGRESIZE_CONTROL(IDC_WIZARDPAGE_HELP, DLSZ_MOVE_Y)
         DLGRESIZE_CONTROL(ID_VIEW_SWITCH_MODERN, DLSZ_MOVE_Y)
         DLGRESIZE_CONTROL(IDC_WIZARDPAGE_STATIC_CAPTION, DLSZ_SIZE_X)
         DLGRESIZE_CONTROL(IDC_WIZARDPAGE_STATIC_BACKGROUND, DLSZ_SIZE_X)
      END_DLGRESIZE_MAP()

      // ddx map
      BEGIN_DDX_MAP(WizardPageHost)
         DDX_CONTROL_HANDLE(IDC_WIZARDPAGE_STATIC_CAPTION, m_staticCaption)
         DDX_CONTROL_HANDLE(IDC_WIZARDPAGE_BUTTON_ACTION1, m_buttonAction1)
         DDX_CONTROL_HANDLE(IDC_WIZARDPAGE_BUTTON_ACTION2, m_buttonAction2)
         DDX_CONTROL_HANDLE(IDOK, m_buttonActionOK)
      END_DDX_MAP()

      // message map
      BEGIN_MSG_MAP(WizardPageHost)
         MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
         MESSAGE_HANDLER(WM_SIZE, OnSize)
         MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnStaticColor)
         MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
         MESSAGE_HANDLER(WM_HELP, OnHelp)
         COMMAND_HANDLER(IDC_WIZARDPAGE_BUTTON_ACTION1, BN_CLICKED, OnButtonClicked)
         COMMAND_HANDLER(IDC_WIZARDPAGE_BUTTON_ACTION2, BN_CLICKED, OnButtonClicked)
         COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnButtonClicked)
         COMMAND_HANDLER(IDOK, BN_CLICKED, OnButtonClicked)
         COMMAND_HANDLER(IDC_WIZARDPAGE_HELP, BN_CLICKED, OnHelpButton)
         COMMAND_ID_HANDLER(ID_VIEW_SWITCH_MODERN, OnViewSwitchToModern)
         MESSAGE_HANDLER(WM_SYSCOMMAND, OnSysCommand)
         CHAIN_MSG_MAP(CDialogResize<WizardPageHost>)
      END_MSG_MAP()

      /// called when dialog is initialized
      LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

      /// called when dialog is resized
      LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

      /// called when a static control background color should be set
      LRESULT OnStaticColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

      /// called when an item should be drawn
      LRESULT OnDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

      /// called when any of the wizard buttons have been clicked
      LRESULT OnButtonClicked(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

      /// called when pressing F1
      LRESULT OnHelp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

      /// called on clicking on the help button
      LRESULT OnHelpButton(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

      /// called when clicking the "switch to Modern UI" button
      LRESULT OnViewSwitchToModern(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

      /// called for every system command; used for the about box system menu entry; only used in classic mode
      LRESULT OnSysCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

   private:
      /// initializes current page
      void InitPage();

      /// configure wizard buttons based on wizard page type
      void ConfigWizardButtons(WizardPage::T_enWizardPageType enWizardPageType);

      /// adds tooltips for all controls under given window handle
      void AddTooltips(HWND hWnd);

      /// switches to Modern UI mode; only used if in classic mode
      void SwitchToModernMode();

      /// adds system menu entries; only in classic mode
      void AddSystemMenuEntries();

      /// maps dialog resource ID to help path in html help file
      static CString MapDialogIdToHelpPath(UINT dialogId);

      /// shows html help page for current page
      void ShowHelp();

      /// called before messages are processed by the message loop
      virtual BOOL PreTranslateMessage(MSG* pMsg) override;

   private:
      // controls

      /// caption
      CStatic m_staticCaption;

      /// font for drawing caption
      CFont m_fontCaption;

      /// cancel button, if visible
      CButton m_buttonAction1;

      /// cancel or back button
      CButton m_buttonAction2;

      /// next or finish button
      CButton m_buttonActionOK;

      /// tooltip control showing tool tips of controls
      CToolTipCtrl m_tooltipCtrl;

      /// help icon
      CImageList m_helpIcon;

      // model

      /// indicates if host is currently in classic mode
      bool m_isClassicMode;

      /// page size; used for resizing
      CSize m_sizePage;

      /// current wizard page
      std::shared_ptr<WizardPage> m_spCurrentPage;

      /// html help object
      HtmlHelper m_htmlHelper;

      /// indicates if the dialog has been closed to change the app mode to modern mode
      bool m_isAppModeChanged;
   };

} // namespace UI
