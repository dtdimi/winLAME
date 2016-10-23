//
// winLAME - a frontend for the LAME encoding engine
// Copyright (c) 2000-2014 Michael Fink
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
/// \file InputCDPage.cpp
/// \brief Input CD page
//
#include "StdAfx.h"
#include "InputCDPage.hpp"
#include "WizardPageHost.hpp"
#include "OutputSettingsPage.hpp"
#include "CDReadSettingsPage.hpp"
#include "IoCContainer.hpp"
#include "UISettings.h"
#include "DynamicLibrary.h"
#include "basscd.h"
#include "FreedbResolver.hpp"
#include "CommonStuff.h"
#include "FreeDbDiscListDlg.hpp"

using namespace UI;

const DWORD INVALID_DRIVE_ID = 0xffffffff;

InputCDPage::InputCDPage(WizardPageHost& pageHost) throw()
:WizardPage(pageHost, IDD_PAGE_INPUT_CD, WizardPage::typeCancelNext),
m_uiSettings(IoCContainer::Current().Resolve<UISettings>()),
m_bEditedTrack(false),
m_bDriveActive(false),
m_bAcquiredDiscInfo(false)
{
}

bool InputCDPage::IsCDExtractionAvail() throw()
{
   return DynamicLibrary(_T("basscd.dll")).IsLoaded();
}

LRESULT InputCDPage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
   DoDataExchange(DDX_LOAD);
   DlgResize_Init(false, false);

   SetupDriveCombobox();

   SetupTracksList();

   // genre combobox
   LPCTSTR* apszGenre = TrackInfo::GetGenreList();
   for (unsigned int i = 0, iMax = TrackInfo::GetGenreListLength(); i<iMax; i++)
      m_cbGenre.AddString(apszGenre[i]);

   GetDlgItem(IDC_CDSELECT_BUTTON_PLAY).EnableWindow(false);
   GetDlgItem(IDC_CDSELECT_BUTTON_STOP).EnableWindow(false);

   m_bEditedTrack = false;

   CheckCD();

   SetTimer(IDT_CDRIP_CHECK, 2 * 1000);

   return 1;
}

LRESULT InputCDPage::OnButtonOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
   KillTimer(IDT_CDRIP_CHECK);

   {
      if (m_bEditedTrack)
         StoreInCdplayerIni(GetCurrentDrive());

      // TODO put selected tracks in encoderjoblist
      //UpdateTrackManager();
   }

   m_uiSettings.m_bFromInputFilesPage = false;
   m_pageHost.SetWizardPage(std::shared_ptr<WizardPage>(new OutputSettingsPage(m_pageHost)));

   return 0;
}

LRESULT InputCDPage::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
   if (wParam == IDT_CDRIP_CHECK)
      CheckCD();
   return 0;
}

LRESULT InputCDPage::OnDriveSelEndOk(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)//(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
   RefreshCDList();
   return 0;
}

LRESULT InputCDPage::OnListDoubleClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& bHandled)
{
   return OnClickedButtonPlay(0, 0, 0, bHandled);
}

LRESULT InputCDPage::OnClickedButtonPlay(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
   DWORD nDrive = GetCurrentDrive();

   int nItem = m_lcTracks.GetNextItem(-1, LVNI_ALL | LVNI_SELECTED);

   if (nItem < 0) nItem = 0; // no track selected? play first one

   DWORD nTrack = m_lcTracks.GetItemData(nItem);

   BASS_CD_Analog_Play(nDrive, nTrack, 0);
   GetDlgItem(IDC_CDSELECT_BUTTON_STOP).EnableWindow(true);

   return 0;
}

LRESULT InputCDPage::OnClickedButtonStop(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
   BASS_CD_Analog_Stop(GetCurrentDrive());

   GetDlgItem(IDC_CDSELECT_BUTTON_STOP).EnableWindow(false);

   return 0;
}

LRESULT InputCDPage::OnClickedButtonFreedb(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
   FreedbLookup();

   return 0;
}

LRESULT InputCDPage::OnClickedButtonOptions(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
   WizardPageHost host;
   host.SetWizardPage(std::shared_ptr<WizardPage>(
      new CDReadSettingsPage(host, m_uiSettings)));
   host.Run(m_hWnd);

   return 0;
}

LRESULT InputCDPage::OnClickedCheckVariousArtists(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
   bool bCheck = BST_CHECKED != m_checkVariousArtists.GetCheck();

   GetDlgItem(IDC_CDSELECT_EDIT_ARTIST).EnableWindow(bCheck);
   GetDlgItem(IDC_CDSELECT_STATIC_ARTIST).EnableWindow(bCheck);

   return 0;
}

