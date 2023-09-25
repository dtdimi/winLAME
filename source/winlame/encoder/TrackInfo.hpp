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
/// \file TrackInfo.hpp
/// \brief manages informations and properties of a track to encode
/// \details id3 tags can be passed in and can be generated from the properties
//
#pragma once

#include <string>
#include <map>

namespace Encoder
{
   /// track text info enum
   enum TrackInfoTextType
   {
      TrackInfoTitle = 0,  ///< title
      TrackInfoArtist,     ///< track artist
      TrackInfoDiscArtist, ///< disc artist
      TrackInfoAlbum,      ///< album name
      TrackInfoComment,    ///< track comment
      TrackInfoGenre,      ///< track genre
      TrackInfoComposer,   ///< track composer
   };

   /// track number info enum
   enum TrackInfoNumberType
   {
      TrackInfoYear = 0,   ///< year of track
      TrackInfoTrack,      ///< track number
      TrackInfoDiscNumber, ///< disc number
   };

   /// track binary info enum
   enum TrackInfoBinaryType
   {
      TrackInfoFrontCover = 0,   ///< front cover art, in JPEG format
   };

   /// track info class
   class TrackInfo
   {
   public:
      /// ctor
      TrackInfo()
      {
      }

      /// resets all infos
      void ResetInfos()
      {
         m_mapTextInfos.clear();
         m_mapNumberInfos.clear();
         m_mapBinaryInfos.clear();
      }

      /// sets a text info value
      void SetTextInfo(TrackInfoTextType type, CString value)
      {
         m_mapTextInfos[type] = value;
      }

      /// retrieves a text info value
      CString GetTextInfo(TrackInfoTextType type, bool& avail) const
      {
         std::map<TrackInfoTextType, CString>::const_iterator iter = m_mapTextInfos.find(type);
         avail = iter != m_mapTextInfos.end();
         return avail ? iter->second : CString();
      }

      /// sets a number info value
      void SetNumberInfo(TrackInfoNumberType type, int value)
      {
         m_mapNumberInfos[type] = value;
      }

      /// retrieves a number info value
      int GetNumberInfo(TrackInfoNumberType type, bool& avail) const
      {
         auto iter = m_mapNumberInfos.find(type);
         avail = iter != m_mapNumberInfos.end();
         return avail ? iter->second : -1;
      }

      /// sets a binary info value
      void SetBinaryInfo(TrackInfoBinaryType type, const std::vector<unsigned char>& value)
      {
         m_mapBinaryInfos[type] = value;
      }

      /// retrieves a binary info value
      bool GetBinaryInfo(TrackInfoBinaryType type, std::vector<unsigned char>& binaryInfo) const
      {
         auto iter = m_mapBinaryInfos.find(type);
         bool avail = iter != m_mapBinaryInfos.end();

         if (avail)
            binaryInfo.assign(iter->second.begin(), iter->second.end());

         return avail;
      }

      /// returns if track info is empty
      bool IsEmpty() const
      {
         return m_mapTextInfos.empty() &&
            m_mapNumberInfos.empty() &&
            m_mapBinaryInfos.empty();
      }

      /// converts genre ID to text
      static CString GenreIDToText(unsigned int genreID);

      /// converts genre text to ID
      static unsigned char TextToGenreID(const CString& text);

      /// returns genre list
      static std::vector<CString> GetGenreList();

   private:
      /// text infos map
      std::map<TrackInfoTextType, CString> m_mapTextInfos;

      /// number infos map
      std::map<TrackInfoNumberType, int> m_mapNumberInfos;

      /// binary infos map
      std::map<TrackInfoBinaryType, std::vector<unsigned char>> m_mapBinaryInfos;
   };

} // namespace Encoder
