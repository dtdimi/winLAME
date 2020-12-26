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
/// \file AacOutputModule.cpp
/// \brief contains the implementation of the AAC output module
//
#include "stdafx.h"
#include <fstream>
#include "resource.h"
#include "AacOutputModule.hpp"
#include "neaacdec.h"
#include <ulib/DynamicLibrary.hpp>
#include "ChannelRemapper.hpp"

using Encoder::AacOutputModule;
using Encoder::TrackInfo;
using Encoder::SampleContainer;

// AacOutputModule methods

AacOutputModule::AacOutputModule()
   :m_handle(nullptr),
   m_inputBufferSize(0),
   m_outputBufferSize(0),
   m_sampleBufferHigh(0),
   m_bitrateControlMethod(0)
{
   m_moduleId = ID_OM_AAC;
}

bool AacOutputModule::IsAvailable() const
{
   return DynamicLibrary(_T("libfaac_dll.dll")).IsLoaded();
}

CString AacOutputModule::GetDescription() const
{
   if (m_handle == nullptr)
      return CString();

   // get config
   faacEncConfigurationPtr config = faacEncGetCurrentConfiguration(m_handle);

   // format string
   CString desc;
   desc.Format(IDS_FORMAT_INFO_AAC_OUTPUT,
      config->mpegVersion == MPEG4 ? 4 : 2,
      (m_bitrateControlMethod == 0) ? _T("Quality ") : _T(""),
      (m_bitrateControlMethod == 0) ? config->quantqual : config->bitRate / 1000,
      (m_bitrateControlMethod == 0) ? _T("") : _T(" kbps/channel"),
      m_channels,
      config->bandWidth,
      config->allowMidside == 1 ? _T(", Mid/Side") : _T(""),
      config->useTns == 1 ? _T(", Temporal Noise Shaping") : _T(""),
      config->useLfe == 1 ? _T(", LFE channel") : _T(""));

   return desc;
}

void AacOutputModule::GetVersionString(CString& version, int special) const
{
   char* id_string = nullptr;
   char* copyright_string = nullptr;

   faacEncGetVersion(&id_string, &copyright_string);

   version.Format(_T("libfaac %hs"), id_string);
}

int AacOutputModule::InitOutput(LPCTSTR outfilename,
   SettingsManager& mgr, const TrackInfo& trackInfo,
   SampleContainer& samples)
{
   m_outputFile.open(outfilename, std::ios::out | std::ios::binary);
   if (!m_outputFile.is_open())
   {
      m_lastError.LoadString(IDS_ENCODER_OUTPUT_FILE_CREATE_ERROR);
      return -1;
   }

   m_samplerate = samples.GetInputModuleSampleRate();
   m_channels = samples.GetInputModuleChannels();

   // try to get a handle
   m_handle = faacEncOpen(m_samplerate, m_channels, &m_inputBufferSize, &m_outputBufferSize);

   if (m_handle == nullptr)
   {
      m_lastError.LoadString(IDS_ENCODER_ERROR_INIT_ENCODER);
      return -1;
   }

   // alloc memory for input buffer
   m_sampleBuffer.resize(m_inputBufferSize);
   m_sampleBufferHigh = 0;

   // alloc memory for output buffer
   m_outputBuffer.resize(m_outputBufferSize);

   // get current config
   faacEncConfigurationPtr config;
   config = faacEncGetCurrentConfiguration(m_handle);

   // set settings
   config->mpegVersion = mgr.QueryValueInt(AacMpegVersion) == 4 ? MPEG4 : MPEG2;

   int value = mgr.QueryValueInt(AacObjectType);
   config->aacObjectType = value == 1 ? LOW : value == 2 ? LTP : MAIN;

   config->allowMidside = mgr.QueryValueInt(AacAllowMS);
   config->useLfe = mgr.QueryValueInt(AacUseLFEChan);
   config->useTns = mgr.QueryValueInt(AacUseTNS);
   config->shortctl = SHORTCTL_NORMAL;
   config->inputFormat = FAAC_INPUT_16BIT;

   // set bandwidth
   if (mgr.QueryValueInt(AacAutoBandwidth))
   {
      config->bandWidth = 0;
   }
   else
   {
      config->bandWidth = mgr.QueryValueInt(AacBandwidth);
   }

   // set bitrate/quality
   m_bitrateControlMethod = mgr.QueryValueInt(AacBRCMethod);
   if (m_bitrateControlMethod == 0) // Quality
   {
      config->quantqual = mgr.QueryValueInt(AacQuality);
      config->bitRate = 0;
   }
   else // Bitrate
   {
      config->quantqual = 0;
      config->bitRate = mgr.QueryValueInt(AacBitrate) * 1000 / m_channels;
   }

   // channel remap
   for (size_t channelIndex = 0; channelIndex < std::min<size_t>(m_channels, ChannelRemapper::GetMaxMappedChannel()); channelIndex++)
      config->channel_map[channelIndex] = (int)ChannelRemapper::GetMappedChannel(
         T_enChannelMapType::aacOutputChannelMap, m_channels, channelIndex);

   // set new config
   faacEncSetConfiguration(m_handle, config);

   // set up output traits
   samples.SetOutputModuleTraits(16, SamplesInterleaved);

   return 0;
}

