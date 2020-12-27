//
// winLAME - a frontend for the LAME encoding engine
// Copyright (c) 2018 Michael Fink
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
/// \file LibMpg123InputModule.cpp
/// \brief libmpg123 input module
//
#include "stdafx.h"
#include "LibMpg123InputModule.hpp"
#include "Id3v1Tag.hpp"
#include "AudioFileTag.hpp"
#include "resource.h"

using Encoder::LibMpg123InputModule;
using Encoder::TrackInfo;
using Encoder::SampleContainer;

/// flag to initialize libmpg123 library only once
static std::once_flag s_libmpg123init;

#pragma comment(lib, "libmpg123-0.lib")

LibMpg123InputModule::LibMpg123InputModule()
:m_isAtEndOfFile(false),
m_fileSize(0L)
{
   std::call_once(s_libmpg123init, []() { mpg123_init(); });

   m_moduleId = ID_IM_LIBMPG123;
}

Encoder::InputModule* LibMpg123InputModule::CloneModule()
{
   return new LibMpg123InputModule;
}

bool LibMpg123InputModule::IsAvailable() const
{
   // it's always available
   return true;
}

CString LibMpg123InputModule::GetDescription() const
{
   CString desc;

   if (m_decoder == nullptr)
      return desc;

   mpg123_frameinfo frameInfo;
   int ret = mpg123_info(m_decoder.get(), &frameInfo);
   if (ret != MPG123_OK)
      return desc;

   LPCTSTR mpegVersion = _T("?");
   switch (frameInfo.version)
   {
   case MPG123_1_0: mpegVersion = _T("1"); break;
   case MPG123_2_0: mpegVersion = _T("2"); break;
   case MPG123_2_5: mpegVersion = _T("2.5"); break;
   default: ATLASSERT(false); break;
   }

   // special options; empty for now
   LPCTSTR option = _T("");

   LPCTSTR stereo = _T("???");
   switch (frameInfo.mode)
   {
   case MPG123_M_STEREO: stereo = _T("Stereo"); break;
   case MPG123_M_DUAL: stereo = _T("Dual Channel Stereo"); break;
   case MPG123_M_MONO: stereo = _T("Mono"); break;
   case MPG123_M_JOINT: stereo = _T("Joint Stereo"); break;
   default: ATLASSERT(false); break;
   }

   if (frameInfo.mode == MPG123_M_JOINT && frameInfo.layer == 3)
   {
      // libmpg123 doesn't define these, so do it here
      const int MPG123_M_EXT_I_STERERO = 0x0100; // Layer 3: Intensity stereo
      const int MPG123_M_EXT_MS_STEREO = 0x0200; // Layer 3: Mid/Side stereo

      if (frameInfo.mode_ext & (MPG123_M_EXT_I_STERERO | MPG123_M_EXT_MS_STEREO))
         stereo = _T("Joint Stereo (Mixed)");
      else if (frameInfo.mode_ext & MPG123_M_EXT_I_STERERO)
         stereo = _T("Joint Stereo (Intensity)");
      else if (frameInfo.mode_ext & MPG123_M_EXT_MS_STEREO)
         stereo = _T("Joint Stereo (Mid/Side)");
   }

   desc.Format(IDS_FORMAT_INFO_MPG123_INPUT,
      mpegVersion,
      frameInfo.layer,
      frameInfo.bitrate,
      frameInfo.rate,
      stereo,
      option);

   return desc;
}

void LibMpg123InputModule::GetVersionString(CString& version, int special) const
{
   UNUSED(special);

   // there is no API to get version, so just return fixed value
   version = _T("mpg123-1.25.10");
}

CString LibMpg123InputModule::GetFilterString() const
{
   CString filterString;
   filterString.LoadString(IDS_FILTER_LIBMPG123_INPUT);
   return filterString;
}

