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

using namespace JSONRPC;

JSONRPC_STATUS CTranscoderOperations::Transcode(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  CVariant movieid = parameterObject["movieid"];

  CFileItemList list;
  CVideoLibrary::FillFileItemList(parameterObject, list);

  if (list.Size() == 1)
  {
    std::string moviename = list[0]->GetMovieName();
    std::string moviepath = list[0]->GetPath();
    CLog::Log(LOGDEBUG, "JSONRPC: Found requested movie '%s' with id '%d' in library. Path: %s"
      , moviename.c_str(), (int) movieid.asInteger(), moviepath.c_str());

    CTranscoder transcoder;
    transcoder.Transcode(moviepath);
  }

  CLog::Log(LOGDEBUG, "JSONRPC: Request for transcoding received but not implemented yet.");

  return OK;
}