int AacOutputModule::EncodeSamples(SampleContainer& samples)
{
   // get input samples
   int numInputSamplesPerChannel = 0;
   short* inputSampleBuffer = (short*)samples.GetSamplesInterleaved(numInputSamplesPerChannel);

   // as faacEncEncode() always wants 'm_inputBufferSize' number of samples, we
   // have to store samples until a whole block of samples can be passed to
   // the function; otherwise faacEncEncode() would pad the buffer with 0's.

   size_t numInputSamples = numInputSamplesPerChannel * m_channels;
   size_t inputSamplePos = 0;

   //ATLTRACE(_T("AacOutputModule: encoding 0x%04x fresh input samples\n"), numInputSamples);

   // fill the sample buffer from the input samples
   do
   {
      size_t numSamplesToTransfer = std::min((size_t)m_inputBufferSize - m_sampleBufferHigh, numInputSamples - inputSamplePos);
      if (numSamplesToTransfer > 0)
      {
         //ATLTRACE(_T("AacOutputModule: copying 0x%04x bytes to sample buffer\n"), numSamplesToTransfer);
         memcpy(m_sampleBuffer.data() + m_sampleBufferHigh, inputSampleBuffer + inputSamplePos, numSamplesToTransfer * sizeof(short));
         m_sampleBufferHigh += numSamplesToTransfer;
         inputSamplePos += numSamplesToTransfer;
      }

      // if the sample buffer is full, encode one frame
      if (m_inputBufferSize == m_sampleBufferHigh)
      {
         // encode the samples
         int ret = faacEncEncode(m_handle,
            reinterpret_cast<int*>(m_sampleBuffer.data()),
            m_inputBufferSize,
            m_outputBuffer.data(),
            m_outputBuffer.size());

         //ATLTRACE(_T("AacOutputModule: encoding the samples to %i output bytes\n"), ret);

         if (ret < 0)
            return ret;

         // write the output buffer
         if (ret > 0)
            m_outputFile.write(reinterpret_cast<char*>(m_outputBuffer.data()), ret);

         m_sampleBufferHigh = 0;
      }

      // loop while the input samples are exhausted
   } while (inputSamplePos < numInputSamples);

   //ATLTRACE(_T("AacOutputModule: finished encoding samples, 0x%04x samples are left in buffer\n"), m_sampleBufferHigh);

   return numInputSamples;
}

void AacOutputModule::DoneOutput()
{
   int ret = 0;

   // encode the last samples in sample buffer
   if (m_sampleBufferHigh > 0)
   {
      //ATLTRACE(_T("AacOutputModule: encoding remaining 0x%04x samples that were left in buffer\n"), m_sampleBufferHigh);

      ret = faacEncEncode(m_handle,
         reinterpret_cast<int*>(m_sampleBuffer.data()),
         m_sampleBufferHigh,
         m_outputBuffer.data(),
         m_outputBuffer.size());

      if (ret > 0)
         m_outputFile.write(reinterpret_cast<char*>(m_outputBuffer.data()), ret);
   }

   // finish encoding and write the last aac frames
   while ((ret = faacEncEncode(m_handle,
      nullptr,
      0,
      m_outputBuffer.data(),
      m_outputBuffer.size())) > 0)
   {
      m_outputFile.write(reinterpret_cast<char*>(m_outputBuffer.data()), ret);
   }

   m_outputFile.close();

   faacEncClose(m_handle);
}
