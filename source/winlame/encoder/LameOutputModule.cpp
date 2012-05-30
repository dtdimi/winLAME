/*
   winLAME - a frontend for the LAME encoding engine
   Copyright (c) 2000-2009 Michael Fink
   Copyright (c) 2004 DeXT

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
/*! \file LameOutputModule.cpp

   \brief contains the implementation of the LAME output module

*/

// needed includes
#include "stdafx.h"
#include <fstream>
#include "resource.h"
#include "LameOutputModule.h"
#include "WaveMp3Header.h"
#include "Id3v1Tag.h"
#include "id3/file.h"

// debug helper
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


// linker options

#if _MSC_VER < 1400
#pragma comment(linker, "/delayload:nlame.dll")
#endif


// static variables

nlame_instance_t *LameOutputModule::nogap_inst = NULL;


// LameOutputModule methods

LameOutputModule::LameOutputModule()
{
   inst = NULL;
   mp3buf = NULL;
   module_id = ID_OM_LAME;
   infotag = true; // always write VBR info tag
   nogap = false;
   waveheader = false;
   id3tag = NULL;
   inbuffer = NULL;
   inbuffer_fill = 0;
   buftype = nle_buffer_short;
   lame_input_buffer_size = 1152;
   lasterror = "";
}

LameOutputModule::~LameOutputModule()
{
   // free buffers
   delete id3tag;
   delete[] mp3buf;
   delete[] inbuffer;
}

bool LameOutputModule::isAvailable()
{
   HMODULE dll = ::LoadLibrary(_T("nlame.dll"));
   bool avail = dll != NULL;

   if (avail)
   {
      typedef int(*nlame_get_api_version_t)();

      nlame_get_api_version_t p_get_api_version =
         reinterpret_cast<nlame_get_api_version_t>(::GetProcAddress(dll, "nlame_get_api_version"));

      avail = p_get_api_version != NULL; // insist on having nlame_get_api_version()

      // check if version is new enough for this
      if (avail)
      {
         // need at least version 1
         // the version that first implemented nle_var_quality_value_high, etc.
         // see nlame.h for more
         avail = p_get_api_version() >= 1;
      }
   }

   ::FreeLibrary(dll);

   return avail;
}

void LameOutputModule::getVersionString(CString& version, int special)
{
   USES_CONVERSION;

   // retrieve lame version
   if (isAvailable())
   {
      switch (special)
      {
      case 0:
         version = A2CT(::nlame_lame_version_get(nle_lame_version_normal));
         break;
      case 1:
         version = A2CT(::nlame_lame_string_get(nle_lame_string_compiler));
         break;
      case 2:
         version = A2CT(::nlame_lame_string_get(nle_lame_string_cpu_features));
         break;
      }
   }
   else
      if (special==1) version = _T("N/A");
}

void LameOutputModule::prepareOutput(SettingsManager &mgr)
{
   waveheader = mgr.queryValueInt(LameWriteWaveHeader)!=0;
}

//! error callback
void LameErrorCallback(const char* format, va_list ap)
{
   USES_CONVERSION;

   TCHAR buffer[256];
   _sntprintf(buffer,256,_T("%hs"), format);

   ATLTRACE(buffer,ap);
}

