/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const FAVICON_QUESTION = "chrome://global/skin/icons/question-32.png";
const FAVICON_PRIVACY = "chrome://browser/skin/privatebrowsing/favicon.svg";

var stringBundle = Services.strings.createBundle(
                    "chrome://browser/locale/aboutPrivateBrowsing.properties");

function setFavIcon(url) {
 document.getElementById("favicon").setAttribute("href", url);
}

document.addEventListener("DOMContentLoaded", function () {
 if (!PrivateBrowsingUtils.isContentWindowPrivate(window)) {
   document.documentElement.classList.remove("private");
   document.documentElement.classList.add("normal");
   document.title = stringBundle.GetStringFromName("title.normal");
   document.getElementById("favicon")
           .setAttribute("href", FAVICON_QUESTION);
   document.getElementById("startPrivateBrowsing")
           .addEventListener("command", openPrivateWindow);
   return;
 }

 document.title = stringBundle.GetStringFromName("title.head");
 document.getElementById("favicon")
         .setAttribute("href", FAVICON_PRIVACY);

 let formatURLPref = Cc["@mozilla.org/toolkit/URLFormatterService;1"]
                       .getService(Ci.nsIURLFormatter).formatURLPref;
 document.getElementById("learnMore").setAttribute("href",
                    formatURLPref("app.support.baseURL") + "private-browsing");

}, false);

function openPrivateWindow() {
 // Ask chrome to open a private window
 document.dispatchEvent(
   new CustomEvent("AboutPrivateBrowsingOpenWindow", {bubbles:true}));
}

