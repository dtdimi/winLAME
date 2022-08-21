//
// winLAME - a frontend for the LAME encoding engine
// Copyright (c) 2000-2020 Michael Fink
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
/// \file OggVorbisOutputModule.cpp
/// \brief contains the implementation of the ogg vorbis output module
//
#include "stdafx.h"
#include <fstream>
#include "resource.h"
#include "OggVorbisOutputModule.hpp"
#include "OpusOutputModule.hpp"
#include "vorbis/vorbisenc.h"
#include <ctime>
#include <cmath>
#include <ulib/UTF8.hpp>
#include "ChannelRemapper.hpp"

using Encoder::OggVorbisOutputModule;
using Encoder::TrackInfo;
using Encoder::SampleContainer;

extern bool GetRandomNumber(int& randomNumber);

OggVorbisOutputModule::OggVorbisOutputModule()
   :m_bitrateMode(0),
   m_baseQuality(0.0),
   m_endOfStream(false)
{
   m_moduleId = ID_OM_OGGV;

   memset(&m_os, 0, sizeof(m_os));
   memset(&m_og, 0, sizeof(m_og));
   memset(&m_op, 0, sizeof(m_op));
   memset(&m_vi, 0, sizeof(m_vi));
   memset(&m_vc, 0, sizeof(m_vc));
   memset(&m_vd, 0, sizeof(m_vd));
   memset(&m_vb, 0, sizeof(m_vb));
}

OggVorbisOutputModule::~OggVorbisOutputModule()
{
}

bool OggVorbisOutputModule::IsAvailable() const
{
   // we don't do delay-loading anymore, so it's available always
   return true;
}

CString OggVorbisOutputModule::GetDescription() const
{
   CString desc;

   switch (m_bitrateMode)
   {
   case 0: // quality mode
   {
      CString quality;
      quality.Format(_T("%s%u.%02u"),
         m_baseQuality < 0.0 ? _T("-") : _T(""),
         static_cast<unsigned int>(fabs(m_baseQuality*10.0)),
         static_cast<unsigned int>((fabs(m_baseQuality*10.0) - static_cast<unsigned int>(fabs(m_baseQuality*10.0)))*100.0));

      desc.Format(IDS_FORMAT_INFO_OGGV_OUTPUT_QUALITY,
         quality.GetString(), m_vi.rate, m_vi.channels);
   }
   break;

   case 1: // variable bitrate
      desc.Format(IDS_FORMAT_INFO_OGGV_OUTPUT_VBR,
         m_vi.bitrate_lower / 1000, m_vi.bitrate_upper / 1000, m_vi.rate, m_vi.channels);
      break;

   case 2: // average bitrate
      desc.Format(IDS_FORMAT_INFO_OGGV_OUTPUT_ABR,
         m_vi.bitrate_nominal / 1000, m_vi.rate, m_vi.channels);
      break;

   case 3: // constant bitrate
      desc.Format(IDS_FORMAT_INFO_OGGV_OUTPUT_CBR,
         m_vi.bitrate_nominal / 1000, m_vi.rate, m_vi.channels);
      break;

   default:
      ATLASSERT(false);
      break;
   }

   return desc;
}

/// returns the Ogg Vorbis library version
CString GetOggVorbisVersionString()
{
   // to get build version, create first 3 ogg packets,
   // take the second and read it back in; the vc.vendor
   // then is filled with build version
   vorbis_info vi;
   vorbis_info_init(&vi);
   vorbis_encode_init(&vi, 2, 44100, -1, 192000, -1);

   vorbis_block vb;
   vorbis_dsp_state vd;
   vorbis_analysis_init(&vd, &vi);
   vorbis_block_init(&vd, &vb);

   vorbis_comment vc;
   vorbis_comment_init(&vc);

   ogg_packet header;
   ogg_packet header_comm;
   ogg_packet header_code;

   vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);

   vorbis_synthesis_headerin(&vi, &vc, &header_comm);

   CString version;
   version.Format(_T("(%hs)"), vc.vendor);

   vorbis_block_clear(&vb);
   vorbis_dsp_clear(&vd);
   vorbis_comment_clear(&vc);
   vorbis_info_clear(&vi);

   return version;
}

