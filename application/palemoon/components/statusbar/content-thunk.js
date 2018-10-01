/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function handleVideoDetect(message)
{
  let isVideo = false;

  let fsEl = content.document.mozFullScreenElement;
  if(fsEl)
  {
    isVideo = (
      fsEl.nodeName == "VIDEO"
      || (fsEl.nodeName == "IFRAME" && fsEl.contentDocument && fsEl.contentDocument.getElementsByTagName("VIDEO").length > 0)
      || fsEl.getElementsByTagName("VIDEO").length > 0
    );
  }

  sendAsyncMessage("status4evar@caligonstudios.com:video-detect-answer", {isVideo: isVideo});
}

addMessageListener("status4evar@caligonstudios.com:video-detect", handleVideoDetect);