int LameOutputModule::initOutput(LPCTSTR outfilename,
   SettingsManager &mgr, const TrackInfo& trackinfo,
   SampleContainer &samplecont)
{
   int ret;

   // alloc memory for output mp3 buffer
   mp3buf = new unsigned char[nlame_const_maxmp3buffer];

   channels = samplecont.getInputModuleChannels();
   samplerate = samplecont.getInputModuleSampleRate();

   // store track info for ID3v2 tag
   m_trackInfoID3v2 = trackinfo;

   // check if we do nogap encoding
   nogap = mgr.queryValueInt(LameOptNoGap)==1;

   if (nogap && nogap_inst!=NULL)
   {
      // use last stored nlame instance
      inst = nogap_inst;
      nogap_inst = NULL;

      // update tag for next track
      if (!waveheader)
         AddLameID3v2Tag(m_trackInfoID3v2);

      // reinit bitstream with new tag infos
      nlame_reinit_bitstream(inst);

      // skip engine init when encoding with nogap
      goto skip_nogap;
   }
   else
   {
      // init nlame
      inst = nlame_new();
   }

   if (inst==NULL)
   {
      lasterror = _T("nlame_new() failed");
      return -1;
   }

   if (!waveheader)
      AddLameID3v2Tag(m_trackInfoID3v2);

   // set callbacks
   nlame_callback_set(inst, nle_callback_error,LameErrorCallback);

   // set all nlame variables
   {
      nlame_var_set_int(inst, nle_var_in_samplerate, samplerate);
      nlame_var_set_int(inst, nle_var_num_channels, channels);

      // mono encoding?
      bool bMono = mgr.queryValueInt(LameSimpleMono) == 1;

      // set mono encoding, else let LAME choose the default (which is joint stereo)
      if (bMono)
         nlame_var_set_int(inst, nle_var_channel_mode, nle_mode_mono);

      // which mode? 0: bitrate mode, 1: quality mode
      if (mgr.queryValueInt(LameSimpleQualityOrBitrate) == 0)
      {
         // bitrate mode
         int nBitrate = mgr.queryValueInt(LameSimpleBitrate);

         if (mgr.queryValueInt(LameSimpleCBR)==1)
         {
            // CBR
            nlame_var_set_int(inst, nle_var_vbr_mode, nle_vbr_mode_off);
            nlame_var_set_int(inst, nle_var_bitrate, nBitrate);
         }
         else
         {
            // ABR
            nlame_var_set_int(inst, nle_var_vbr_mode, nle_vbr_mode_abr);
            nlame_var_set_int(inst, nle_var_abr_mean_bitrate, nBitrate);
         }
      }
      else
      {
         // quality mode; value ranges from 0 to 9
         int nQuality = mgr.queryValueInt(LameSimpleQuality);

         nlame_var_set_int(inst, nle_var_vbr_quality, nQuality);

         // VBR mode; LameSimpleVBRMode, 0: standard, 1: fast
         int nVbrMode = mgr.queryValueInt(LameSimpleVBRMode);

         if (nVbrMode == 0)
            nlame_var_set_int(inst, nle_var_vbr_mode, nle_vbr_mode_old); // standard
         else
            nlame_var_set_int(inst, nle_var_vbr_mode, nle_vbr_mode_new); // fast
      }

      // encode quality; LameSimpleEncodeQuality, 0: fast, 1: standard, 2: high
      // fast maps to quality factor 7, high is quality factor 2
      // note: this currently is hard coded; maybe move this into nlame?
      int nEncQuality = mgr.queryValueInt(LameSimpleEncodeQuality);

      // mapping:
      // 0: "fast", or -f
      // 1: "standard" (no -q switch)
      // 2: "high", or -h
      // note: when using "standard" encoding quality we don't set nle_var_quality,
      // since the LAME engine then chooses the default quality value.
      if (nEncQuality == 0)
         nlame_var_set_int(inst, nle_var_quality, nlame_var_get_int(inst, nle_var_quality_value_fast));
      else if (nEncQuality == 2)
         nlame_var_set_int(inst, nle_var_quality, nlame_var_get_int(inst, nle_var_quality_value_high));
   }

   // always use replay gain
   nlame_var_set_int(inst,nle_var_find_replay_gain,1);
   // note: decode on the fly is not supported, since nLAME.dll doesn't have the
   //       decoder compiled in; so no --replaygain-accurate for now
//   nlame_var_set_int(inst,nle_var_decode_on_the_fly,0);

   // init more settings in nlame
   ret = nlame_init_params(inst);

   if (ret<0)
   {
      lasterror = _T("nlame_init_params() failed");
      return ret;
   }

skip_nogap:

   // do description string
   {
      CString cszText;

      // encoding quality
      int nEncQuality = mgr.queryValueInt(LameSimpleEncodeQuality);

      cszText.Format(IDS_FORMAT_INFO_LAME,
         nEncQuality == 0 ? _T("Fast") : nEncQuality == 1 ? _T("Standard") : _T("High"),
         mgr.queryValueInt(LameSimpleMono) == 0 ? _T("") : _T(" (mono)"));

      CString cszFormat;

      // quality or bitrate mode?
      int nQualityOrBitrate = mgr.queryValueInt(LameSimpleQualityOrBitrate);
      if (nQualityOrBitrate == 0) // bitrate
      {
         int nBitrate = mgr.queryValueInt(LameSimpleBitrate);
         cszFormat.Format(
            mgr.queryValueInt(LameSimpleCBR) == 0 ? IDS_FORMAT_INFO_LAME_ABR : IDS_FORMAT_INFO_LAME_CBR,
            nBitrate);

         cszText += cszFormat;
      }
      else // quality
      {
         int nQuality = mgr.queryValueInt(LameSimpleQuality);
         cszFormat.Format(IDS_FORMAT_INFO_LAME_VBR,
            nQuality,
            mgr.queryValueInt(LameSimpleVBRMode) == 0 ? _T("Standard") : _T("Fast"));
         cszText += cszFormat;
      }

      // stereo mode
      int value = nlame_var_get_int(inst, nle_var_channel_mode);
      cszText += (value==nle_mode_stereo ? _T("Stereo") :
                  value==nle_mode_joint_stereo ? _T("Joint Stereo") : _T("Mono"));

      // nogap option
      if (nogap)
         cszText += _T(", gapless encoding");

      description = cszText;
   }

   mp3filename = outfilename;
//   infotag = mgr.queryValueInt(LameVBRWriteTag)==1;

   nogap_lastfile = mgr.queryValueInt(wlGeneralIsLastFile)==1;

   // generate info tag?
   nlame_var_set_int(inst,nle_var_vbr_generate_info_tag,infotag?1:0);

   // create id3 tag data
   if (!trackinfo.IsEmpty())
      id3tag = new Id3v1Tag(trackinfo);

   USES_CONVERSION;
   // open output file
   ostr.open(T2CA(outfilename),std::ios::out|std::ios::binary);
   if (!ostr.is_open())
   {
      lasterror.LoadString(IDS_ENCODER_OUTPUT_FILE_CREATE_ERROR);
      return -1;
   }

   // set up output traits
   int bps = 16;
   buftype = nle_buffer_short;

   // beginning with version 2 of the nLAME API, the encoder can handle 32-bit
   // input samples
   if (nlame_get_api_version() >= 2 && samplecont.getInputModuleBitsPerSample()>16)
   {
      bps=32;
      buftype = nle_buffer_int;
   }

   samplecont.setOutputModuleTraits(bps,SamplesInterleaved);

   // retrieve framesize from LAME encoder; varies from MPEG version and layer number
   int framesize = nlame_var_get_int(inst, nle_var_framesize);
   if (framesize <= 0)
      return -1;
   lame_input_buffer_size = static_cast<unsigned int>(framesize);

   inbuffer = new unsigned char[lame_input_buffer_size*channels*(bps>>3)];
   inbuffer_fill = 0;

   samplecount=0;
   datalen=0;

   // write wave mp3 header, when wanted
   if (waveheader)
   {
      // write wave header
      wlWriteWaveMp3Header(ostr,
         nlame_var_get_int(inst, nle_var_channel_mode)==nle_mode_mono ? 1 : 2,
         nlame_var_get_int(inst, nle_var_out_samplerate),
         nlame_var_get_int(inst, nle_var_bitrate),
         nlame_var_get_int(inst, nle_var_encoder_delay));
   }

   return 0;
}

