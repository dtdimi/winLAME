//
// winLAME - a frontend for the LAME encoding engine
// Copyright (c) 2000-2018 Michael Fink
// Copyright (c) 2004 DeXT
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
/// \file UISettings.cpp
/// \brief contains functions to read and store the general UI settings in the registry
//
#include "stdafx.h"
#include "UISettings.hpp"
#include <stdio.h>
#include <sys/stat.h>

// constants

/// registry root path
LPCTSTR g_pszRegistryRoot = _T("Software\\winLAME");

LPCTSTR g_pszOutputPath = _T("OutputPath");
LPCTSTR g_pszOutputModule = _T("OutputModule");
LPCTSTR g_pszInputOutputSameFolder = _T("InputOutputSameFolder");
LPCTSTR g_pszLastInputPath = _T("LastInputPath");
LPCTSTR g_pszDeleteAfterEncode = _T("DeleteAfterEncode");
LPCTSTR g_pszOverwriteExisting = _T("OverwriteExisting");
LPCTSTR g_pszActionAfterEncoding = _T("ActionAfterEncoding");
LPCTSTR g_pszEjectDiscAfterReading = _T("EjectDiscAfterReading");
LPCTSTR g_pszLastSelectedPresetIndex = _T("LastSelectedPresetIndex");
LPCTSTR g_pszCdripTempFolder = _T("CDExtractTempFolder");
LPCTSTR g_pszOutputPathHistory = _T("OutputPathHistory%02zu");
LPCTSTR g_pszFreedbServer = _T("FreedbServer");
LPCTSTR g_pszDiscInfosCdplayerIni = _T("StoreDiscInfosInCdplayerIni");
LPCTSTR g_pszFormatVariousTrack = _T("CDExtractFormatVariousTrack");
LPCTSTR g_pszFormatAlbumTrack = _T("CDExtractFormatAlbumTrack");
LPCTSTR g_pszLanguageId = _T("LanguageId");
LPCTSTR g_pszAppMode = _T("AppMode");
LPCTSTR g_pszAutoTasksPerCpu = _T("TaskManagerAutoTasksPerCPU");
LPCTSTR g_pszUseNumTasks = _T("TaskManagerUseNumTasks");


// EncodingSettings methods

EncodingSettings::EncodingSettings()
   :delete_after_encode(false),
   overwrite_existing(true)
{
}


// UISettings methods

UISettings::UISettings()
   :output_module(0),
   out_location_use_input_dir(false),
   preset_avail(false),
   m_bFromInputFilesPage(true),
   after_encoding_action(-1),
   create_playlist(false),
   playlist_filename(MAKEINTRESOURCE(IDS_GENERAL_PLAYLIST_FILENAME)),
   m_iLastSelectedPresetIndex(1), // first preset is the "best practice" preset
   last_page_was_cdrip_page(false),
   cdrip_temp_folder(Path::TempFolder()),
   freedb_server(_T("gnudb.gnudb.org")),
   store_disc_infos_cdplayer_ini(true),
   cdrip_format_various_track(_T("%track% - %album% - %artist% - %title%")),
   cdrip_format_album_track(_T("%track% - %albumartist% - %album% - %title%")),
   language_id(MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT)),
   m_appMode(modernMode)
{
}

#pragma warning(push)
#pragma warning(disable: 4996) // 'ATL::CRegKey::QueryValue': CRegKey::QueryValue(TCHAR *value, TCHAR *valueName) has been superseded by CRegKey::QueryStringValue and CRegKey::QueryMultiStringValue

/// reads string value from registry
void ReadStringValue(CRegKey& regKey, LPCTSTR pszName, UINT uiMaxLength, CString& cszValue)
{
   std::vector<TCHAR> buffer(uiMaxLength, 0);
   DWORD count = uiMaxLength;
   if (ERROR_SUCCESS == regKey.QueryValue(&buffer[0], pszName, &count))
      cszValue = &buffer[0];
}

/// reads int value from registry
void ReadIntValue(CRegKey& regKey, LPCTSTR pszName, int& iValue)
{
   DWORD value = 0;
   if (ERROR_SUCCESS == regKey.QueryValue(value, pszName))
      iValue = static_cast<int>(value);
}

