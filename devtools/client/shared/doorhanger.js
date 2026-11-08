/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cc } = require("chrome");
const Services = require("Services");
const { DOMHelpers } = require("resource://devtools/client/shared/DOMHelpers.jsm");
const { Task } = require("devtools/shared/task");
const defer = require("devtools/shared/defer");
const { getMostRecentBrowserWindow } = require("sdk/window/utils");

const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const LOCALE = Cc["@mozilla.org/chrome/chrome-registry;1"]
               .getService(Ci.nsIXULChromeRegistry)
               .getSelectedLocale("global");

var TYPES = {
  // We don't support any doorhanger types at the moment.
  // This is vestigial from the FF dev edition promo.
};

var panelAttrs = {
  orient: "vertical",
  hidden: "false",
  consumeoutsideclicks: "true",
  noautofocus: "true",
  align: "start",
  role: "alert"
};

/**
 * Helper to call a doorhanger, defined in `TYPES`, with defined conditions,
 * success handlers and loads its own XUL in a frame. Takes an object with
 * several properties:
 *
 * @param {XULWindow} window
 *        The window that should house the doorhanger.
 * @param {String} type
 *        The type of doorhanger to be displayed is, using the `TYPES`
 *        definition.
 * @param {String} selector
 *        The selector that the doorhanger should be appended to within
 *        `window`.  Defaults to a XUL Document's `window` element.
 */
exports.showDoorhanger = Task.async(function* ({ window, type, anchor }) {
  let { predicate, success, url, action } = TYPES[type];
  // Abort if predicate fails
  if (!predicate()) {
    return;
  }

  // Call success function to set preferences/cleanup immediately,
  // so if triggered multiple times, only happens once (Windows/Linux)
  success();

  // Wait 200ms to prevent flickering where the popup is displayed
  // before the underlying window (Windows 7, 64bit)
  yield wait(200);

  let document = window.document;

  let panel = document.createElementNS(XULNS, "panel");
  let frame = document.createElementNS(XULNS, "iframe");
  let parentEl = document.querySelector("window");

  frame.setAttribute("src", url);
  let close = () => parentEl.removeChild(panel);

  setDoorhangerStyle(panel, frame);

  panel.appendChild(frame);
  parentEl.appendChild(panel);

  yield onFrameLoad(frame);

  panel.openPopup(anchor);

  let closeBtn = frame.contentDocument.querySelector("#close");
  if (closeBtn) {
    closeBtn.addEventListener("click", close);
  }

  let goBtn = frame.contentDocument.querySelector("#go");
  if (goBtn) {
    goBtn.addEventListener("click", () => {
      if (action) {
        action();
      }
      close();
    });
  }
});

function setDoorhangerStyle(panel, frame) {
  Object.keys(panelAttrs).forEach(prop => {
    return panel.setAttribute(prop, panelAttrs[prop]);
  });
  panel.style.margin = "20px";
  panel.style.borderRadius = "5px";
  panel.style.border = "none";
  panel.style.MozAppearance = "none";
  panel.style.backgroundColor = "transparent";

  frame.style.borderRadius = "5px";
  frame.setAttribute("flex", "1");
  frame.setAttribute("width", "450");
  frame.setAttribute("height", "179");
}

function onFrameLoad(frame) {
  let { resolve, promise } = defer();

  if (frame.contentWindow) {
    let domHelper = new DOMHelpers(frame.contentWindow);
    domHelper.onceDOMReady(resolve);
  } else {
    let callback = () => {
      frame.removeEventListener("DOMContentLoaded", callback);
      resolve();
    };
    frame.addEventListener("DOMContentLoaded", callback);
  }

  return promise;
}

function getGBrowser() {
  return getMostRecentBrowserWindow().gBrowser;
}

function wait(n) {
  let { resolve, promise } = defer();
  setTimeout(resolve, n);
  return promise;
}
