//
// winLAME - a frontend for the LAME encoding engine
// Copyright (c) 2000-2020 Michael Fink
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
/// \file AudioFileInfoManager.cpp
/// \brief Manager to fetch audio file infos
//
#include "stdafx.h"
#include "AudioFileInfoManager.hpp"
#include "ModuleManager.hpp"
#include <functional>

AudioFileInfoManager::AudioFileInfoManager()
   :m_ioContext(1), // one thread max.
   m_defaultWork(boost::asio::make_work_guard(m_ioContext)),
   m_stopping(false)
{
}

AudioFileInfoManager::~AudioFileInfoManager()
{
   try
   {
      Stop();
   }
   catch (...) // NOSONAR
   {
      // ignore errors when stopping
   }

   if (m_upThread != nullptr)
      m_upThread->join();
}

bool AudioFileInfoManager::GetAudioFileInfo(LPCTSTR filename,
   int& lengthInSeconds, int& bitrateInBps, int& sampleFrequencyInHz, CString& errorMessage)
{
   Encoder::ModuleManager& moduleManager = IoCContainer::Current().Resolve<Encoder::ModuleManager>();

   return moduleManager.GetAudioFileInfo(filename, lengthInSeconds, bitrateInBps, sampleFrequencyInHz, errorMessage);
}

void AudioFileInfoManager::AsyncGetAudioFileInfo(LPCTSTR filename, AudioFileInfoManager::T_fnCallback fnCallback)
{
   ATLASSERT(fnCallback != nullptr);

   if (m_stopping)
      return;

   if (m_upThread == nullptr)
   {
      m_upThread.reset(new std::thread(std::bind(&AudioFileInfoManager::RunThread, std::ref(m_ioContext))));
   }

   boost::asio::post(
      m_ioContext.get_executor(),
      std::bind(&AudioFileInfoManager::WorkerGetAudioFileInfo, std::ref(m_stopping), CString(filename), fnCallback));
}

void AudioFileInfoManager::Stop()
{
   m_stopping = true;
   m_defaultWork.reset();
   m_ioContext.stop();
}

void AudioFileInfoManager::RunThread(boost::asio::io_context& ioContext)
{
   try
   {
      ioContext.run();
   }
   catch (boost::system::system_error& error)
   {
      UNUSED(error);
      ATLTRACE(_T("system_error: %hs\n"), error.what());
      ATLASSERT(false);
   }
}

void AudioFileInfoManager::WorkerGetAudioFileInfo(std::atomic<bool>& stopping,
   const CString& filename, AudioFileInfoManager::T_fnCallback fnCallback)
{
   if (stopping)
      return;

   int lengthInSeconds = 0;
   int bitrateInBps = 0;
   int sampleFrequencyInHz = 0;
   CString errorMessage;

   bool ret = GetAudioFileInfo(filename, lengthInSeconds, bitrateInBps, sampleFrequencyInHz, errorMessage);

   bool isStopped = stopping;
   if (isStopped)
      return;

   fnCallback(!ret, errorMessage, lengthInSeconds, bitrateInBps, sampleFrequencyInHz);
}