/// reads unsigned int value from registry
void ReadUIntValue(CRegKey& regKey, LPCTSTR pszName, UINT& uiValue)
{
   DWORD value = 0;
   if (ERROR_SUCCESS == regKey.QueryValue(value, pszName))
      uiValue = value;
}

/// reads boolean value from registry
void ReadBooleanValue(CRegKey& regKey, LPCTSTR pszName, bool& bValue)
{
   DWORD value;
   if (ERROR_SUCCESS == regKey.QueryValue(value, pszName))
      bValue = value == 1;
}
#pragma warning(pop)

void UISettings::ReadSettings()
{
   // open root key
   CRegKey regRoot;
   if (ERROR_SUCCESS != regRoot.Open(HKEY_CURRENT_USER, g_pszRegistryRoot, KEY_READ))
      return;

   // read output path
   ReadStringValue(regRoot, g_pszOutputPath, MAX_PATH, m_defaultSettings.outputdir);

   // read last input path
   CString cszLastInputPath;
   ReadStringValue(regRoot, g_pszLastInputPath, MAX_PATH, cszLastInputPath);
   if (!cszLastInputPath.IsEmpty())
      lastinputpath = cszLastInputPath;

   // read "output module" value
   UINT tempOutputModule = 0;
   ReadUIntValue(regRoot, g_pszOutputModule, tempOutputModule);
   output_module = tempOutputModule;

   // read "use input file's folder as output location" value
   ReadBooleanValue(regRoot, g_pszInputOutputSameFolder, out_location_use_input_dir);

   // read "delete after encode" value
   ReadBooleanValue(regRoot, g_pszDeleteAfterEncode, m_defaultSettings.delete_after_encode);

   // read "overwrite existing" value
   ReadBooleanValue(regRoot, g_pszOverwriteExisting, m_defaultSettings.overwrite_existing);

   // read "action after encoding" value
   ReadIntValue(regRoot, g_pszActionAfterEncoding, after_encoding_action);

   // read last selected preset index
   ReadIntValue(regRoot, g_pszLastSelectedPresetIndex, m_iLastSelectedPresetIndex);

   // read "cd extraction temp folder"
   ReadStringValue(regRoot, g_pszCdripTempFolder, MAX_PATH, cdrip_temp_folder);

   // read "freedb server"
   ReadStringValue(regRoot, g_pszFreedbServer, MAX_PATH, freedb_server);

   // read "store disc infos in cdplayer.ini" value
   ReadBooleanValue(regRoot, g_pszDiscInfosCdplayerIni, store_disc_infos_cdplayer_ini);

   // read "format various track" / "format album track"
   ReadStringValue(regRoot, g_pszFormatVariousTrack, MAX_PATH, cdrip_format_various_track);
   ReadStringValue(regRoot, g_pszFormatAlbumTrack, MAX_PATH, cdrip_format_album_track);

   // read "eject disc after reading
   ReadBooleanValue(regRoot, g_pszEjectDiscAfterReading, m_ejectDiscAfterReading);

   // read "language id" value
   ReadUIntValue(regRoot, g_pszLanguageId, language_id);

   // read "app mode" value
   UINT appMode = 1;
   ReadUIntValue(regRoot, g_pszAppMode, appMode);
   m_appMode = static_cast<ApplicationMode>(appMode);

   if (m_appMode != classicMode &&
      m_appMode != modernMode)
   {
      m_appMode = modernMode;
   }

   // read "output path history" entries
   outputhistory.clear();

   CString histkey, histentry;
   for (int i = 0; i < 10; i++)
   {
      histkey.Format(g_pszOutputPathHistory, i);

      histentry.Empty();
      ReadStringValue(regRoot, histkey, MAX_PATH, histentry);

      if (!histentry.IsEmpty())
      {
         // remove last slash
         histentry.TrimRight(_T('\\'));

         // check if path exists
         DWORD dwAttr = ::GetFileAttributes(histentry);
         if (INVALID_FILE_ATTRIBUTES != dwAttr && (dwAttr & FILE_ATTRIBUTE_DIRECTORY) != 0)
            outputhistory.push_back(histentry + _T("\\"));
      }
   }

   // read task manager config
   ReadBooleanValue(regRoot, g_pszAutoTasksPerCpu, m_taskManagerConfig.m_bAutoTasksPerCpu);

   UINT numCpuCores = 0;
   ReadUIntValue(regRoot, g_pszUseNumTasks, numCpuCores);
   m_taskManagerConfig.m_uiUseNumTasks = numCpuCores;

   regRoot.Close();
}

