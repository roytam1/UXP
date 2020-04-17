/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var browser;
var dialog = {};
var pref = null;
var openLocationModule = {};
try {
  pref = Components.classes["@mozilla.org/preferences-service;1"]
                   .getService(Components.interfaces.nsIPrefBranch);
} catch(ex) {
  // not critical, remain silent
}

Components.utils.import("resource:///modules/openLocationLastURL.jsm", openLocationModule);
var gOpenLocationLastURL = new openLocationModule.OpenLocationLastURL(window.opener);

function onLoad() {
  dialog.input         = document.getElementById("dialog.input");
  dialog.open          = document.documentElement.getButton("accept");
  dialog.openWhereList = document.getElementById("openWhereList");
  dialog.openTopWindow = document.getElementById("currentWindow");
  dialog.bundle        = document.getElementById("openLocationBundle");

  if ("arguments" in window && window.arguments.length >= 1) {
    browser = window.arguments[0];
  }
   
  dialog.openWhereList.selectedItem = dialog.openTopWindow;

  if (pref) {
    try {
      var useAutoFill = pref.getBoolPref("browser.urlbar.autoFill");
      if (useAutoFill) {
        dialog.input.setAttribute("completedefaultindex", "true");
      }
    } catch(ex) {}

    try {
      var value = pref.getIntPref("general.open_location.last_window_choice");
      var element = dialog.openWhereList.getElementsByAttribute("value", value)[0];
      if (element) {
        dialog.openWhereList.selectedItem = element;
      }
      dialog.input.value = gOpenLocationLastURL.value;
    } catch(ex) {}
    
    if (dialog.input.value) {
      // XXX should probably be done automatically
      dialog.input.select();
    }
  }

  doEnabling();
}

function doEnabling() {
    dialog.open.disabled = !dialog.input.value;
}

function open() {
  var openData = {
    "url": null,
    "postData": null,
    "mayInheritPrincipal": false
  };
  if (browser) {
    browser.getShortcutOrURIAndPostData(dialog.input.value).then(data => {
      openData.url = data.url;
      openData.postData = data.postData;
      openData.mayInheritPrincipal = data.mayInheritPrincipal;

      openLocation(openData);
    });
  } else {
    openData.url = dialog.input.value;

    openLocation(openData);
  }

  return false;
}

function openLocation(openData) {
  try {
    // Whichever target we use for the load, we allow third-party services to
    // fix up the URI
    switch (dialog.openWhereList.value) {
      case "0":
        var webNav = Components.interfaces.nsIWebNavigation;
        var flags = webNav.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
                    webNav.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
        if (!openData.mayInheritPrincipal) {
          flags |= webNav.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
        }
        browser.gBrowser.loadURIWithFlags(openData.url, flags, null, null, openData.postData);
        break;
      case "1":
        window.opener.delayedOpenWindow(getBrowserURL(), "all,dialog=no",
                                        openData.url, openData.postData,
                                        null, null, true);
        break;
      case "3":
        browser.delayedOpenTab(openData.url, null, null, openData.postData, true);
        break;
    }
  } catch(ex) {}

  if (pref) {
    gOpenLocationLastURL.value = dialog.input.value;
    pref.setIntPref("general.open_location.last_window_choice", dialog.openWhereList.value);
  }

  window.close();
}

function createInstance(contractid, iidName)
{
  var iid = Components.interfaces[iidName];
  return Components.classes[contractid].createInstance(iid);
}

const nsIFilePicker = Components.interfaces.nsIFilePicker;
function onChooseFile()
{
  try {
    let fp = Components.classes["@mozilla.org/filepicker;1"]
                       .createInstance(nsIFilePicker);
    let fpCallback = function fpCallback_done(aResult) {
      if (aResult == nsIFilePicker.returnOK &&
          fp.fileURL.spec &&
          fp.fileURL.spec.length > 0) {
        dialog.input.value = fp.fileURL.spec;
      }
      doEnabling();
    };

    fp.init(window, dialog.bundle.getString("chooseFileDialogTitle"),
            nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText |
                     nsIFilePicker.filterImages | nsIFilePicker.filterXML |
                     nsIFilePicker.filterHTML);
    fp.open(fpCallback);
  } catch(ex) {}
}