LRESULT InputCDPage::OnChangedEditCtrl(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
   m_bEditedTrack = true;

   return 0;
}

LRESULT InputCDPage::OnEndLabelEdit(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
   NMLVDISPINFO* pLvDispInfo = reinterpret_cast<NMLVDISPINFO*>(pnmh);
   if (pLvDispInfo->item.iItem == -1)
      return 0;

   m_bEditedTrack = true;

   return 1;
}
void InputCDPage::SetupDriveCombobox()
{
   unsigned int nDriveCount = 0;

   for (DWORD n = 0; n<26; n++)
   {
      BASS_CD_INFO info = { 0 };
      BOOL bRet = BASS_CD_GetInfo(n, &info);
      if (bRet == FALSE)
         break;

      CString cszDriveDescription(info.product);
      DWORD nLetter = info.letter;

      if (!cszDriveDescription.IsEmpty())
      {
         CString cszText;

         if (nLetter == -1)
            cszText = cszDriveDescription; // couldn't get drive letter; restricted user account
         else
            cszText.Format(_T("[%c:] %s"), _T('A') + nLetter, cszDriveDescription);

         int nItem = m_cbDrives.AddString(cszText);
         m_cbDrives.SetItemData(nItem, n);

         nDriveCount++;
      }
   }

   m_cbDrives.SetCurSel(0);

   // hide drive combobox when only one drive present
   if (nDriveCount <= 1)
      HideDriveCombobox();
}

void InputCDPage::HideDriveCombobox()
{
   CRect rcCombo, rcList;
   m_cbDrives.GetWindowRect(rcCombo);
   m_lcTracks.GetWindowRect(rcList);
   rcList.top = rcCombo.top;
   ScreenToClient(rcList);
   m_lcTracks.MoveWindow(rcList);

   m_cbDrives.ShowWindow(SW_HIDE);
   m_lcTracks.SetFocus();
}

void InputCDPage::SetupTracksList()
{
   // tracks list
   m_lcTracks.SetExtendedListViewStyle(LVS_EX_ONECLICKACTIVATE | LVS_EX_FULLROWSELECT,
      LVS_EX_ONECLICKACTIVATE | LVS_EX_FULLROWSELECT);

   CString cszText(MAKEINTRESOURCE(IDS_CDRIP_COLUMN_NR));
   m_lcTracks.InsertColumn(0, cszText, LVCFMT_LEFT, 30, 0);
   cszText.LoadString(IDS_CDRIP_COLUMN_TRACK);
   m_lcTracks.InsertColumn(1, cszText, LVCFMT_LEFT, 250, 0);
   cszText.LoadString(IDS_CDRIP_COLUMN_LENGTH);
   m_lcTracks.InsertColumn(2, cszText, LVCFMT_LEFT, 60, 0);

   cszText.LoadString(IDS_CDRIP_UNKNOWN_ARTIST);
   SetDlgItemText(IDC_CDSELECT_EDIT_ARTIST, cszText);
   cszText.LoadString(IDS_CDRIP_UNKNOWN_TITLE);
   SetDlgItemText(IDC_CDSELECT_EDIT_TITLE, cszText);
}

DWORD InputCDPage::GetCurrentDrive()
{
   int nSel = m_cbDrives.GetCurSel();
   if (nSel == -1)
      return INVALID_DRIVE_ID;

   return m_cbDrives.GetItemData(nSel);
}

