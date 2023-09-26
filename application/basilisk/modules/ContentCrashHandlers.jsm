/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

this.EXPORTED_SYMBOLS = [ "TabCrashHandler",
                          "PluginCrashReporter" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CrashSubmit",
  "resource://gre/modules/CrashSubmit.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RemotePages",
  "resource://gre/modules/RemotePageManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
  "resource:///modules/RecentWindow.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

XPCOMUtils.defineLazyGetter(this, "gNavigatorBundle", function() {
  const url = "chrome://browser/locale/browser.properties";
  return Services.strings.createBundle(url);
});

// We don't process crash reports older than 28 days, so don't bother
// submitting them
const PENDING_CRASH_REPORT_DAYS = 28;
const DAY = 24 * 60 * 60 * 1000; // milliseconds
const DAYS_TO_SUPPRESS = 30;
const MAX_UNSEEN_CRASHED_CHILD_IDS = 20;

this.TabCrashHandler = {
  _crashedTabCount: 0,
  childMap: new Map(),
  browserMap: new WeakMap(),
  unseenCrashedChildIDs: [],
  crashedBrowserQueues: new Map(),

  init: function () {
    if (this.initialized)
      return;
    this.initialized = true;

    Services.obs.addObserver(this, "ipc:content-shutdown", false);
    Services.obs.addObserver(this, "oop-frameloader-crashed", false);

    this.pageListener = new RemotePages("about:tabcrashed");
    // LOAD_BACKGROUND pages don't fire load events, so the about:tabcrashed
    // content will fire up its own message when its initial scripts have
    // finished running.
    this.pageListener.addMessageListener("Load", this.receiveMessage.bind(this));
    this.pageListener.addMessageListener("RemotePage:Unload", this.receiveMessage.bind(this));
    this.pageListener.addMessageListener("closeTab", this.receiveMessage.bind(this));
    this.pageListener.addMessageListener("restoreTab", this.receiveMessage.bind(this));
    this.pageListener.addMessageListener("restoreAll", this.receiveMessage.bind(this));
  },

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "ipc:content-shutdown": {
        aSubject.QueryInterface(Ci.nsIPropertyBag2);

        if (!aSubject.get("abnormal")) {
          return;
        }

        let childID = aSubject.get("childID");
        let dumpID = aSubject.get("dumpID");

        if (!this.flushCrashedBrowserQueue(childID)) {
          this.unseenCrashedChildIDs.push(childID);
          // The elements in unseenCrashedChildIDs will only be removed if
          // the tab crash page is shown. However, ipc:content-shutdown might
          // be fired for processes for which we'll never show the tab crash
          // page - for example, the thumbnailing process. Another case to
          // consider is if the user is configured to submit backlogged crash
          // reports automatically, and a background tab crashes. In that case,
          // we will never show the tab crash page, and never remove the element
          // from the list.
          //
          // Instead of trying to account for all of those cases, we prevent
          // this list from getting too large by putting a reasonable upper
          // limit on how many childIDs we track. It's unlikely that this
          // array would ever get so large as to be unwieldy (that'd be a lot
          // or crashes!), but a leak is a leak.
          if (this.unseenCrashedChildIDs.length > MAX_UNSEEN_CRASHED_CHILD_IDS) {
            this.unseenCrashedChildIDs.shift();
          }
        }

        break;
      }
      case "oop-frameloader-crashed": {
        aSubject.QueryInterface(Ci.nsIFrameLoader);

        let browser = aSubject.ownerElement;
        if (!browser) {
          return;
        }

        this.browserMap.set(browser.permanentKey, aSubject.childID);
        break;
      }
    }
  },

  receiveMessage: function(message) {
    let browser = message.target.browser;
    let gBrowser = browser.ownerGlobal.gBrowser;
    let tab = gBrowser.getTabForBrowser(browser);

    switch (message.name) {
      case "Load": {
        this.onAboutTabCrashedLoad(message);
        break;
      }

      case "RemotePage:Unload": {
        this.onAboutTabCrashedUnload(message);
        break;
      }

      case "closeTab": {
        gBrowser.removeTab(tab, { animate: true });
        break;
      }

      case "restoreTab": {
        SessionStore.reviveCrashedTab(tab);
        break;
      }

      case "restoreAll": {
        SessionStore.reviveAllCrashedTabs();
        break;
      }
    }
  },

  /**
   * This should be called once a content process has finished
   * shutting down abnormally. Any tabbrowser browsers that were
   * selected at the time of the crash will then be sent to
   * the crashed tab page.
   *
   * @param childID (int)
   *        The childID of the content process that just crashed.
   * @returns boolean
   *        True if one or more browsers were sent to the tab crashed
   *        page.
   */
  flushCrashedBrowserQueue(childID) {
    let browserQueue = this.crashedBrowserQueues.get(childID);
    if (!browserQueue) {
      return false;
    }

    this.crashedBrowserQueues.delete(childID);

    let sentBrowser = false;
    for (let weakBrowser of browserQueue) {
      let browser = weakBrowser.get();
      if (browser) {
        this.sendToTabCrashedPage(browser);
        sentBrowser = true;
      }
    }

    return sentBrowser;
  },

  /**
   * Called by a tabbrowser when it notices that its selected browser
   * has crashed. This will queue the browser to show the tab crash
   * page once the content process has finished tearing down.
   *
   * @param browser (<xul:browser>)
   *        The selected browser that just crashed.
   */
  onSelectedBrowserCrash(browser) {
    if (!browser.isRemoteBrowser) {
      Cu.reportError("Selected crashed browser is not remote.")
      return;
    }
    if (!browser.frameLoader) {
      Cu.reportError("Selected crashed browser has no frameloader.");
      return;
    }

    let childID = browser.frameLoader.childID;
    let browserQueue = this.crashedBrowserQueues.get(childID);
    if (!browserQueue) {
      browserQueue = [];
      this.crashedBrowserQueues.set(childID, browserQueue);
    }
    // It's probably unnecessary to store this browser as a
    // weak reference, since the content process should complete
    // its teardown in the same tick of the event loop, and then
    // this queue will be flushed. The weak reference is to avoid
    // leaking browsers in case anything goes wrong during this
    // teardown process.
    browserQueue.push(Cu.getWeakReference(browser));
  },

  /**
   * This method is exposed for SessionStore to call if the user selects
   * a tab which will restore on demand. It's possible that the tab
   * is in this state because it recently crashed. If that's the case, then
   * it's also possible that the user has not seen the tab crash page for
   * that particular crash, in which case, we might show it to them instead
   * of restoring the tab.
   *
   * @param browser (<xul:browser>)
   *        A browser from a browser tab that the user has just selected
   *        to restore on demand.
   * @returns (boolean)
   *        True if TabCrashHandler will send the user to the tab crash
   *        page instead.
   */
  willShowCrashedTab(browser) {
    let childID = this.browserMap.get(browser.permanentKey);
    // We will only show the tab crash page if:
    // 1) We are aware that this browser crashed
    // 2) We know we've never shown the tab crash page for the
    //    crash yet
    if (childID && this.unseenCrashedChildIDs.indexOf(childID) != -1) {
      this.sendToTabCrashedPage(browser);
      return true;
    }

    return false;
  },

  /**
   * We show a special page to users when a normal browser tab has crashed.
   * This method should be called to send a browser to that page once the
   * process has completely closed.
   *
   * @param browser (<xul:browser>)
   *        The browser that has recently crashed.
   */
  sendToTabCrashedPage(browser) {
    let title = browser.contentTitle;
    let uri = browser.currentURI;
    let gBrowser = browser.ownerGlobal.gBrowser;
    let tab = gBrowser.getTabForBrowser(browser);
    // The tab crashed page is non-remote by default.
    gBrowser.updateBrowserRemoteness(browser, false);

    browser.setAttribute("crashedPageTitle", title);
    browser.docShell.displayLoadError(Cr.NS_ERROR_CONTENT_CRASHED, uri, null);
    browser.removeAttribute("crashedPageTitle");
    tab.setAttribute("crashed", true);
  },

  removeSubmitCheckboxesForSameCrash: function(childID) {
    let enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      let window = enumerator.getNext();
      if (!window.gMultiProcessBrowser)
        continue;

      for (let browser of window.gBrowser.browsers) {
        if (browser.isRemoteBrowser)
          continue;

        let doc = browser.contentDocument;
        if (!doc.documentURI.startsWith("about:tabcrashed"))
          continue;

        if (this.browserMap.get(browser.permanentKey) == childID) {
          this.browserMap.delete(browser.permanentKey);
          let ports = this.pageListener.portsForBrowser(browser);
          if (ports.length) {
            // For about:tabcrashed, we don't expect subframes. We can
            // assume sending to the first port is sufficient.
            ports[0].sendAsyncMessage("CrashReportSent");
          }
        }
      }
    }
  },

  onAboutTabCrashedLoad: function (message) {
    this._crashedTabCount++;

    // Broadcast to all about:tabcrashed pages a count of
    // how many about:tabcrashed pages exist, so that they
    // can decide whether or not to display the "Restore All
    // Crashed Tabs" button.
    this.pageListener.sendAsyncMessage("UpdateCount", {
      count: this._crashedTabCount,
    });

    let browser = message.target.browser;

    let childID = this.browserMap.get(browser.permanentKey);
    let index = this.unseenCrashedChildIDs.indexOf(childID);
    if (index != -1) {
      this.unseenCrashedChildIDs.splice(index, 1);
    }

    message.target.sendAsyncMessage("SetCrashReportAvailable", {
      hasReport: false,
    });
    return;
  },

  onAboutTabCrashedUnload(message) {
    if (!this._crashedTabCount) {
      Cu.reportError("Can not decrement crashed tab count to below 0");
      return;
    }
    this._crashedTabCount--;

    // Broadcast to all about:tabcrashed pages a count of
    // how many about:tabcrashed pages exist, so that they
    // can decide whether or not to display the "Restore All
    // Crashed Tabs" button.
    this.pageListener.sendAsyncMessage("UpdateCount", {
      count: this._crashedTabCount,
    });

    let browser = message.target.browser;
    let childID = this.browserMap.get(browser.permanentKey);

  },
}