int LibMpg123InputModule::InitInput(LPCTSTR infilename, SettingsManager& mgr,
   TrackInfo& trackInfo, SampleContainer& samples)
{
   int errorCode = 0;
   mpg123_handle* handle = mpg123_new(nullptr, &errorCode);
   if (handle == nullptr || errorCode != MPG123_OK)
   {
      m_lastError.LoadString(IDS_ENCODER_ERROR_INIT_DECODER);
      m_lastError.AppendFormat(_T(" (%hs)"), mpg123_plain_strerror(errorCode));
      return -1;
   }

   m_decoder.reset(handle, mpg123_delete);

   FILE* fd = nullptr;
   errno_t err = _tfopen_s(&fd, infilename, _T("rb"));

   if (err != 0 || fd == nullptr)
   {
      m_lastError.LoadString(IDS_ENCODER_INPUT_FILE_OPEN_ERROR);
      return -1;
   }

   m_inputFile.reset(fd, fclose);

   if (!GetFileSize())
      return -1;

   GetTrackInfo(infilename, trackInfo);

   if (!OpenStream())
      return -1;

   if (!SetupDecoder())
      return -1;

   if (!SetFormat(samples))
      return -1;

   return 0;
}

void LibMpg123InputModule::GetInfo(int& numChannels, int& bitrateInBps, int& lengthInSeconds, int& samplerateInHz) const
{
   if (m_decoder == nullptr)
      return;

   mpg123_frameinfo frameInfo;
   int ret = mpg123_info(m_decoder.get(), &frameInfo);
   if (ret != MPG123_OK)
      return;

   bitrateInBps = frameInfo.bitrate * 1000;
   samplerateInHz = frameInfo.rate;
   numChannels = frameInfo.mode == MPG123_M_MONO ? 1 : 2;

   off_t numTotalSamples = mpg123_length(m_decoder.get());

   lengthInSeconds = numTotalSamples / samplerateInHz;
}

int LibMpg123InputModule::DecodeSamples(SampleContainer& samples)
{
   unsigned char sampleBuffer[32768];

   size_t bytesWritten = 0;
   int ret = mpg123_read(m_decoder.get(), sampleBuffer, sizeof(sampleBuffer), &bytesWritten);
   if (ret != MPG123_OK &&
      ret != MPG123_DONE)
   {
      m_lastError.LoadString(IDS_ENCODER_INTERNAL_DECODE_ERROR);
      m_lastError.AppendFormat(_T(" (%hs)"), mpg123_plain_strerror(ret));
      return -1;
   }

   m_isAtEndOfFile = bytesWritten == 0 ||
      ret == MPG123_DONE;

   if (bytesWritten == 0)
      return 0;

   int sampleSize = samples.GetInputModuleBitsPerSample();
   int numSamplesPerChannel = bytesWritten / m_channels / (sampleSize / 8);

   samples.PutSamplesInterleaved(sampleBuffer, numSamplesPerChannel);

   return numSamplesPerChannel;
}

float LibMpg123InputModule::PercentDone() const
{
   if (m_decoder == nullptr ||
      m_inputFile == nullptr ||
      m_fileSize == 0)
      return 0.0f;

   if (feof(m_inputFile.get()) ||
      m_isAtEndOfFile)
      return 100.0f;

   long pos = ftell(m_inputFile.get());

   return float(pos) * 100.0f / m_fileSize;
}

void LibMpg123InputModule::DoneInput()
{
   m_isAtEndOfFile = true;

   if (m_decoder != nullptr)
      mpg123_close(m_decoder.get());

   m_decoder.reset();
}

bool LibMpg123InputModule::GetFileSize()
{
   errno_t err = fseek(m_inputFile.get(), 0, SEEK_END);
   if (err != 0)
   {
      m_lastError.LoadString(IDS_ENCODER_INPUT_FILE_OPEN_ERROR);
      m_inputFile.reset();
      return false;
   }

   m_fileSize = ftell(m_inputFile.get());

   err = fseek(m_inputFile.get(), 0, SEEK_SET);
   if (err != 0)
   {
      m_lastError.LoadString(IDS_ENCODER_INPUT_FILE_OPEN_ERROR);
      m_inputFile.reset();
      return false;
   }

   return true;
}