/*! encodes exactly one frame, consisting of 576 samples per channel. this is
    done due to the fact that LAME expects that number of samples, or it will
    produce different output, e.g. when feeding less than 576 samples per call
    to nlame_encode_buffer_*().
*/
int LameOutputModule::encodeFrame()
{
   int bufsize = nlame_const_maxmp3buffer;

   // encode buffer
   int ret;
   if (channels==1)
   {
      ret = nlame_encode_buffer_mono(inst, buftype,
         inbuffer, inbuffer_fill, mp3buf, bufsize);
   }
   else
   {
      ret = nlame_encode_buffer_interleaved(inst, buftype,
         inbuffer, inbuffer_fill, mp3buf, bufsize);
   }

   samplecount += inbuffer_fill;
   inbuffer_fill = 0;

   // error?
   if (ret<0)
      return ret;

   // write out data when available
   if (ret>0)
   {
      ostr.write(reinterpret_cast<char*>(mp3buf), ret);
      datalen += ret;
   }

   return ret;
}

int LameOutputModule::encodeSamples(SampleContainer& samples)
{
   // get samples
   int numsamples=0;
   unsigned char* samplebuf = (unsigned char*)samples.getSamplesInterleaved(numsamples);

   unsigned int sbuf_count = 0;

   int ret = 0;
   int samplesize = samples.getOutputModuleBitsPerSample() >> 3;
   do
   {
      unsigned int fillsize = std::min(lame_input_buffer_size-inbuffer_fill, static_cast<unsigned int>(numsamples));

      // copy samples into inbuffer; note: inbuffer_fill is counted in "samples"
      memcpy(
         inbuffer+inbuffer_fill*channels*samplesize,
         samplebuf+sbuf_count*channels*samplesize,
         fillsize*channels*samplesize);

      inbuffer_fill += fillsize;
      numsamples -= fillsize;
      sbuf_count += fillsize;

      if (inbuffer_fill == lame_input_buffer_size)
      {
         // encode one frame
         ret = encodeFrame();
         if (ret < 0)
            break;
      }

   } while (numsamples > 0);

   return ret;
}