void OggVorbisOutputModule::GetVersionString(CString& version, int special) const
{
   if (!IsAvailable())
      return;

   version = GetOggVorbisVersionString();
}

int OggVorbisOutputModule::InitOutput(LPCTSTR outfilename,
   SettingsManager& mgr, const TrackInfo& trackInfo,
   SampleContainer& samples)
{
   IsAvailable();

   m_baseQuality = 1.0;
   m_endOfStream = false;

   m_channels = samples.GetInputModuleChannels();
   m_samplerate = samples.GetInputModuleSampleRate();

   m_outputStream.open(CStringA(outfilename).GetString(), std::ios::out | std::ios::binary);
   if (!m_outputStream.is_open())
   {
      m_lastError.LoadString(IDS_ENCODER_OUTPUT_FILE_CREATE_ERROR);
      return -1;
   }

   int ret = InitVorbisInfo(mgr);
   if (ret < 0)
      return ret;

   AddTrackInfo(trackInfo);

   InitEncoder();

   WriteHeader();

   samples.SetOutputModuleTraits(16, SamplesChannelArray, m_samplerate, m_channels);

   return 0;
}

int OggVorbisOutputModule::InitVorbisInfo(SettingsManager& mgr)
{
   vorbis_info_init(&m_vi);

   int ret = 0;

   switch (m_bitrateMode = mgr.QueryValueInt(OggBitrateMode))
   {
   case 0: // base quality mode
      m_baseQuality = mgr.QueryValueInt(OggBaseQuality) / 1000.f;
      ret = vorbis_encode_init_vbr(&m_vi, m_channels, m_samplerate, m_baseQuality);
      break;

   case 1: // variable bitrate mode
   {
      int min_bitrate = mgr.QueryValueInt(OggVarMinBitrate);
      int max_bitrate = mgr.QueryValueInt(OggVarMaxBitrate);
      ret = vorbis_encode_init(&m_vi, m_channels, m_samplerate,
         max_bitrate * 1000, -1, min_bitrate * 1000);
   }
   break;

   case 2: // average bitrate mode
   {
      int average_bitrate = mgr.QueryValueInt(OggVarNominalBitrate);
      ret = vorbis_encode_init(&m_vi, m_channels, m_samplerate,
         -1, average_bitrate * 1000, -1);
   }
   break;

   case 3: // constant bitrate mode
   {
      int const_bitrate = mgr.QueryValueInt(OggVarNominalBitrate);
      ret = vorbis_encode_init(&m_vi, m_channels, m_samplerate,
         const_bitrate * 1000, const_bitrate * 1000, const_bitrate * 1000);
   }
   break;

   default:
      ATLASSERT(false);
      break;
   }

   if (ret < 0)
   {
      m_lastError.LoadString(IDS_ENCODER_ERROR_INIT_ENCODER);
      m_lastError += _T(" (");

      switch (ret)
      {
      case OV_EIMPL:
         m_lastError += _T("Unimplemented mode; unable to comply with quality level request.");
         break;

      case OV_EINVAL:
         m_lastError += _T("Invalid setup request, e.g., out of range argument.");
         break;

      case OV_EFAULT:
         m_lastError += _T("Internal logic fault; indicates a bug or heap/stack corruption.");
         break;

      default:
         ATLASSERT(false);
         break;
      }

      m_lastError += _T(")");

      vorbis_info_clear(&m_vi);

      return ret;
   }

   return ret;
}