static ssize_t ReadFromFile(void* handle, void* buffer, size_t size)
{
   return fread(buffer, 1, size, (FILE*)handle);
}

static off_t SeekInFile(void* handle, off_t offset, int direction)
{
   if (fseek((FILE*)handle, offset, direction) != 0)
      return (off_t)-1;
   return ftell((FILE*)handle);
}

static void CleanupFile(void* handle)
{
   // don't fclose the handle here, since m_inputFile will do that for us
   UNUSED(handle);
}

bool LibMpg123InputModule::OpenStream()
{
   mpg123_replace_reader_handle(m_decoder.get(), ReadFromFile, SeekInFile, CleanupFile);

   int ret = mpg123_open_handle(m_decoder.get(), m_inputFile.get());
   if (ret != MPG123_OK)
   {
      m_lastError.LoadString(IDS_ENCODER_INPUT_FILE_OPEN_ERROR);
      m_lastError.AppendFormat(_T(" (%hs)"), mpg123_plain_strerror(ret));
      return false;
   }

   m_isAtEndOfFile = false;

   return true;
}

bool LibMpg123InputModule::GetTrackInfo(const CString& filename, TrackInfo& trackInfo)
{
   // search for id3v2 tag
   bool found = GetId3v2TagInfos(filename, trackInfo);

   // search for id3v1 tag
   if (!found)
   {
      errno_t err = fseek(m_inputFile.get(), -128L, SEEK_END);
      if (err == 0)
      {
         Id3v1Tag id3tag;
         int ret = fread(id3tag.GetData(), 128, 1, m_inputFile.get());
         if (ret == 128 && id3tag.IsValidTag())
         {
            // store found id3 tag infos
            id3tag.ToTrackInfo(trackInfo);
            found = true;
         }
      }

      err = fseek(m_inputFile.get(), 0, SEEK_SET);
      if (err != 0)
         return false;
   }

   return found;
}

bool LibMpg123InputModule::GetId3v2TagInfos(const CString& filename, TrackInfo& trackInfo)
{
   AudioFileTag tag(trackInfo);
   return tag.ReadFromFile(filename);
}

bool LibMpg123InputModule::SetupDecoder()
{
   mpg123_format_none(m_decoder.get());

   const long* ratesList = nullptr;
   size_t ratesListSize = 0;
   mpg123_rates(&ratesList, &ratesListSize);

   for (long rate : std::vector<long>(ratesList, ratesList + ratesListSize))
   {
      // only request 32-bit samples
      int ret = mpg123_format(m_decoder.get(), rate, MPG123_STEREO | MPG123_MONO, MPG123_ENC_SIGNED_32);

      if (ret != MPG123_OK)
      {
         m_lastError.LoadString(IDS_ENCODER_ERROR_INIT_DECODER);
         m_lastError.AppendFormat(_T(" (%hs)"), mpg123_strerror(m_decoder.get()));
         return false;
      }
   }

   return true;
}

bool LibMpg123InputModule::SetFormat(SampleContainer& samples)
{
   long sampleRate = 0;
   int numChannels = 0;
   int encoding = 0;
   int ret = mpg123_getformat(m_decoder.get(), &sampleRate, &numChannels, &encoding);
   if (ret != MPG123_OK)
   {
      m_lastError.LoadString(IDS_ENCODER_ERROR_INIT_DECODER);
      m_lastError.AppendFormat(_T(" (%hs)"), mpg123_plain_strerror(ret));
      return false;
   }

   m_channels = numChannels;
   m_samplerate = sampleRate;

   samples.SetInputModuleTraits(encoding == MPG123_ENC_SIGNED_32 ? 32 : 16, SamplesInterleaved, m_samplerate, numChannels);

   return true;
}