void InputCDPage::RefreshCDList()
{
   CWaitCursor waitCursor;

   DWORD nDrive = GetCurrentDrive();
   if (nDrive == INVALID_DRIVE_ID)
      return;

   m_lcTracks.SetRedraw(FALSE);
   m_lcTracks.DeleteAllItems();

   m_bEditedTrack = false;

   if (FALSE == BASS_CD_IsReady(nDrive))
   {
      m_bDriveActive = false;
      m_lcTracks.SetRedraw(TRUE);
      return;
   }

   m_bDriveActive = true;

   DWORD uMaxCDTracks = BASS_CD_GetTracks(nDrive);
   if (uMaxCDTracks == DWORD(-1))
   {
      m_lcTracks.SetRedraw(TRUE);
      return;
   }

   for (DWORD n = 0; n<uMaxCDTracks; n++)
   {
      CString cszText;
      cszText.Format(_T("%u"), n + 1);

      DWORD nLength = BASS_CD_GetTrackLength(nDrive, n);
      bool bDataTrack = (nLength == 0xFFFFFFFF && BASS_ERROR_NOTAUDIO == BASS_ErrorGetCode());

      int nItem = m_lcTracks.InsertItem(m_lcTracks.GetItemCount(), cszText);
      m_lcTracks.SetItemData(nItem, n);

      cszText.Format(IDS_CDRIP_TRACK_U, n + 1);
      m_lcTracks.SetItemText(nItem, 1, cszText);

      if (!bDataTrack)
      {
         if (nLength != 0xFFFFFFFF)
         {
            nLength /= 176400;

            cszText.Format(_T("%u:%02u"), nLength / 60, nLength % 60);
            m_lcTracks.SetItemText(nItem, 2, cszText);
         }
         else
         {
            cszText.LoadString(IDS_CDRIP_TRACKTYPE_UNKNOWN);
            m_lcTracks.SetItemText(nItem, 2, cszText);
         }
      }
      else
      {
         cszText.LoadString(IDS_CDRIP_TRACKTYPE_DATA);
         m_lcTracks.SetItemText(nItem, 2, cszText);
      }
   }

   bool bVarious = false;

   if (!ReadCdplayerIni(bVarious))
      ReadCDText(bVarious);

   // check or uncheck "various artists"
   m_checkVariousArtists.SetCheck(bVarious ? BST_CHECKED : BST_UNCHECKED);

   BOOL bDummy = true;
   OnClickedCheckVariousArtists(0, 0, NULL, bDummy);

   m_bEditedTrack = false;

   m_lcTracks.SetRedraw(TRUE);
}

bool InputCDPage::ReadCdplayerIni(bool& bVarious)
{
   // retrieve info from cdplayer.ini
   CString cszCDPlayerIniFilename;
   ::GetWindowsDirectory(cszCDPlayerIniFilename.GetBuffer(MAX_PATH), MAX_PATH);
   cszCDPlayerIniFilename.ReleaseBuffer();
   cszCDPlayerIniFilename += _T("\\cdplayer.ini");

   DWORD nDrive = GetCurrentDrive();

   DWORD uMaxCDTracks = BASS_CD_GetTracks(nDrive);

   const char* cdplayer_id_raw = BASS_CD_GetID(nDrive, BASS_CDID_CDPLAYER);

   CString cdplayer_id(cdplayer_id_raw);

   unsigned int nNumTracks = 0;
   if (cdplayer_id_raw != NULL && 0 != (nNumTracks = ::GetPrivateProfileInt(cdplayer_id, _T("numtracks"), 0, cszCDPlayerIniFilename)))
   {
      CString cszText;
      // title
      ::GetPrivateProfileString(cdplayer_id, _T("title"), _T("[]#"), cszText.GetBuffer(512), 512, cszCDPlayerIniFilename);
      cszText.ReleaseBuffer();

      if (cszText != _T("[]#"))
         SetDlgItemText(IDC_CDSELECT_EDIT_TITLE, cszText);

      // artist
      ::GetPrivateProfileString(cdplayer_id, _T("artist"), _T("[]#"), cszText.GetBuffer(512), 512, cszCDPlayerIniFilename);
      cszText.ReleaseBuffer();

      if (cszText != _T("[]#"))
         SetDlgItemText(IDC_CDSELECT_EDIT_ARTIST, cszText);

      if (-1 != cszText.Find(_T("various")))
         bVarious = true;

      // year
      ::GetPrivateProfileString(cdplayer_id, _T("year"), _T("[]#"), cszText.GetBuffer(512), 512, cszCDPlayerIniFilename);
      cszText.ReleaseBuffer();

      if (cszText != _T("[]#"))
         SetDlgItemText(IDC_CDSELECT_EDIT_YEAR, cszText);

      // genre
      ::GetPrivateProfileString(cdplayer_id, _T("genre"), _T("[]#"), cszText.GetBuffer(512), 512, cszCDPlayerIniFilename);
      cszText.ReleaseBuffer();

      if (cszText != _T("[]#"))
      {
         int nItem = m_cbGenre.FindStringExact(-1, cszText);
         if (nItem == CB_ERR)
            nItem = m_cbGenre.AddString(cszText);

         m_cbGenre.SetCurSel(nItem);
      }

      // limit to actual number of tracks in list
      if (nNumTracks > uMaxCDTracks)
         nNumTracks = uMaxCDTracks;

      // tracks
      CString cszNumTrack;
      for (unsigned int n = 0; n<nNumTracks; n++)
      {
         cszNumTrack.Format(_T("%u"), n);

         ::GetPrivateProfileString(cdplayer_id, cszNumTrack, _T("[]#"), cszText.GetBuffer(512), 512, cszCDPlayerIniFilename);
         cszText.ReleaseBuffer();

         if (cszText != _T("[]#"))
         {
            m_lcTracks.SetItemText(n, 1, cszText);

            if (!bVarious && cszText.Find(_T(" / ")) != -1)
               bVarious = true;
         }
      }

      m_bAcquiredDiscInfo = true;

      return true;
   }

   return false;
}

