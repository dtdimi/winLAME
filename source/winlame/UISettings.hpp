//
// winLAME - a frontend for the LAME encoding engine
// Copyright (c) 2000-2007 Michael Fink
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
/// \file UISettings.hpp
/// \brief UI settings class
//
#pragma once

#include "SettingsManager.hpp"
#include "PresetManagerInterface.hpp"
#include "EncoderInterface.hpp"
#include "CDReadJob.hpp"
#include "TaskManagerConfig.hpp"

namespace Encoder
{
   class ModuleManager;
}

/// list of filenames
typedef std::vector<Encoder::EncoderJob> EncoderJobList;

/// \brief settings for encoding a file
/// \details encoding settings may differ between two files, so this struct
/// collects all those settings.
struct EncodingSettings
{
   /// ctor
   EncodingSettings();

   /// output directory
   CString outputdir;

   /// indicates if source files should be deleted after encoding
   bool delete_after_encode;

   /// indicates if existing files will be overwritten
   bool overwrite_existing;
};

/// general UI settings
struct UISettings
{
   /// ctor
   UISettings();

   /// reads settings from the registry
   void ReadSettings();

   /// stores settings in the registry
   void StoreSettings();

   /// list of encoder jobs
   EncoderJobList encoderjoblist;

   /// list of CD read jobs
   std::vector<Encoder::CDReadJob> cdreadjoblist;

   /// default encoding settings
   EncodingSettings m_defaultSettings;

   /// indicates if OutputSettingsPage was called from InputFilesPage (true)
   /// or InputCDPage (false)
   bool m_bFromInputFilesPage;

   /// output directory history list
   std::vector<CString> outputhistory;

   /// last input files folder
   CString lastinputpath;

   /// indicates if an output playlist should be created
   bool create_playlist;

   /// playlist filename
   CString playlist_filename;

   /// action to perform after all files were encoded
   int after_encoding_action;

   /// last selected output module id
   size_t output_module;

   /// use input dir as output location
   bool out_location_use_input_dir;

   /// indicates if presets are available
   bool preset_avail;

   /// current filename of presets.xml file
   CString presets_filename;

   /// last selected preset index
   int m_iLastSelectedPresetIndex;

   /// indicates that last page was cdrip page
   bool last_page_was_cdrip_page;

   /// temporary folder for cd ripping
   CString cdrip_temp_folder;

   /// freedb servername
   CString freedb_server;

   /// indicates if disc infos retrieved by freedb should be stored in cdplayer.ini
   bool store_disc_infos_cdplayer_ini;

   /// indicates if disc tray is opened and/or CD is ejected
   bool m_ejectDiscAfterReading = false;

   /// format string for various track
   CString cdrip_format_various_track;

   /// format string for album track
   CString cdrip_format_album_track;

   /// language id
   UINT language_id;

   /// application mode
   enum ApplicationMode
   {
      classicMode = 0, ///< run classic mode
      modernMode = 1, ///< run modern mode
   };

   /// current application mode
   ApplicationMode m_appMode;

   /// settings manager
   SettingsManager settings_manager;

   /// configuration for task manager
   TaskManagerConfig m_taskManagerConfig;
};
