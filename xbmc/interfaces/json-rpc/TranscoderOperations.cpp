/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "TranscoderOperations.h"
#include "VideoLibrary.h"
#include "utils/log.h"
#include "cores/Transcoder/Transcoder.h"
#include "filesystem/File.h"
#include "URL.h"

using namespace JSONRPC;

JSONRPC_STATUS CTranscoderOperations::Transcode(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  CVariant movieid = parameterObject["movieid"];
  if (movieid.isNull())
  {
    CLog::Log(LOGERROR, "Request for transcode does not contain a 'movieid' parameter.");
    return InvalidParams;
  }

  CFileItemList list;
  CVideoLibrary::FillFileItemList(parameterObject, list);

  if (list.Size() == 1)
  {
    CFileItemPtr movie = list[0];
    std::string moviename = movie->GetMovieName();
    std::string moviepath = movie->GetPath();
    CLog::Log(LOGDEBUG, "JSONRPC: Found requested movie '%s' with id '%d' in library. Path: %s"
      , moviename.c_str(), (int) movieid.asInteger(), moviepath.c_str());

    CTranscoder transcoder;
    std::string transpath = transcoder.TranscodePath(moviepath);

    if (XFILE::CFile::Exists(transpath))
    {
      CLog::Log(LOGDEBUG, "Transcoded movie already exists.");
    }
    else
    {
      CLog::Log(LOGDEBUG, "Transcoding movie to: %s", transpath);
      transcoder.Transcode(moviepath);
    }

    result["protocol"] = "http";

    if ((transport->GetCapabilities() & FileDownloadDirect) == FileDownloadDirect)
      result["mode"] = "direct";
    else
      result["mode"] = "redirect";

    std::string url = "vfs/";
    url += CURL::Encode(transpath);
    result["details"]["path"] = url;

    //result["path"] = transpath;
    return OK;
  }

  CLog::Log(LOGDEBUG, "The requested movie could not be found.");
  return JSONRPC::JSONRPC_STATUS::InternalError;
}