this.PluginCrashReporter = {
  /**
   * Makes the PluginCrashReporter ready to hear about and
   * submit crash reports.
   */
  init() {
    if (this.initialized) {
      return;
    }

    this.initialized = true;
    this.crashReports = new Map();

    Services.obs.addObserver(this, "plugin-crashed", false);
    Services.obs.addObserver(this, "gmp-plugin-crash", false);
    Services.obs.addObserver(this, "profile-after-change", false);
  },

  uninit() {
    Services.obs.removeObserver(this, "plugin-crashed", false);
    Services.obs.removeObserver(this, "gmp-plugin-crash", false);
    Services.obs.removeObserver(this, "profile-after-change", false);
    this.initialized = false;
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "plugin-crashed": {
        let propertyBag = subject;
        if (!(propertyBag instanceof Ci.nsIPropertyBag2) ||
            !(propertyBag instanceof Ci.nsIWritablePropertyBag2) ||
            !propertyBag.hasKey("runID") ||
            !propertyBag.hasKey("pluginDumpID")) {
          Cu.reportError("PluginCrashReporter can not read plugin information.");
          return;
        }

        let runID = propertyBag.getPropertyAsUint32("runID");
        let pluginDumpID = propertyBag.getPropertyAsAString("pluginDumpID");
        let browserDumpID = propertyBag.getPropertyAsAString("browserDumpID");
        if (pluginDumpID) {
          this.crashReports.set(runID, { pluginDumpID, browserDumpID });
        }
        break;
      }
      case "gmp-plugin-crash": {
        let propertyBag = subject;
        if (!(propertyBag instanceof Ci.nsIWritablePropertyBag2) ||
            !propertyBag.hasKey("pluginID") ||
            !propertyBag.hasKey("pluginDumpID") ||
            !propertyBag.hasKey("pluginName")) {
          Cu.reportError("PluginCrashReporter can not read plugin information.");
          return;
        }

        let pluginID = propertyBag.getPropertyAsUint32("pluginID");
        let pluginDumpID = propertyBag.getPropertyAsAString("pluginDumpID");
        if (pluginDumpID) {
          this.crashReports.set(pluginID, { pluginDumpID });
        }

        // Only the parent process gets the gmp-plugin-crash observer
        // notification, so we need to inform any content processes that
        // the GMP has crashed.
        if (Cc["@mozilla.org/parentprocessmessagemanager;1"]) {
          let pluginName = propertyBag.getPropertyAsAString("pluginName");
          let mm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
            .getService(Ci.nsIMessageListenerManager);
          mm.broadcastAsyncMessage("gmp-plugin-crash",
                                   { pluginName, pluginID });
        }
        break;
      }
      case "profile-after-change":
        this.uninit();
        break;
    }
  },

  /**
   * Submit a crash report for a crashed NPAPI plugin.
   *
   * @param runID
   *        The runID of the plugin that crashed. A run ID is a unique
   *        identifier for a particular run of a plugin process - and is
   *        analogous to a process ID (though it is managed by Gecko instead
   *        of the operating system).
   * @param keyVals
   *        An object whose key-value pairs will be merged
   *        with the ".extra" file submitted with the report.
   *        The properties of htis object will override properties
   *        of the same name in the .extra file.
   */
  submitCrashReport(runID, keyVals) {
    if (!this.crashReports.has(runID)) {
      Cu.reportError(`Could not find plugin dump IDs for run ID ${runID}.` +
                     `It is possible that a report was already submitted.`);
      return;
    }

    keyVals = keyVals || {};
    let { pluginDumpID, browserDumpID } = this.crashReports.get(runID);

    let submissionPromise = CrashSubmit.submit(pluginDumpID, {
      recordSubmission: true,
      extraExtraKeyVals: keyVals,
    });

    if (browserDumpID)
      CrashSubmit.submit(browserDumpID);

    this.broadcastState(runID, "submitting");

    submissionPromise.then(() => {
      this.broadcastState(runID, "success");
    }, () => {
      this.broadcastState(runID, "failed");
    });

    this.crashReports.delete(runID);
  },

  broadcastState(runID, state) {
    let enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      let window = enumerator.getNext();
      let mm = window.messageManager;
      mm.broadcastAsyncMessage("BrowserPlugins:CrashReportSubmitted",
                               { runID, state });
    }
  },

  hasCrashReport(runID) {
    return this.crashReports.has(runID);
  },
};