void OggVorbisOutputModule::AddTrackInfo(const TrackInfo& trackInfo)
{
   vorbis_comment_init(&m_vc);

   std::vector<char> buffer;

   bool avail = false;
   CString text = trackInfo.GetTextInfo(TrackInfoTitle, avail);
   if (avail && !text.IsEmpty())
   {
      // note: ogg vorbis tag values are stored as UTF-8, so convert here
      StringToUTF8(text, buffer);
      vorbis_comment_add_tag(&m_vc, "TITLE", buffer.data());
   }

   text = trackInfo.GetTextInfo(TrackInfoArtist, avail);
   if (avail && !text.IsEmpty())
   {
      StringToUTF8(text, buffer);
      vorbis_comment_add_tag(&m_vc, "ARTIST", buffer.data());
   }

   text = trackInfo.GetTextInfo(TrackInfoDiscArtist, avail);
   if (avail && !text.IsEmpty())
   {
      StringToUTF8(text, buffer);
      vorbis_comment_add_tag(&m_vc, "ALBUMARTIST", buffer.data());
   }

   text = trackInfo.GetTextInfo(TrackInfoComposer, avail);
   if (avail && !text.IsEmpty())
   {
      StringToUTF8(text, buffer);
      vorbis_comment_add_tag(&m_vc, "COMPOSER", buffer.data());
   }

   text = trackInfo.GetTextInfo(TrackInfoAlbum, avail);
   if (avail && !text.IsEmpty())
   {
      StringToUTF8(text, buffer);
      vorbis_comment_add_tag(&m_vc, "ALBUM", buffer.data());
   }

   int year = trackInfo.GetNumberInfo(TrackInfoYear, avail);
   if (avail && year > 0)
   {
      text.Format(_T("%i"), year);
      StringToUTF8(text, buffer);
      vorbis_comment_add_tag(&m_vc, "DATE", buffer.data());
   }

   text = trackInfo.GetTextInfo(TrackInfoComment, avail);
   if (avail && !text.IsEmpty())
   {
      StringToUTF8(text, buffer);
      vorbis_comment_add_tag(&m_vc, "COMMENT", buffer.data());
   }

   int trackNumber = trackInfo.GetNumberInfo(TrackInfoTrack, avail);
   if (avail && trackNumber >= 0)
   {
      text.Format(_T("%i"), trackNumber);
      StringToUTF8(text, buffer);
      vorbis_comment_add_tag(&m_vc, "TRACKNUMBER", buffer.data());
   }

   int discNumber = trackInfo.GetNumberInfo(TrackInfoDiscNumber, avail);
   if (avail && discNumber >= 0)
   {
      text.Format(_T("%i"), discNumber);
      StringToUTF8(text, buffer);
      vorbis_comment_add_tag(&m_vc, "DISCNUMBER", buffer.data());
   }

   text = trackInfo.GetTextInfo(TrackInfoGenre, avail);
   if (avail && !text.IsEmpty())
   {
      StringToUTF8(text, buffer);
      vorbis_comment_add_tag(&m_vc, "GENRE", buffer.data());
   }

   std::vector<unsigned char> binaryInfo;
   avail = trackInfo.GetBinaryInfo(TrackInfoFrontCover, binaryInfo);
   if (avail)
   {
      std::string pictureData = OpusOutputModule::GetMetadataBlockPicture(binaryInfo);

      if (!pictureData.empty())
         vorbis_comment_add_tag(&m_vc, "METADATA_BLOCK_PICTURE", pictureData.c_str());
   }
}

void OggVorbisOutputModule::InitEncoder()
{
   // set up the analysis state and auxiliary encoding storage
   vorbis_analysis_init(&m_vd, &m_vi);
   vorbis_block_init(&m_vd, &m_vb);

   // set up our packet->stream encoder
   // pick a random serial number; that way we can more likely build
   // chained streams just by concatenation
   int randomNumber = 0;
   if (!GetRandomNumber(randomNumber))
      return;

   ogg_stream_init(&m_os, randomNumber);
}