void LameOutputModule::doneOutput()
{
   // encode remaining samples, if any
   encodeFrame();

   // finish encoding
   int ret;

   if (nogap && !nogap_lastfile)
   {
      ret = nlame_encode_flush_nogap(inst,mp3buf,nlame_const_maxmp3buffer);
   }
   else
   {
      ret = nlame_encode_flush(inst,mp3buf,nlame_const_maxmp3buffer);
   }

   if (ret>0)
   {
      ostr.write(reinterpret_cast<char*>(mp3buf), ret);
      datalen += ret;
   }

   // write id3 tag when available
   // note: we write id3 tag when we do gapless encoding, too, since
   //       most decoders should be aware now
   // note: we don't write id3 tags to wave files when we wrote a wave header,
   //       since that that might confuse some software
   if (!waveheader && /* !nogap && */ id3tag != NULL)
   {
      ostr.write((char*)id3tag->getData(), 128);
   }

   if (waveheader)
   {
      // fix up fact chunk and riff header lengths; seeks around a bit
      wlFixupWaveMp3Header(ostr,datalen,samplecount);
   }

   // close file
   ostr.close();

   // write ID3v2 tag
//   if (!waveheader)
//      WriteID3v2Tag();

   // add VBR info tag to mp3 file
   // note: since nlame_write_vbr_infotag() seeks to the front of the output
   //       file, the wave header might get overwritten, so we don't write a
   //       info tag when writing a wave header
   if (infotag && !nogap && !waveheader)
   {
      USES_CONVERSION;
      FILE *fp = _tfopen(mp3filename, _T("r+b"));

      nlame_write_vbr_infotag(inst,fp);
      fclose(fp);
   }

   // free lame input buffer
   delete[] inbuffer;
   inbuffer = NULL;

   // free output mp3 buffer
   delete[] mp3buf;
   mp3buf = NULL;

   if (nogap && !nogap_lastfile)
   {
      nogap_inst = inst;
   }
   else
   {
      // free nlame instance
      nlame_delete(inst);
   }

   inst = NULL;
}

void LameOutputModule::AddLameID3v2Tag(const TrackInfo& trackinfo)
{
   // add ID3v2 tags using LAME's functions; note that this writes ID3v2 version 2.3 tags
   // with text encoded as ISO-8859-1 (Latin1) that may not be able to represent all
   // characters we have

   unsigned int uiPaddingLength = GetID3v2PaddingLength();
   nlame_id3tag_init(inst, false, true, uiPaddingLength);

   // add all tags
   USES_CONVERSION;
   bool bAvail = false;
   CString cszValue = trackinfo.TextInfo(TrackInfoTitle, bAvail);
   if (bAvail)
      nlame_id3tag_setfield_latin1(inst, nif_title, T2CA(cszValue));

   cszValue = trackinfo.TextInfo(TrackInfoArtist, bAvail);
   if (bAvail)
      nlame_id3tag_setfield_latin1(inst, nif_artist, T2CA(cszValue));

   cszValue = trackinfo.TextInfo(TrackInfoComment, bAvail);
   if (bAvail)
      nlame_id3tag_setfield_latin1(inst, nif_comment, T2CA(cszValue));

   cszValue = trackinfo.TextInfo(TrackInfoAlbum, bAvail);
   if (bAvail)
      nlame_id3tag_setfield_latin1(inst, nif_album, T2CA(cszValue));

   // numeric
   int iValue = trackinfo.NumberInfo(TrackInfoYear, bAvail);
   if (bAvail)
   {
      cszValue.Format(_T("%i"), iValue);
      nlame_id3tag_setfield_latin1(inst, nif_year, T2CA(cszValue));
   }

   iValue = trackinfo.NumberInfo(TrackInfoTrack, bAvail);
   if (bAvail)
   {
      cszValue.Format(_T("%i"), iValue);
      nlame_id3tag_setfield_latin1(inst, nif_track, T2CA(cszValue));
   }

   cszValue = trackinfo.TextInfo(TrackInfoGenre, bAvail);
   if (bAvail)
   {
      nlame_id3tag_setfield_latin1(inst, nif_genre, T2CA(cszValue));
   }
}

