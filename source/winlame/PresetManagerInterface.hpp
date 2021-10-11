//
// winLAME - a frontend for the LAME encoding engine
// Copyright (c) 2000-2021 Michael Fink
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
/// \file PresetManagerInterface.hpp
/// \brief PresetManagerInterface is an interface for the preset management
//
#pragma once

#include <string>
#include "SettingsManager.hpp"

/// encoder interface
class PresetManagerInterface
{
public:
   /// loads preset from an xml file
   virtual bool loadPreset(LPCTSTR filename) = 0;

   /// sets currently used facility
   virtual void setFacility(LPCTSTR facilityName) = 0;

   /// returns number of presets
   virtual size_t getPresetCount() = 0;

   /// returns name of preset
   virtual std::tstring getPresetName(size_t index) = 0;

   /// returns preset description
   virtual std::tstring getPresetDescription(size_t index) = 0;

   /// loads the specified settings into the settings manager
   virtual void setSettings(size_t index, SettingsManager& settingsManager) = 0;

   /// sets the default settings for all variables
   virtual void setDefaultSettings(SettingsManager& settingsManager) = 0;

   /// shows the property dialog for a specific preset
   virtual void showPropertyDialog(size_t index) = 0;

   /// dtor
   virtual ~PresetManagerInterface() {}

protected:
   /// ctor
   PresetManagerInterface() {}

   /// copy ctor
   PresetManagerInterface(const PresetManagerInterface&) = delete;

   /// copy assignment operator
   PresetManagerInterface& operator=(const PresetManagerInterface&) = delete;
};