void InputCDPage::ReadCDText(bool& bVarious)
{
   DWORD nDrive = GetCurrentDrive();

   DWORD uMaxCDTracks = BASS_CD_GetTracks(nDrive);

   const CHAR* cdtext = BASS_CD_GetID(nDrive, BASS_CDID_TEXT);
   if (cdtext != NULL)
   {
      std::vector<CString> vecTitles(uMaxCDTracks + 1);
      std::vector<CString> vecPerformer(uMaxCDTracks + 1);

      CString cszOutput;
      const CHAR* endpos = cdtext;
      do
      {
         while (*endpos++ != 0);

         CString cszText(cdtext);
         if (cdtext == strstr(cdtext, "TITLE"))
         {
            LPSTR pNext = NULL;
            unsigned long uTrack = strtoul(cdtext + 5, &pNext, 10);
            if (uTrack < uMaxCDTracks + 1)
               vecTitles[uTrack] = pNext + 1;
         }
         if (cdtext == strstr(cdtext, "PERFORMER"))
         {
            LPSTR pNext = NULL;
            unsigned long uPerf = strtoul(cdtext + 9, &pNext, 10);
            if (uPerf < uMaxCDTracks + 1)
               vecPerformer[uPerf] = pNext + 1;

            if (uPerf > 0 && strlen(pNext + 1) > 0)
               bVarious = true;
         }

         cdtext = endpos;
      } while (*endpos != 0);

      // set title and artist
      SetDlgItemText(IDC_CDSELECT_EDIT_TITLE, vecTitles[0]);
      SetDlgItemText(IDC_CDSELECT_EDIT_ARTIST, vecPerformer[0]);

      CString cszFormat;
      for (DWORD n = 1; n < uMaxCDTracks + 1; n++)
      {
         if (vecPerformer[n].GetLength() == 0)
            cszFormat = vecTitles[n];
         else
            cszFormat.Format(_T("%s / %s"), vecPerformer[n], vecTitles[n]);

         m_lcTracks.SetItemText(n - 1, 1, cszFormat);
      }

      m_bAcquiredDiscInfo = true;
   }
}

void InputCDPage::CheckCD()
{
   DWORD dwDrive = GetCurrentDrive();
   if (dwDrive == INVALID_DRIVE_ID)
      return;

   // check if current track still plays
   bool bPlaying = BASS_ACTIVE_PLAYING == BASS_CD_Analog_IsActive(dwDrive);
   GetDlgItem(IDC_CDSELECT_BUTTON_STOP).EnableWindow(bPlaying);

   // check for new cd in drive
   DWORD nDrive = GetCurrentDrive();

   bool bIsReady = BASS_CD_IsReady(nDrive) == TRUE;
   if (m_bDriveActive != bIsReady)
   {
      RefreshCDList();
   }
}