unsigned int LameOutputModule::GetID3v2PaddingLength()
{
   unsigned int uiLength = 0;

   CString cszValue;
   bool bAvail;
   const TrackInfo& trackinfo = m_trackInfoID3v2;

   // add all frames
   cszValue = trackinfo.TextInfo(TrackInfoTitle, bAvail);
   if (bAvail)
      uiLength += cszValue.GetLength() + 1;

   cszValue = trackinfo.TextInfo(TrackInfoArtist, bAvail);
   if (bAvail)
      uiLength += cszValue.GetLength() + 1;

   cszValue = trackinfo.TextInfo(TrackInfoComment, bAvail);
   if (bAvail)
      uiLength += cszValue.GetLength() + 1;

   cszValue = trackinfo.TextInfo(TrackInfoAlbum, bAvail);
   if (bAvail)
      uiLength += cszValue.GetLength() + 1;

   // numeric

   int iValue = trackinfo.NumberInfo(TrackInfoYear, bAvail);
   if (bAvail)
   {
      cszValue.Format(_T("%i"), iValue);
      uiLength += cszValue.GetLength() + 1;
   }

   iValue = trackinfo.NumberInfo(TrackInfoTrack, bAvail);
   if (bAvail)
   {
      cszValue.Format(_T("%i"), iValue);
      uiLength += cszValue.GetLength() + 1;
   }

   cszValue = trackinfo.TextInfo(TrackInfoGenre, bAvail);
   if (bAvail)
   {
      uiLength += cszValue.GetLength() + 1;
   }

   return unsigned((uiLength * 4) * 1.5); // multiply with 4 for UCS-32, and add 50% overhead due to tag headers
}

void LameOutputModule::WriteID3v2Tag()
{
   ID3::File file(mp3filename, false); // read-write

   // get primary tag
   ID3::Tag tag = file.GetTag();

   tag.SetOption(ID3::Tag::foID3v1, 0);

   // if there's already a title tag, remove it; written in initOutput()
   {
      if (tag.IsFrameAvail(ID3::FrameId::Title))
      {
         ID3::Frame frame = tag.FindFrame(ID3::FrameId::Title);
         tag.DetachFrame(frame);
      }
   }

   CString cszValue;
   bool bAvail;
   const TrackInfo& trackinfo = m_trackInfoID3v2;

   // add all frames
   cszValue = trackinfo.TextInfo(TrackInfoTitle, bAvail);
   if (bAvail)
   {
      ID3::Frame frame(ID3::FrameId::Title);
      frame.SetString(cszValue);
      tag.AttachFrame(frame);
   }

   cszValue = trackinfo.TextInfo(TrackInfoArtist, bAvail);
   if (bAvail)
   {
      ID3::Frame frame(ID3::FrameId::Artist);
      frame.SetString(cszValue);
      tag.AttachFrame(frame);
   }

   cszValue = trackinfo.TextInfo(TrackInfoComment, bAvail);
   if (bAvail)
   {
      ID3::Frame frame(ID3::FrameId::Comment);
      frame.SetString(cszValue);
      tag.AttachFrame(frame);
   }

   cszValue = trackinfo.TextInfo(TrackInfoAlbum, bAvail);
   if (bAvail)
   {
      ID3::Frame frame(ID3::FrameId::AlbumTitle);
      frame.SetString(cszValue);
      tag.AttachFrame(frame);
   }

   // numeric

   int iValue = trackinfo.NumberInfo(TrackInfoYear, bAvail);
   if (bAvail)
   {
      cszValue.Format(_T("%i"), iValue);

      ID3::Frame frame(ID3::FrameId::RecordingTime);
      frame.SetString(cszValue);
      tag.AttachFrame(frame);
   }

   iValue = trackinfo.NumberInfo(TrackInfoTrack, bAvail);
   if (bAvail)
   {
      cszValue.Format(_T("%i"), iValue);

      ID3::Frame frame(ID3::FrameId::TrackNumber);
      frame.SetString(cszValue);
      tag.AttachFrame(frame);
   }

   cszValue = trackinfo.TextInfo(TrackInfoGenre, bAvail);
   if (bAvail)
   {
      ID3::Frame frame(ID3::FrameId::Title);
      frame.SetString(cszValue);
      tag.AttachFrame(frame);
   }

   file.Update();
}