void OggVorbisOutputModule::WriteHeader()
{
   // Vorbis streams begin with three headers; the initial header (with
   // most of the codec setup parameters) which is mandated by the Ogg
   // bitstream spec.  The second header holds any comment fields.  The
   // third header holds the bitstream codebook.  We merely need to
   // make the headers, then pass them to libvorbis one at a time;
   // libvorbis handles the additional Ogg bitstream constraints
   ogg_packet header;
   ogg_packet header_comm;
   ogg_packet header_code;

   vorbis_analysis_headerout(&m_vd, &m_vc, &header, &header_comm, &header_code);

   // automatically placed in its own page
   ogg_stream_packetin(&m_os, &header);

   ogg_stream_packetin(&m_os, &header_comm);
   ogg_stream_packetin(&m_os, &header_code);

   // We don't have to write out here, but doing so makes streaming
   // much easier, so we do, flushing ALL pages. This ensures the actual
   // audio data will start on a new page
   while (!m_endOfStream)
   {
      int result = ogg_stream_flush(&m_os, &m_og);
      if (result == 0)
         break;

      m_outputStream.write(reinterpret_cast<char*>(m_og.header), m_og.header_len);
      m_outputStream.write(reinterpret_cast<char*>(m_og.body), m_og.body_len);
   }
}

int OggVorbisOutputModule::EncodeSamples(SampleContainer& samples)
{
   if (m_endOfStream)
      return 0;

   // get samples
   int numSamples = 0;
   short** buffer = (short**)samples.GetSamplesArray(numSamples);

   if (numSamples != 0)
   {
      float** sampleBuffer = vorbis_analysis_buffer(&m_vd, numSamples);

      // copy samples to analysis buffer
      if (m_channels > 2)
      {
         ChannelRemapper::RemapArrayToFloat(T_enChannelMapType::oggVorbisOutputChannelMap,
            buffer, numSamples, m_channels, sampleBuffer);
      }
      else
      {
         ChannelRemapper::ConvertSamplesToFloat(buffer, numSamples, m_channels, sampleBuffer);
      }
   }

   // tell the library how much we actually submitted
   vorbis_analysis_wrote(&m_vd, numSamples);

   WriteBlocks();

   return numSamples;
}

void OggVorbisOutputModule::WriteBlocks()
{
   // vorbis does some data preanalysis, then divides up blocks for
   // more involved (potentially parallel) processing.  Get a single
   // block for encoding now
   while (vorbis_analysis_blockout(&m_vd, &m_vb) == 1)
   {
      // analysis, assume we want to use bitrate management
      vorbis_analysis(&m_vb, &m_op);
      vorbis_bitrate_addblock(&m_vb);

      while (vorbis_bitrate_flushpacket(&m_vd, &m_op))
      {
         // weld the packet into the bitstream
         ogg_stream_packetin(&m_os, &m_op);

         // write out pages (if any)
         while (!m_endOfStream)
         {
            int result = ogg_stream_pageout(&m_os, &m_og);
            if (result == 0)
               break;

            m_outputStream.write(reinterpret_cast<char*>(m_og.header), m_og.header_len);
            m_outputStream.write(reinterpret_cast<char*>(m_og.body), m_og.body_len);

            // this could be set above, but for illustrative purposes, I do
            // it here (to show that vorbis does know where the stream ends)
            if (ogg_page_eos(&m_og))
               m_endOfStream = true;
         }
      }
   }
}

void OggVorbisOutputModule::DoneOutput()
{
   if (!m_lastError.IsEmpty())
      return;

   vorbis_analysis_wrote(&m_vd, 0);

   // put out the last ogg blocks
   WriteBlocks();

   // clean up and exit.  vorbis_info_clear() must be called last
   ogg_stream_clear(&m_os);
   vorbis_block_clear(&m_vb);
   vorbis_dsp_clear(&m_vd);
   vorbis_comment_clear(&m_vc);
   vorbis_info_clear(&m_vi);

   // ogg_page and ogg_packet structs always point to storage in
   // libvorbis.  They're never freed or manipulated directly

   m_outputStream.close();
}