void UISettings::StoreSettings()
{
   // open root key
   CRegKey regRoot;
   if (ERROR_SUCCESS != regRoot.Open(HKEY_CURRENT_USER, g_pszRegistryRoot))
   {
      // try to create key
      if (ERROR_SUCCESS != regRoot.Create(HKEY_CURRENT_USER, g_pszRegistryRoot))
         return;
   }

#pragma warning(push)
#pragma warning(disable: 4996) // 'ATL::CRegKey::QueryValue': CRegKey::QueryValue(TCHAR *value, TCHAR *valueName) has been superseded by CRegKey::QueryStringValue and CRegKey::QueryMultiStringValue

   // write output path
   regRoot.SetValue(m_defaultSettings.outputdir, g_pszOutputPath);

   // write last input path
   regRoot.SetValue(lastinputpath, g_pszLastInputPath);

   // write "output module" value
   regRoot.SetValue(static_cast<DWORD>(output_module), g_pszOutputModule);

   // write "use input file's folder as output location" value
   DWORD value = out_location_use_input_dir ? 1 : 0;
   regRoot.SetValue(value, g_pszInputOutputSameFolder);

   // write "delete after encode" value
   value = m_defaultSettings.delete_after_encode ? 1 : 0;
   regRoot.SetValue(value, g_pszDeleteAfterEncode);

   // write "overwrite existing" value
   value = m_defaultSettings.overwrite_existing ? 1 : 0;
   regRoot.SetValue(value, g_pszOverwriteExisting);

   // write "action after encoding" value
   value = after_encoding_action;
   regRoot.SetValue(value, g_pszActionAfterEncoding);

   // write "eject disc after reading" value
   value = m_ejectDiscAfterReading ? 1 : 0;
   regRoot.SetValue(value, g_pszEjectDiscAfterReading);

   // write last selected preset index
   regRoot.SetValue(m_iLastSelectedPresetIndex, g_pszLastSelectedPresetIndex);

   // write cd extraction temp folder
   regRoot.SetValue(cdrip_temp_folder, g_pszCdripTempFolder);

   // write freedb server
   regRoot.SetValue(freedb_server, g_pszFreedbServer);

   // write "store disc infos in cdplayer.ini" value
   value = store_disc_infos_cdplayer_ini ? 1 : 0;
   regRoot.SetValue(value, g_pszDiscInfosCdplayerIni);

   // write "format various track" / "format album track"
   regRoot.SetValue(cdrip_format_various_track, g_pszFormatVariousTrack);
   regRoot.SetValue(cdrip_format_album_track, g_pszFormatAlbumTrack);

   // write "language id" value
   value = language_id;
   regRoot.SetValue(value, g_pszLanguageId);

   // write "app mode" value
   value = (DWORD)m_appMode;
   regRoot.SetValue(value, g_pszAppMode);

   // store "output path history" entries
   CString buffer;
   size_t i, max = outputhistory.size() > 10 ? 10 : outputhistory.size();
   for (i = 0; i < max; i++)
   {
      buffer.Format(g_pszOutputPathHistory, i);
      regRoot.SetValue(outputhistory[i], buffer);
   }

   // delete the rest of the entries
   for (i = max; i < 10; i++)
   {
      buffer.Format(g_pszOutputPathHistory, i);
      regRoot.DeleteValue(buffer);
   }

   // read task manager config
   value = m_taskManagerConfig.m_bAutoTasksPerCpu ? 1 : 0;
   regRoot.SetValue(value, g_pszAutoTasksPerCpu);

   value = m_taskManagerConfig.m_uiUseNumTasks;
   regRoot.SetValue(value, g_pszUseNumTasks);
#pragma warning(pop)

   regRoot.Close();
}