void InputCDPage::StoreInCdplayerIni(unsigned int nDrive)
{
#if 0
   if (!m_uiSettings.store_disc_infos_cdplayer_ini)
      return;

   CString cszCDPlayerIniFilename;
   ::GetWindowsDirectory(cszCDPlayerIniFilename.GetBuffer(MAX_PATH), MAX_PATH);
   cszCDPlayerIniFilename.ReleaseBuffer();
   cszCDPlayerIniFilename += _T("\\cdplayer.ini");

   const char* cdplayer_id_raw = BASS_CD_GetID(nDrive, BASS_CDID_CDPLAYER);

   CString cdplayer_id(cdplayer_id_raw);

   CDRipTrackManager* pManager = CDRipTrackManager::getCDRipTrackManager();
   CDRipDiscInfo& discinfo = pManager->GetDiscInfo();

   CString cszFormat;

   // numtracks
   unsigned int nNumTracks = m_lcTracks.GetItemCount();
   cszFormat.Format(_T("%u"), nNumTracks);
   ::WritePrivateProfileString(cdplayer_id, _T("numtracks"), cszFormat, cszCDPlayerIniFilename);

   // artist
   ::WritePrivateProfileString(cdplayer_id, _T("artist"), discinfo.m_cszDiscArtist, cszCDPlayerIniFilename);

   // title
   ::WritePrivateProfileString(cdplayer_id, _T("title"), discinfo.m_cszDiscTitle, cszCDPlayerIniFilename);

   // year
   if (discinfo.m_nYear > 0)
   {
      cszFormat.Format(_T("%u"), discinfo.m_nYear);
      ::WritePrivateProfileString(cdplayer_id, _T("year"), cszFormat, cszCDPlayerIniFilename);
   }

   // genre
   ::WritePrivateProfileString(cdplayer_id, _T("genre"), discinfo.m_cszGenre, cszCDPlayerIniFilename);

   // tracks
   CString cszTrackText;
   for (unsigned int n = 0; n<nNumTracks; n++)
   {
      cszFormat.Format(_T("%u"), n);

      m_lcTracks.GetItemText(n, 1, cszTrackText);

      ::WritePrivateProfileString(cdplayer_id, cszFormat, cszTrackText, cszCDPlayerIniFilename);
   }
#endif
}

void InputCDPage::FreedbLookup()
{
   if (!FreedbResolver::IsAvail())
   {
      AppMessageBox(m_hWnd, IDS_CDRIP_NO_INTERNET_AVAIL, MB_OK | MB_ICONSTOP);
      return;
   }

   DWORD nDrive = GetCurrentDrive();

   const char* cdtext = BASS_CD_GetID(nDrive, BASS_CDID_CDDB);
   if (!cdtext || strlen(cdtext) == 0)
   {
      AppMessageBox(m_hWnd, IDS_CDRIP_ERROR_NOCDINFO, MB_OK | MB_ICONSTOP);
      return;
   }

   CWaitCursor waitCursor;

   FreedbResolver resolver(
      m_uiSettings.freedb_server,
      m_uiSettings.freedb_username);

   CString cszErrorMessage;
   bool bRet = resolver.Lookup(cdtext, cszErrorMessage);

   if (!cszErrorMessage.IsEmpty())
   {
      DWORD dwMbIcon = bRet ? MB_ICONEXCLAMATION : MB_ICONSTOP;
      AppMessageBox(m_hWnd, cszErrorMessage, MB_OK | dwMbIcon);

      return;
   }

   // show dialog when it's more than one item
   unsigned int nSelected = 0;

   if (resolver.Results().size() > 1)
   {
      UI::FreeDbDiscListDlg dlg(resolver.Results());

      waitCursor.Restore();
      ATLVERIFY(IDOK == dlg.DoModal());
      waitCursor.Set();

      // select which one to take
      nSelected = dlg.GetSelectedItem();
   }

   resolver.ReadResultInfo(nSelected);

   FillListFreedbInfo(resolver.CDInfo());

   m_bAcquiredDiscInfo = true;
}

void InputCDPage::FillListFreedbInfo(const Freedb::CDInfo& info)
{
   CString cszText;
   unsigned int nMax = info.tracktitles.size();
   for (unsigned int n = 0; n<nMax; n++)
   {
      cszText = info.tracktitles[n].c_str();
      m_lcTracks.SetItemText(n, 1, cszText);
   }

   cszText = info.dartist.c_str();
   SetDlgItemText(IDC_CDSELECT_EDIT_ARTIST, cszText);

   cszText = info.dtitle.c_str();
   SetDlgItemText(IDC_CDSELECT_EDIT_TITLE, cszText);

   if (info.dyear.size() > 0)
      SetDlgItemInt(IDC_CDSELECT_EDIT_YEAR, strtoul(info.dyear.c_str(), NULL, 10), FALSE);
   else
   {
      unsigned int nPos = info.dextinfo.find("YEAR: ");
      if (-1 != nPos)
      {
         unsigned long nYear = strtoul(info.dextinfo.c_str() + nPos + 6, NULL, 10);
         SetDlgItemInt(IDC_CDSELECT_EDIT_YEAR, nYear, FALSE);
      }
   }

   CString cszGenre(info.dgenre.c_str());
   if (cszGenre.GetLength() > 0)
   {
      int nItem = m_cbGenre.FindStringExact(-1, cszGenre);
      if (nItem == CB_ERR)
         nItem = m_cbGenre.AddString(cszGenre);

      m_cbGenre.SetCurSel(nItem);
   }
}
