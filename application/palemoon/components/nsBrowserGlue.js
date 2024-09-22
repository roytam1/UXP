# -*- indent-tabs-mode: nil -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Define Lazy Service Getters
XPCOMUtils.defineLazyServiceGetter(this, "AlertsService",
                                   "@mozilla.org/alerts-service;1", "nsIAlertsService");

// Define Lazy Module Getters
[
  ["AddonManager", "resource://gre/modules/AddonManager.jsm"],
  ["NetUtil", "resource://gre/modules/NetUtil.jsm"],
  ["UserAgentOverrides", "resource://gre/modules/UserAgentOverrides.jsm"],
  ["FileUtils", "resource://gre/modules/FileUtils.jsm"],
  ["PlacesUtils", "resource://gre/modules/PlacesUtils.jsm"],
  ["BookmarkHTMLUtils", "resource://gre/modules/BookmarkHTMLUtils.jsm"],
  ["BookmarkJSONUtils", "resource://gre/modules/BookmarkJSONUtils.jsm"],
  ["PageThumbs", "resource://gre/modules/PageThumbs.jsm"],
  ["NewTabUtils", "resource://gre/modules/NewTabUtils.jsm"],
  ["BrowserNewTabPreloader", "resource:///modules/BrowserNewTabPreloader.jsm"],
#ifdef MOZ_WEBRTC
  ["webrtcUI", "resource:///modules/webrtcUI.jsm"],
#endif
  ["PrivateBrowsingUtils", "resource://gre/modules/PrivateBrowsingUtils.jsm"],
  ["RecentWindow", "resource:///modules/RecentWindow.jsm"],
  ["Task", "resource://gre/modules/Task.jsm"],
  ["PlacesBackups", "resource://gre/modules/PlacesBackups.jsm"],
  ["OS", "resource://gre/modules/osfile.jsm"],
  ["LoginManagerParent", "resource://gre/modules/LoginManagerParent.jsm"],
  ["FormValidationHandler", "resource:///modules/FormValidationHandler.jsm"],
  ["AutoCompletePopup", "resource:///modules/AutoCompletePopup.jsm"],
  ["DateTimePickerHelper", "resource://gre/modules/DateTimePickerHelper.jsm"],
  ["ShellService", "resource:///modules/ShellService.jsm"],
].forEach(([name, resource]) => XPCOMUtils.defineLazyModuleGetter(this, name, resource));

// Define Lazy Getters

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle('chrome://branding/locale/brand.properties');
});

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle('chrome://browser/locale/browser.properties');
});

// We try to backup bookmarks at idle times, to avoid doing that at shutdown.
// Number of idle seconds before trying to backup bookmarks.  15 minutes.
const BOOKMARKS_BACKUP_IDLE_TIME = 15 * 60;
// Minimum interval in milliseconds between backups.
const BOOKMARKS_BACKUP_INTERVAL = 86400 * 1000;
// Maximum number of backups to create.  Old ones will be purged.
const BOOKMARKS_BACKUP_MAX_BACKUPS = 10;

// Use users' idle time to unlink ghost windows and clean up memory.
// Trigger this by default every 5 minutes.
const GHOSTBUSTER_INTERVAL = 5 * 60;

// Factory object
const BrowserGlueServiceFactory = {
  _instance: null,
  createInstance: function(outer, iid) {
    if (outer != null) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }
    return this._instance == null ?
           this._instance = new BrowserGlue() :
           this._instance;
  }
};

// Constructor

function BrowserGlue() {
  XPCOMUtils.defineLazyServiceGetter(this, "_idleService",
                                     "@mozilla.org/widget/idleservice;1",
                                     "nsIIdleService");

  XPCOMUtils.defineLazyServiceGetter(this, "_ghostBusterService",
                                     "@mozilla.org/widget/idleservice;1",
                                     "nsIIdleService");

  XPCOMUtils.defineLazyGetter(this, "_distributionCustomizer", function() {
                                Cu.import("resource:///modules/distribution.js");
                                return new DistributionCustomizer();
                              });

  XPCOMUtils.defineLazyGetter(this, "_sanitizer",
    function() {
      let sanitizerScope = {};
      Services.scriptloader.loadSubScript("chrome://browser/content/sanitize.js", sanitizerScope);
      return sanitizerScope.Sanitizer;
    });

  this._init();
}

# We don't have the concept of zero-window sessions on any supported OS-es
# and therefore have to observe the browser-lastwindow-close-* topics.
#define OBSERVE_LASTWINDOW_CLOSE_TOPICS 1

BrowserGlue.prototype = {
  _saveSession: false,
  _isIdleObserver: false,
  _isPlacesInitObserver: false,
  _isPlacesLockedObserver: false,
  _isPlacesShutdownObserver: false,
  _isPlacesDatabaseLocked: false,
  _migrationImportsDefaultBookmarks: false,
  _isGhostBusterObserver: false,

  _setPrefToSaveSession: function(aForce) {
    if (!this._saveSession && !aForce) {
      return;
    }

    Services.prefs.setBoolPref("browser.sessionstore.resume_session_once", true);

    // This method can be called via [NSApplication terminate:] on Mac, which
    // ends up causing prefs not to be flushed to disk, so we need to do that
    // explicitly here. See bug 497652.
    Services.prefs.savePrefFile(null);
  },

#ifdef MOZ_SERVICES_SYNC
  _setSyncAutoconnectDelay: function() {
    // Assume that a non-zero value for services.sync.autoconnectDelay should override
    if (Services.prefs.prefHasUserValue("services.sync.autoconnectDelay")) {
      let prefDelay = Services.prefs.getIntPref("services.sync.autoconnectDelay");

      if (prefDelay > 0) {
        return;
      }
    }

    // delays are in seconds
    const MAX_DELAY = 300;
    let delay = 3;
    let browserEnum = Services.wm.getEnumerator("navigator:browser");
    while (browserEnum.hasMoreElements()) {
      delay += browserEnum.getNext().gBrowser.tabs.length;
    }
    delay = delay <= MAX_DELAY ? delay : MAX_DELAY;

    Cu.import("resource://services-sync/main.js");
    Weave.Service.scheduler.delayedAutoConnect(delay);
  },
#endif

  // nsIObserver implementation 
  observe: function(subject, topic, data) {
    switch (topic) {
      case "notifications-open-settings":
        this._openPermissions(subject);
        break;
      case "prefservice:after-app-defaults":
        this._onAppDefaults();
        break;
      case "final-ui-startup":
        this._finalUIStartup();
        break;
      case "browser-delayed-startup-finished":
        this._onFirstWindowLoaded();
        Services.obs.removeObserver(this, "browser-delayed-startup-finished");
        break;
      case "sessionstore-windows-restored":
        this._onWindowsRestored();
        break;
      case "browser:purge-session-history":
        // reset the console service's error buffer
        Services.console.logStringMessage(null); // clear the console (in case it's open)
        Services.console.reset();
        break;
      case "quit-application-requested":
        this._onQuitRequest(subject, data);
        break;
      case "quit-application-granted":
        // This pref must be set here because SessionStore will use its value
        // on quit-application.
        this._setPrefToSaveSession();
        try {
          let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                           getService(Ci.nsIAppStartup);
          appStartup.trackStartupCrashEnd();
        } catch(e) {
          Cu.reportError("Could not end startup crash tracking in quit-application-granted: " + e);
        }
        DateTimePickerHelper.uninit();
        break;
#ifdef OBSERVE_LASTWINDOW_CLOSE_TOPICS
      case "browser-lastwindow-close-requested":
        // The application is not actually quitting, but the last full browser
        // window is about to be closed.
        this._onQuitRequest(subject, "lastwindow");
        break;
      case "browser-lastwindow-close-granted":
        this._setPrefToSaveSession();
        break;
#endif
#ifdef MOZ_SERVICES_SYNC
      case "weave:service:ready":
        this._setSyncAutoconnectDelay();
        break;
      case "weave:engine:clients:display-uri":
        this._onDisplaySyncURI(subject);
        break;
#endif
      case "session-save":
        this._setPrefToSaveSession(true);
        subject.QueryInterface(Ci.nsISupportsPRBool);
        subject.data = true;
        break;
      case "places-init-complete":
        if (!this._migrationImportsDefaultBookmarks) {
          this._initPlaces(false);
        }

        Services.obs.removeObserver(this, "places-init-complete");
        this._isPlacesInitObserver = false;
        // no longer needed, since history was initialized completely.
        Services.obs.removeObserver(this, "places-database-locked");
        this._isPlacesLockedObserver = false;
        break;
      case "places-database-locked":
        this._isPlacesDatabaseLocked = true;
        // Stop observing, so further attempts to load history service
        // will not show the prompt.
        Services.obs.removeObserver(this, "places-database-locked");
        this._isPlacesLockedObserver = false;
        break;
      case "places-shutdown":
        if (this._isPlacesShutdownObserver) {
          Services.obs.removeObserver(this, "places-shutdown");
          this._isPlacesShutdownObserver = false;
        }
        // places-shutdown is fired when the profile is about to disappear.
        this._onPlacesShutdown();
        break;
      case "idle":
        if (this._idleService.idleTime > BOOKMARKS_BACKUP_IDLE_TIME * 1000) {
          this._backupBookmarks();
        }
        if (this._ghostBusterService.idleTime > GHOSTBUSTER_INTERVAL * 1000) {
          if (Services.prefs.getBoolPref("browser.ghostbuster.enabled", true)) {
            Cu.unlinkGhostWindows();
            Cu.forceGC();
#ifdef DEBUG
            dump("Unlinking ghost windows + GC has run on idle.\n");
#endif
          }
        }
        break;
      case "distribution-customization-complete":
        Services.obs.removeObserver(this, "distribution-customization-complete");
        // Customization has finished, we don't need the customizer anymore.
        delete this._distributionCustomizer;
        break;
      case "browser-glue-test": // used by tests
        if (data == "post-update-notification") {
          if (Services.prefs.prefHasUserValue("app.update.postupdate")) {
            this._showUpdateNotification();
          }
        } else if (data == "force-ui-migration") {
          this._migrateUI();
        } else if (data == "force-distribution-customization") {
          this._distributionCustomizer.applyPrefDefaults();
          this._distributionCustomizer.applyCustomizations();
          // To apply distribution bookmarks use "places-init-complete".
        } else if (data == "force-places-init") {
          this._initPlaces(false);
        }
        break;
      case "initial-migration-will-import-default-bookmarks":
        this._migrationImportsDefaultBookmarks = true;
        break;
      case "initial-migration-did-import-default-bookmarks":
        this._initPlaces(true);
        break;
      case "handle-xul-text-link":
        let linkHandled = subject.QueryInterface(Ci.nsISupportsPRBool);
        if (!linkHandled.data) {
          let win = this.getMostRecentBrowserWindow();
          if (win) {
            data = JSON.parse(data);
            win.openUILinkIn(data.href, "tab");
            linkHandled.data = true;
          }
        }
        break;
      case "profile-before-change":
        this._onProfileShutdown();
        break;
      case "profile-after-change":
         this._onProfileAfterChange();
         this._promptForMasterPassword();
         break;
      case "browser-search-engine-modified":
        if (data != "engine-default" && data != "engine-current") {
          break;
        }
        // Enforce that the search service's defaultEngine is always equal to
        // its currentEngine. The search service will notify us any time either
        // of them are changed (either by directly setting the relevant prefs,
        // i.e. if add-ons try to change this directly, or if the
        // nsIBrowserSearchService setters are called).
        // No need to initialize the search service, since it's guaranteed to be
        // initialized already when this notification fires.
        let ss = Services.search;
        if (ss.currentEngine.name == ss.defaultEngine.name) {
          return;
        }
        if (data == "engine-current") {
          ss.defaultEngine = ss.currentEngine;
        } else {
          ss.currentEngine = ss.defaultEngine;
        }
        break;
      case "browser-search-service":
        if (data != "init-complete") {
          return;
        }
        Services.obs.removeObserver(this, "browser-search-service");
        this._syncSearchEngines();
        break;
    }
  },

  _syncSearchEngines: function() {
    // Only do this if the search service is already initialized. This function
    // gets called in finalUIStartup and from a browser-search-service observer,
    // to catch both cases (search service initialization occurring before and
    // after final-ui-startup)
    if (Services.search.isInitialized) {
      Services.search.defaultEngine = Services.search.currentEngine;
    }
  },

  // initialization (called on application startup) 
  _init: function() {
    let os = Services.obs;
    os.addObserver(this, "notifications-open-settings", false);
    os.addObserver(this, "prefservice:after-app-defaults", false);
    os.addObserver(this, "final-ui-startup", false);
    os.addObserver(this, "browser-delayed-startup-finished", false);
    os.addObserver(this, "sessionstore-windows-restored", false);
    os.addObserver(this, "browser:purge-session-history", false);
    os.addObserver(this, "quit-application-requested", false);
    os.addObserver(this, "quit-application-granted", false);
#ifdef OBSERVE_LASTWINDOW_CLOSE_TOPICS
    os.addObserver(this, "browser-lastwindow-close-requested", false);
    os.addObserver(this, "browser-lastwindow-close-granted", false);
#endif
#ifdef MOZ_SERVICES_SYNC
    os.addObserver(this, "weave:service:ready", false);
    os.addObserver(this, "weave:engine:clients:display-uri", false);
#endif
    os.addObserver(this, "session-save", false);
    os.addObserver(this, "places-init-complete", false);
    this._isPlacesInitObserver = true;
    os.addObserver(this, "places-database-locked", false);
    this._isPlacesLockedObserver = true;
    os.addObserver(this, "distribution-customization-complete", false);
    os.addObserver(this, "places-shutdown", false);
    this._isPlacesShutdownObserver = true;
    os.addObserver(this, "handle-xul-text-link", false);
    os.addObserver(this, "profile-before-change", false);
    os.addObserver(this, "profile-after-change", false);
    os.addObserver(this, "browser-search-engine-modified", false);
    os.addObserver(this, "browser-search-service", false);
  },

  // cleanup (called on application shutdown)
  _dispose: function() {
    let os = Services.obs;
    os.removeObserver(this, "notifications-open-settings");
    os.removeObserver(this, "prefservice:after-app-defaults");
    os.removeObserver(this, "final-ui-startup");
    os.removeObserver(this, "sessionstore-windows-restored");
    os.removeObserver(this, "browser:purge-session-history");
    os.removeObserver(this, "quit-application-requested");
    os.removeObserver(this, "quit-application-granted");
#ifdef OBSERVE_LASTWINDOW_CLOSE_TOPICS
    os.removeObserver(this, "browser-lastwindow-close-requested");
    os.removeObserver(this, "browser-lastwindow-close-granted");
#endif
#ifdef MOZ_SERVICES_SYNC
    os.removeObserver(this, "weave:service:ready");
    os.removeObserver(this, "weave:engine:clients:display-uri");
#endif
    os.removeObserver(this, "session-save");
    if (this._isIdleObserver) {
      this._idleService.removeIdleObserver(this, BOOKMARKS_BACKUP_IDLE_TIME);
    }
    if (this._isPlacesInitObserver) {
      os.removeObserver(this, "places-init-complete");
    }
    if (this._isPlacesLockedObserver) {
      os.removeObserver(this, "places-database-locked");
    }
    if (this._isPlacesShutdownObserver) {
      os.removeObserver(this, "places-shutdown");
    }
    os.removeObserver(this, "handle-xul-text-link");
    os.removeObserver(this, "profile-before-change");
    os.removeObserver(this, "profile-after-change");
    os.removeObserver(this, "browser-search-engine-modified");
    try {
      os.removeObserver(this, "browser-search-service");
    } catch(ex) {
      // may have already been removed by the observer
    }
  },

  // profile is available
  _onProfileAfterChange: function() {
    this._copyDefaultProfileFiles();
  },
  
  _promptForMasterPassword: function() {
    if (!Services.prefs.getBoolPref("signon.startup.prompt", false))
      return;

    // Try to avoid the multiple master password prompts on startup scenario
    // by prompting for the master password upfront.
    let token = Components.classes["@mozilla.org/security/pk11tokendb;1"]
                          .getService(Components.interfaces.nsIPK11TokenDB)
                          .getInternalKeyToken();

    // Only log in to the internal token if it is already initialized,
    // otherwise we get a "Change Master Password" dialog.
    try {
      if (!token.needsUserInit)
        token.login(false);
    } catch (ex) {
      // If user cancels an exception is expected.
    }
  },

  _onAppDefaults: function() {
    // apply distribution customizations (prefs)
    // other customizations are applied in _finalUIStartup()
    this._distributionCustomizer.applyPrefDefaults();
  },

  // runs on startup, before the first command line handler is invoked
  // (i.e. before the first window is opened)
  _finalUIStartup: function() {
    this._sanitizer.onStartup();
    // check if we're in safe mode
    if (Services.appinfo.inSafeMode) {
      Services.ww.openWindow(null, "chrome://browser/content/safeMode.xul", 
                             "_blank", "chrome,centerscreen,modal,resizable=no", null);
    }

    // apply distribution customizations
    // prefs are applied in _onAppDefaults()
    this._distributionCustomizer.applyCustomizations();

    // handle any UI migration
    this._migrateUI();

    this._setUpUserAgentOverrides();

    this._syncSearchEngines();

    PageThumbs.init();
    NewTabUtils.init();
    BrowserNewTabPreloader.init();
#ifdef MOZ_WEBRTC
    webrtcUI.init();
#endif
    FormValidationHandler.init();
    
    AutoCompletePopup.init();
    
    LoginManagerParent.init();
    
    Services.obs.notifyObservers(null, "browser-ui-startup-complete", "");
  },

  // Copies additional profile files from the default profile to the current profile.
  // Only files not covered by the regular profile creation process.
  // Currently only the userchrome examples.
  _copyDefaultProfileFiles: function() {
    // Copy default chrome example files if they do not exist in the current profile.
    var profileDir = Services.dirsvc.get("ProfD", Components.interfaces.nsILocalFile);
    profileDir.append("chrome");

    // The chrome directory in the current/new profile already exists so no copying.
    if (profileDir.exists())
      return;

    let defaultProfileDir = Services.dirsvc.get("DefRt",
                                                Components.interfaces.nsIFile);
    defaultProfileDir.append("profile");
    defaultProfileDir.append("chrome");

    if (defaultProfileDir.exists() && defaultProfileDir.isDirectory()) {
      try {
        this._copyDir(defaultProfileDir, profileDir);
      } catch (e) {
        Components.utils.reportError(e);
      }
    }
  },

  // Simple copy function for copying complete aSource Directory to aDestiniation.
  _copyDir: function(aSource, aDestination)
  {
    let enumerator = aSource.directoryEntries;

    while (enumerator.hasMoreElements()) {
      let file = enumerator.getNext().QueryInterface(Components.interfaces.nsIFile);

      if (file.isDirectory()) {
        let subdir = aDestination.clone();
        subdir.append(file.leafName);

        // Create the target directory. If it already exists continue copying files.
        try {
          subdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE,
                        FileUtils.PERMS_DIRECTORY);
        } catch (ex) {
           if (ex.result != Components.results.NS_ERROR_FILE_ALREADY_EXISTS)
            throw ex;
        }
        // Directory created. Now copy the files.
        this._copyDir(file, subdir);
      } else {
        try {
          file.copyTo(aDestination, null);
        } catch (e) {
          Components.utils.reportError(e);
        }
      }
    }
  },

  _setUpUserAgentOverrides: function() {
    UserAgentOverrides.init();

    if (Services.prefs.getBoolPref("general.useragent.complexOverride.moodle")) {
      UserAgentOverrides.addComplexOverride(function(aHttpChannel, aOriginalUA) {
        let cookies;
        try {
          cookies = aHttpChannel.getRequestHeader("Cookie");
        } catch(e) {
          // no cookie sent
        }
        if (cookies && cookies.indexOf("MoodleSession") > -1) {
          return aOriginalUA.replace(/Goanna\/[^ ]*/, "Goanna/20100101");
        }
        return null;
      });
    }
  },

  _trackSlowStartup: function() {
    if (Services.startup.interrupted ||
        Services.prefs.getBoolPref("browser.slowStartup.notificationDisabled")) {
      return;
    }

    let currentTime = Date.now() - Services.startup.getStartupInfo().process;
    let averageTime = 0;
    let samples = 0;
    try {
      averageTime = Services.prefs.getIntPref("browser.slowStartup.averageTime");
      samples = Services.prefs.getIntPref("browser.slowStartup.samples");
    } catch(e) {}

    averageTime = (averageTime * samples + currentTime) / ++samples;

    if (samples >= Services.prefs.getIntPref("browser.slowStartup.maxSamples")) {
      if (averageTime > Services.prefs.getIntPref("browser.slowStartup.timeThreshold")) {
        this._showSlowStartupNotification();
      }
      averageTime = 0;
      samples = 0;
    }

    Services.prefs.setIntPref("browser.slowStartup.averageTime", averageTime);
    Services.prefs.setIntPref("browser.slowStartup.samples", samples);
  },

  _showSlowStartupNotification: function() {
    let win = this.getMostRecentBrowserWindow();
    if (!win) {
      return;
    }

    let productName = gBrandBundle.GetStringFromName("brandFullName");
    let message = win.gNavigatorBundle.getFormattedString("slowStartup.message", [productName]);

    let buttons = [
      {
        label:     win.gNavigatorBundle.getString("slowStartup.helpButton.label"),
        accessKey: win.gNavigatorBundle.getString("slowStartup.helpButton.accesskey"),
        callback: function() {
          win.openUILinkIn(Services.prefs.getCharPref("browser.slowstartup.help.url"), "tab");
        }
      },
      {
        label:     win.gNavigatorBundle.getString("slowStartup.disableNotificationButton.label"),
        accessKey: win.gNavigatorBundle.getString("slowStartup.disableNotificationButton.accesskey"),
        callback: function() {
          Services.prefs.setBoolPref("browser.slowStartup.notificationDisabled", true);
        }
      }
    ];

    let nb = win.document.getElementById("global-notificationbox");
    nb.appendNotification(message, "slow-startup",
                          "chrome://browser/skin/slowStartup-16.png",
                          nb.PRIORITY_INFO_LOW, buttons);
  },

  // the first browser window has finished initializing
  _onFirstWindowLoaded: function() {
#ifdef XP_WIN
    // For Windows, initialize the jump list module.
    const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";
    if (WINTASKBAR_CONTRACTID in Cc &&
        Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar).available) {
      let temp = {};
      Cu.import("resource:///modules/WindowsJumpLists.jsm", temp);
      temp.WinTaskbarJumpList.startup();
    }
#endif

    DateTimePickerHelper.init();

    this._trackSlowStartup();

    // Initialize ghost window idle observer.
    if (!this._isGhostBusterObserver) {
      this._ghostBusterService.addIdleObserver(this, GHOSTBUSTER_INTERVAL);
      // Prevent re-entry.
      this._isGhostBusterObserver = true;
    }
  },

  /**
   * Profile shutdown handler (contains profile cleanup routines).
   * All components depending on Places should be shut down in
   * _onPlacesShutdown() and not here.
   */
  _onProfileShutdown: function() {
    BrowserNewTabPreloader.uninit();
    UserAgentOverrides.uninit();
#ifdef MOZ_WEBRTC
    webrtcUI.uninit();
#endif
    FormValidationHandler.uninit();
    AutoCompletePopup.uninit();
    this._dispose();

    // Shut down ghost window idle observer.
    if (this._isGhostBusterObserver) {
      this._ghostBusterService.removeIdleObserver(this, GHOSTBUSTER_INTERVAL);
      this._isGhostBusterObserver = false;
    }
    // Do one final unlink to combat shutdown issues.
    Cu.unlinkGhostWindows();
  },

  // All initial windows have opened.
  _onWindowsRestored: function() {
    // Show update notification, if needed.
    if (Services.prefs.prefHasUserValue("app.update.postupdate")) {
      this._showUpdateNotification();
    }

    // Load the "more info" page for a locked places.sqlite
    // This property is set earlier by places-database-locked topic.
    if (this._isPlacesDatabaseLocked) {
      this._showPlacesLockedNotificationBox();
    }

    // For any add-ons that were installed disabled and can be enabled offer
    // them to the user.
    let changedIDs = AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_INSTALLED);
    if (changedIDs.length > 0) {
      let win = this.getMostRecentBrowserWindow();
      AddonManager.getAddonsByIDs(changedIDs, function(aAddons) {
        aAddons.forEach(function(aAddon) {
          // If the add-on isn't user disabled or can't be enabled then skip it.
          if (!aAddon.userDisabled || !(aAddon.permissions & AddonManager.PERM_CAN_ENABLE)) {
            return;
          }

          win.openUILinkIn("about:newaddon?id=" + aAddon.id, "tab");
        })
      });
    }

    // Perform default browser checking.
    if (ShellService) {
      let shouldCheck = ShellService.shouldCheckDefaultBrowser;

      const skipDefaultBrowserCheck =
        Services.prefs.getBoolPref("browser.shell.skipDefaultBrowserCheckOnFirstRun") &&
        Services.prefs.getBoolPref("browser.shell.skipDefaultBrowserCheck");

      const usePromptLimit = false;
      let promptCount =
        usePromptLimit ? Services.prefs.getIntPref("browser.shell.defaultBrowserCheckCount") : 0;

      let willRecoverSession = false;
      try {
        let ss = Cc["@mozilla.org/browser/sessionstartup;1"].
                 getService(Ci.nsISessionStartup);
        willRecoverSession =
          (ss.sessionType == Ci.nsISessionStartup.RECOVER_SESSION);
      } catch(ex) {
        // never mind; suppose SessionStore is broken
      }

      // startup check, check all assoc
      let isDefault = false;
      let isDefaultError = false;
      try {
        isDefault = ShellService.isDefaultBrowser(true, false);
      } catch(ex) {
        isDefaultError = true;
      }

      if (isDefault) {
        let now = (Math.floor(Date.now() / 1000)).toString();
        Services.prefs.setCharPref("browser.shell.mostRecentDateSetAsDefault", now);
      }

      let willPrompt = shouldCheck && !isDefault && !willRecoverSession;

      // Skip the "Set Default Browser" check during first-run or after the
      // browser has been run a few times.
      if (willPrompt) {
        Services.tm.mainThread.dispatch(function() {
          var win = this.getMostRecentBrowserWindow();
          var brandBundle = win.document.getElementById("bundle_brand");
          var shellBundle = win.document.getElementById("bundle_shell");

          var brandShortName = brandBundle.getString("brandShortName");
          var promptTitle = shellBundle.getString("setDefaultBrowserTitle");
          var promptMessage = shellBundle.getFormattedString("setDefaultBrowserMessage",
                                                             [brandShortName]);
          var checkboxLabel = shellBundle.getFormattedString("setDefaultBrowserDontAsk",
                                                             [brandShortName]);
          var checkEveryTime = { value: shouldCheck };
          var ps = Services.prompt;
          var rv = ps.confirmEx(win, promptTitle, promptMessage,
                                ps.STD_YES_NO_BUTTONS,
                                null, null, null, checkboxLabel, checkEveryTime);
          if (rv == 0) {
            var claimAllTypes = true;
#ifdef XP_WIN
            try {
              // In Windows 8+, the UI for selecting default protocol is much
              // nicer than the UI for setting file type associations. So we
              // only show the protocol association screen on Windows 8.
              // Windows 8 is version 6.2.
              let version = Services.sysinfo.getProperty("version");
              claimAllTypes = (parseFloat(version) < 6.2);
            } catch (ex) {}
#endif
            ShellService.setDefaultBrowser(claimAllTypes, false);
          }
          ShellService.shouldCheckDefaultBrowser = checkEveryTime.value;
        }.bind(this), Ci.nsIThread.DISPATCH_NORMAL);
      }
    }
  },

  _onQuitRequest: function(aCancelQuit, aQuitType) {
    // If user has already dismissed quit request, then do nothing
    if ((aCancelQuit instanceof Ci.nsISupportsPRBool) && aCancelQuit.data) {
      return;
    }

    // There are several cases where we won't show a dialog here:
    // 1. There is only 1 tab open in 1 window
    // 2. The session will be restored at startup, indicated by
    //    browser.startup.page == 3 or browser.sessionstore.resume_session_once == true
    // 3. browser.warnOnQuit == false
    // 4. The browser is currently in Private Browsing mode
    // 5. The browser will be restarted.
    //
    // Otherwise these are the conditions and the associated dialogs that will be shown:
    // 1. aQuitType == "lastwindow" or "quit" and browser.showQuitWarning == true
    //    - The quit dialog will be shown
    // 2. aQuitType == "lastwindow" && browser.tabs.warnOnClose == true
    //    - The "closing multiple tabs" dialog will be shown
    //
    // aQuitType == "lastwindow" is overloaded. "lastwindow" is used to indicate
    // "the last window is closing but we're not quitting (a non-browser window is open)"
    // and also "we're quitting by closing the last window".

    if (aQuitType == "restart") {
      return;
    }

    var windowcount = 0;
    var pagecount = 0;
    var browserEnum = Services.wm.getEnumerator("navigator:browser");
    let allWindowsPrivate = true;
    while (browserEnum.hasMoreElements()) {
      windowcount++;

      var browser = browserEnum.getNext();
      if (!PrivateBrowsingUtils.isWindowPrivate(browser)) {
        allWindowsPrivate = false;
      }
      var tabbrowser = browser.document.getElementById("content");
      if (tabbrowser) {
        pagecount += tabbrowser.browsers.length - tabbrowser._numPinnedTabs;
      }
    }

    this._saveSession = false;
    if (pagecount < 2) {
      return;
    }

    if (!aQuitType) {
      aQuitType = "quit";
    }

    // browser.warnOnQuit is a hidden global boolean to override all quit prompts
    // browser.showQuitWarning specifically covers quitting
    // browser.tabs.warnOnClose is the global "warn when closing multiple tabs" pref

    var sessionWillBeRestored = Services.prefs.getIntPref("browser.startup.page") == 3 ||
                                Services.prefs.getBoolPref("browser.sessionstore.resume_session_once");
    if (sessionWillBeRestored || !Services.prefs.getBoolPref("browser.warnOnQuit")) {
      return;
    }

    let win = Services.wm.getMostRecentWindow("navigator:browser");

    // On last window close or quit && showQuitWarning, we want to show the
    // quit warning.
    if (!Services.prefs.getBoolPref("browser.showQuitWarning")) {
      if (aQuitType == "lastwindow") {
        // If aQuitType is "lastwindow" and we aren't showing the quit warning,
        // we should show the window closing warning instead. warnAboutClosing
        // tabs checks browser.tabs.warnOnClose and returns if it's ok to close
        // the window. It doesn't actually close the window.
        aCancelQuit.data =
          !win.gBrowser.warnAboutClosingTabs(win.gBrowser.closingTabsEnum.ALL);
      }
      return;
    }

    let prompt = Services.prompt;
    let quitBundle = Services.strings.createBundle("chrome://browser/locale/quitDialog.properties");

    let appName = gBrandBundle.GetStringFromName("brandShortName");
    let quitDialogTitle = quitBundle.formatStringFromName("quitDialogTitle",
                                                          [appName], 1);
    let neverAskText = quitBundle.GetStringFromName("neverAsk2");
    let neverAsk = {value: false};

    let choice;
    if (allWindowsPrivate) {
      let text = quitBundle.formatStringFromName("messagePrivate", [appName], 1);
      let flags = prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_0 +
                  prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_1 +
                  prompt.BUTTON_POS_0_DEFAULT;
      choice = prompt.confirmEx(win, quitDialogTitle, text, flags,
                                quitBundle.GetStringFromName("quitTitle"),
                                quitBundle.GetStringFromName("cancelTitle"),
                                null,
                                neverAskText, neverAsk);

      // The order of the buttons differs between the prompt.confirmEx calls
      // here so we need to fix this for proper handling below.
      if (choice == 0) {
        choice = 2;
      }
    } else {
      let text = quitBundle.formatStringFromName(
        windowcount == 1 ? "messageNoWindows" : "message", [appName], 1);
      let flags = prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_0 +
                  prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_1 +
                  prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_2 +
                  prompt.BUTTON_POS_0_DEFAULT;
      choice = prompt.confirmEx(win, quitDialogTitle, text, flags,
                                quitBundle.GetStringFromName("saveTitle"),
                                quitBundle.GetStringFromName("cancelTitle"),
                                quitBundle.GetStringFromName("quitTitle"),
                                neverAskText, neverAsk);
    }

    switch (choice) {
      case 2: // Quit
        if (neverAsk.value) {
          Services.prefs.setBoolPref("browser.showQuitWarning", false);
        }
        break;
      case 1: // Cancel
        aCancelQuit.QueryInterface(Ci.nsISupportsPRBool);
        aCancelQuit.data = true;
        break;
      case 0: // Save & Quit
        this._saveSession = true;
        if (neverAsk.value) {
          // always save state when shutting down
          Services.prefs.setIntPref("browser.startup.page", 3);
        }
        break;
    }
  },

  _showUpdateNotification: function() {
    Services.prefs.clearUserPref("app.update.postupdate");

    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    try {
      // If the updates.xml file is deleted then getUpdateAt will throw.
      var update = um.getUpdateAt(0).QueryInterface(Ci.nsIPropertyBag);
    } catch(e) {
      // This should never happen.
      Cu.reportError("Unable to find update: " + e);
      return;
    }

    var actions = update.getProperty("actions");
    if (!actions || actions.indexOf("silent") != -1) {
      return;
    }

    var formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"]
                      .getService(Ci.nsIURLFormatter);
    var appName = gBrandBundle.GetStringFromName("brandShortName");

    function getNotifyString(aPropData) {
      var propValue = update.getProperty(aPropData.propName);
      if (!propValue) {
        if (aPropData.prefName) {
          propValue = formatter.formatURLPref(aPropData.prefName);
        } else if (aPropData.stringParams) {
          propValue = gBrowserBundle.formatStringFromName(aPropData.stringName,
                                                          aPropData.stringParams,
                                                          aPropData.stringParams.length);
        } else {
          propValue = gBrowserBundle.GetStringFromName(aPropData.stringName);
        }
      }
      return propValue;
    }

    if (actions.indexOf("showNotification") != -1) {
      let text = getNotifyString({propName: "notificationText",
                                  stringName: "puNotifyText",
                                  stringParams: [appName]});
      let url = getNotifyString({propName: "notificationURL",
                                 prefName: "startup.homepage_override_url"});
      let label = getNotifyString({propName: "notificationButtonLabel",
                                   stringName: "pu.notifyButton.label"});
      let key = getNotifyString({propName: "notificationButtonAccessKey",
                                 stringName: "pu.notifyButton.accesskey"});

      let win = this.getMostRecentBrowserWindow();
      let notifyBox = win.gBrowser.getNotificationBox();

      let buttons = [
                      {
                        label:     label,
                        accessKey: key,
                        popup:     null,
                        callback: function(aNotificationBar, aButton) {
                          win.openUILinkIn(url, "tab");
                        }
                      }
                    ];

      let notification = notifyBox.appendNotification(text, "post-update-notification",
                                                      null, notifyBox.PRIORITY_INFO_LOW,
                                                      buttons);
      notification.persistence = -1; // Until user closes it
    }

    if (actions.indexOf("showAlert") == -1) {
      return;
    }

    let title = getNotifyString({ propName: "alertTitle",
                                  stringName: "puAlertTitle",
                                  stringParams: [appName] });
    let text = getNotifyString({ propName: "alertText",
                                 stringName: "puAlertText",
                                 stringParams: [appName] });
    let url = getNotifyString({ propName: "alertURL",
                                prefName: "startup.homepage_override_url" });

    var self = this;
    function clickCallback(subject, topic, data) {
      // This callback will be called twice but only once with this topic
      if (topic != "alertclickcallback") {
        return;
      }
      let win = self.getMostRecentBrowserWindow();
      win.openUILinkIn(data, "tab");
    }

    try {
      // This will throw NS_ERROR_NOT_AVAILABLE if the notification cannot
      // be displayed per the idl.
      AlertsService.showAlertNotification(null, title, text,
                                          true, url, clickCallback);
    } catch(e) {
      Cu.reportError(e);
    }
  },

  /**
   * Initialize Places
   * - imports the bookmarks html file if bookmarks database is empty, try to
   *   restore bookmarks from a JSON/JSONLZ4 backup if the backend indicates
   *   that the database was corrupt.
   *
   * These prefs can be set up by the frontend:
   *
   * WARNING: setting these preferences to true will overwite existing bookmarks
   *
   * - browser.places.importBookmarksHTML
   *   Set to true will import the bookmarks.html file from the profile folder.
   * - browser.places.smartBookmarksVersion
   *   Set during HTML import to indicate that Smart Bookmarks were created.
   *   Set to -1 to disable Smart Bookmarks creation.
   *   Set to 0 to restore current Smart Bookmarks.
   * - browser.bookmarks.restore_default_bookmarks
   *   Set to true by safe-mode dialog to indicate we must restore default
   *   bookmarks.
   */
  _initPlaces: function(aInitialMigrationPerformed) {
    // We must instantiate the history service since it will tell us if we
    // need to import or restore bookmarks due to first-run, corruption or
    // forced migration (due to a major schema change).
    // If the database is corrupt or has been newly created we should
    // import bookmarks.
    var dbStatus = PlacesUtils.history.databaseStatus;
    var importBookmarks = !aInitialMigrationPerformed &&
                          (dbStatus == PlacesUtils.history.DATABASE_STATUS_CREATE ||
                           dbStatus == PlacesUtils.history.DATABASE_STATUS_CORRUPT);

    // Check if user or an extension has required to import bookmarks.html
    var importBookmarksHTML = false;
    try {
      importBookmarksHTML =
        Services.prefs.getBoolPref("browser.places.importBookmarksHTML");
      if (importBookmarksHTML)
        importBookmarks = true;
    } catch(ex) {}

    Task.spawn(function() {
      // Check if Safe Mode or the user has required to restore bookmarks from
      // default profile's bookmarks.html
      var restoreDefaultBookmarks = false;
      try {
        restoreDefaultBookmarks =
          Services.prefs.getBoolPref("browser.bookmarks.restore_default_bookmarks");
        if (restoreDefaultBookmarks) {
          // Ensure that we already have a bookmarks backup for today.
          yield this._backupBookmarks();
          importBookmarks = true;
        }
      } catch(ex) {}

      // If the user did not require to restore default bookmarks, or import
      // from bookmarks.html, we will try to restore from JSON/JSONLZ4
      if (importBookmarks && !restoreDefaultBookmarks && !importBookmarksHTML) {
        // get latest JSON/JSONLZ4 backup
        var bookmarksBackupFile = yield PlacesBackups.getMostRecentBackup();
        if (bookmarksBackupFile) {
          // restore from JSON/JSONLZ4 backup
          yield BookmarkJSONUtils.importFromFile(bookmarksBackupFile, true);
          importBookmarks = false;
        } else {
          // We have created a new database but we don't have any backup available
          importBookmarks = true;
          if (yield OS.File.exists(BookmarkHTMLUtils.defaultPath)) {
            // If bookmarks.html is available in current profile import it...
            importBookmarksHTML = true;
          } else {
            // ...otherwise we will restore defaults
            restoreDefaultBookmarks = true;
          }
        }
      }

      // If bookmarks are not imported, then initialize smart bookmarks.  This
      // happens during a common startup.
      // Otherwise, if any kind of import runs, smart bookmarks creation should be
      // delayed till the import operations has finished.  Not doing so would
      // cause them to be overwritten by the newly imported bookmarks.
      if (!importBookmarks) {
        // Now apply distribution customized bookmarks.
        // This should always run after Places initialization.
        try {
          this._distributionCustomizer.applyBookmarks();
          this.ensurePlacesDefaultQueriesInitialized();
        } catch(e) {
          Cu.reportError(e);
        }
      } else {
        // An import operation is about to run.
        // Don't try to recreate smart bookmarks if autoExportHTML is true or
        // smart bookmarks are disabled.
        var autoExportHTML = Services.prefs.getBoolPref("browser.bookmarks.autoExportHTML", false);
        var smartBookmarksVersion = Services.prefs.getIntPref("browser.places.smartBookmarksVersion", 0);
        if (!autoExportHTML && smartBookmarksVersion != -1) {
          Services.prefs.setIntPref("browser.places.smartBookmarksVersion", 0);
        }

        var bookmarksUrl = null;
        if (restoreDefaultBookmarks) {
          // User wants to restore bookmarks.html file from default profile folder
          bookmarksUrl = "resource:///defaults/profile/bookmarks.html";
        } else if (yield OS.File.exists(BookmarkHTMLUtils.defaultPath)) {
          bookmarksUrl = OS.Path.toFileURI(BookmarkHTMLUtils.defaultPath);
        }

        if (bookmarksUrl) {
          // Import from bookmarks.html file.
          try {
            BookmarkHTMLUtils.importFromURL(bookmarksUrl, true).then(
              null,
              function onFailure() {
                Cu.reportError(new Error("Bookmarks.html file could be corrupt."));
              }
            ).then(
              function onComplete() {
                try {
                  // Now apply distribution customized bookmarks.
                  // This should always run after Places initialization.
                  this._distributionCustomizer.applyBookmarks();
                  // Ensure that smart bookmarks are created once the operation
                  // is complete.
                  this.ensurePlacesDefaultQueriesInitialized();
                } catch(e) {
                  Cu.reportError(e);
                }
              }.bind(this)
            );
          } catch(e) {
            Cu.reportError(
                new Error("Bookmarks.html file could be corrupt." + "\n" +
                e.message));
          }
        } else {
          Cu.reportError(new Error("Unable to find bookmarks.html file."));
        }

        // See #1083:
        // "Delete all bookmarks except for backups" in Safe Mode doesn't work
        var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        let observer = {
          "observe": function() {
            delete observer.timer;
            // Reset preferences, so we won't try to import again at next run
            if (importBookmarksHTML) {
              Services.prefs.setBoolPref("browser.places.importBookmarksHTML", false);
            }
            if (restoreDefaultBookmarks) {
              Services.prefs.setBoolPref("browser.bookmarks.restore_default_bookmarks",
                                         false);
            }
          },
          "timer": timer,
        };
        timer.init(observer, 100, Ci.nsITimer.TYPE_ONE_SHOT);
      }

      // Initialize bookmark archiving on idle.
      // Once a day, either on idle or shutdown, bookmarks are backed up.
      if (!this._isIdleObserver) {
        this._idleService.addIdleObserver(this, BOOKMARKS_BACKUP_IDLE_TIME);
        this._isIdleObserver = true;
      }

    }.bind(this)).catch(ex => {
      Cu.reportError(ex);
    }).then(result => {
      // NB: deliberately after the catch so that we always do this, even if
      // we threw halfway through initializing in the Task above.
      Services.obs.notifyObservers(null, "places-browser-init-complete", "");
    });
  },

  /**
   * Places shut-down tasks
   * - back up bookmarks if needed.
   * - export bookmarks as HTML, if so configured.
   * - finalize components depending on Places.
   */
  _onPlacesShutdown: function() {
    this._sanitizer.onShutdown();
    PageThumbs.uninit();

    if (this._isIdleObserver) {
      this._idleService.removeIdleObserver(this, BOOKMARKS_BACKUP_IDLE_TIME);
      this._isIdleObserver = false;
    }

    let waitingForBackupToComplete = true;
    this._backupBookmarks().then(
      function onSuccess() {
        waitingForBackupToComplete = false;
      },
      function onFailure() {
        Cu.reportError("Unable to backup bookmarks.");
        waitingForBackupToComplete = false;
      }
    );

    // Backup bookmarks to bookmarks.html to support apps that depend
    // on the legacy format.
    let waitingForHTMLExportToComplete = false;
    // If this fails to get the preference value, we don't export.
    if (Services.prefs.getBoolPref("browser.bookmarks.autoExportHTML")) {
      // Exceptionally, since this is a non-default setting and HTML format is
      // discouraged in favor of the JSON/JSONLZ4 backups, we spin the event
      // loop on shutdown, to wait for the export to finish.  We cannot safely
      // spin the event loop on shutdown until we include a watchdog to prevent
      // potential hangs (bug 518683).  The asynchronous shutdown operations
      // will then be handled by a shutdown service (bug 435058).
      waitingForHTMLExportToComplete = true;
      BookmarkHTMLUtils.exportToFile(BookmarkHTMLUtils.defaultPath).then(
        function onSuccess() {
          waitingForHTMLExportToComplete = false;
        },
        function onFailure() {
          Cu.reportError("Unable to auto export html.");
          waitingForHTMLExportToComplete = false;
        }
      );
    }

    // The events loop should spin at least once because waitingForBackupToComplete
    // is true before checking whether backup should be made.
    let thread = Services.tm.currentThread;
    while (waitingForBackupToComplete || waitingForHTMLExportToComplete) {
      thread.processNextEvent(true);
    }
  },

  /**
   * Backup bookmarks.
   */
  _backupBookmarks: function() {
    return Task.spawn(function() {
      let lastBackupFile = yield PlacesBackups.getMostRecentBackup();
      // Should backup bookmarks if there are no backups or the maximum
      // interval between backups elapsed.
      if (!lastBackupFile ||
          new Date() - PlacesBackups.getDateForFile(lastBackupFile) > BOOKMARKS_BACKUP_INTERVAL) {
        let maxBackups = BOOKMARKS_BACKUP_MAX_BACKUPS;
        try {
          maxBackups = Services.prefs.getIntPref("browser.bookmarks.max_backups");
        } catch(ex) {
          // Use default.
        }

        // Don't force creation.
        yield PlacesBackups.create(maxBackups);
      }
    });
  },

  /**
   * Show the notificationBox for a locked places database.
   */
  _showPlacesLockedNotificationBox: function() {
    var applicationName = gBrandBundle.GetStringFromName("brandShortName");
    var placesBundle = Services.strings.createBundle("chrome://browser/locale/places/places.properties");
    var title = placesBundle.GetStringFromName("lockPrompt.title");
    var text = placesBundle.formatStringFromName("lockPrompt.text", [applicationName], 1);
    var buttonText = placesBundle.GetStringFromName("lockPromptInfoButton.label");
    var accessKey = placesBundle.GetStringFromName("lockPromptInfoButton.accessKey");

    var helpTopic = "places-locked";
    var url = Cc["@mozilla.org/toolkit/URLFormatterService;1"]
                .getService(Components.interfaces.nsIURLFormatter)
                .formatURLPref("app.support.baseURL");
    url += helpTopic;

    var win = this.getMostRecentBrowserWindow();

    var buttons = [
                    {
                      label:     buttonText,
                      accessKey: accessKey,
                      popup:     null,
                      callback:  function(aNotificationBar, aButton) {
                        win.openUILinkIn(url, "tab");
                      }
                    }
                  ];

    var notifyBox = win.gBrowser.getNotificationBox();
    var notification = notifyBox.appendNotification(text, title, null,
                                                    notifyBox.PRIORITY_CRITICAL_MEDIUM,
                                                    buttons);
    notification.persistence = -1; // Until user closes it
  },

  _migrateUI: function() {
    const UI_VERSION = 26;
    const BROWSER_DOCURL = "chrome://browser/content/browser.xul#";
    let currentUIVersion = 0;
    try {
      currentUIVersion = Services.prefs.getIntPref("browser.migration.version");
    } catch(ex) {}
    if (currentUIVersion >= UI_VERSION) {
      return;
    }

    this._rdf = Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService);
    this._dataSource = this._rdf.GetDataSource("rdf:local-store");
    this._dirty = false;

    if (currentUIVersion < 2) {
      // This code adds the customizable bookmarks button.
      let currentsetResource = this._rdf.GetResource("currentset");
      let toolbarResource = this._rdf.GetResource(BROWSER_DOCURL + "nav-bar");
      let currentset = this._getPersist(toolbarResource, currentsetResource);
      // Need to migrate only if toolbar is customized and the element is not found.
      if (currentset &&
          currentset.indexOf("bookmarks-menu-button-container") == -1) {
        currentset += ",bookmarks-menu-button-container";
        this._setPersist(toolbarResource, currentsetResource, currentset);
      }
    }

    if (currentUIVersion < 3) {
      // This code merges the reload/stop/go button into the url bar.
      let currentsetResource = this._rdf.GetResource("currentset");
      let toolbarResource = this._rdf.GetResource(BROWSER_DOCURL + "nav-bar");
      let currentset = this._getPersist(toolbarResource, currentsetResource);
      // Need to migrate only if toolbar is customized and all 3 elements are found.
      if (currentset &&
          currentset.indexOf("reload-button") != -1 &&
          currentset.indexOf("stop-button") != -1 &&
          currentset.indexOf("urlbar-container") != -1 &&
          currentset.indexOf("urlbar-container,reload-button,stop-button") == -1) {
        currentset = currentset.replace(/(^|,)reload-button($|,)/, "$1$2")
                               .replace(/(^|,)stop-button($|,)/, "$1$2")
                               .replace(/(^|,)urlbar-container($|,)/,
                                        "$1urlbar-container,reload-button,stop-button$2");
        this._setPersist(toolbarResource, currentsetResource, currentset);
      }
    }

    if (currentUIVersion < 4) {
      // This code moves the home button to the immediate left of the bookmarks menu button.
      let currentsetResource = this._rdf.GetResource("currentset");
      let toolbarResource = this._rdf.GetResource(BROWSER_DOCURL + "nav-bar");
      let currentset = this._getPersist(toolbarResource, currentsetResource);
      // Need to migrate only if toolbar is customized and the elements are found.
      if (currentset &&
          currentset.indexOf("home-button") != -1 &&
          currentset.indexOf("bookmarks-menu-button-container") != -1) {
        currentset = currentset.replace(/(^|,)home-button($|,)/, "$1$2")
                               .replace(/(^|,)bookmarks-menu-button-container($|,)/,
                                        "$1home-button,bookmarks-menu-button-container$2");
        this._setPersist(toolbarResource, currentsetResource, currentset);
      }
    }

    if (currentUIVersion < 5) {
      // This code uncollapses PersonalToolbar if its collapsed status is not
      // persisted, and user customized it or changed default bookmarks.
      let toolbarResource = this._rdf.GetResource(BROWSER_DOCURL + "PersonalToolbar");
      let collapsedResource = this._rdf.GetResource("collapsed");
      let collapsed = this._getPersist(toolbarResource, collapsedResource);
      // If the user does not have a persisted value for the toolbar's
      // "collapsed" attribute, try to determine whether it's customized.
      if (collapsed === null) {
        // We consider the toolbar customized if it has more than
        // 3 children, or if it has a persisted currentset value.
        let currentsetResource = this._rdf.GetResource("currentset");
        let toolbarIsCustomized = !!this._getPersist(toolbarResource,
                                                     currentsetResource);
        function getToolbarFolderCount() {
          let toolbarFolder =
            PlacesUtils.getFolderContents(PlacesUtils.toolbarFolderId).root;
          let toolbarChildCount = toolbarFolder.childCount;
          toolbarFolder.containerOpen = false;
          return toolbarChildCount;
        }

        if (toolbarIsCustomized || getToolbarFolderCount() > 3) {
          this._setPersist(toolbarResource, collapsedResource, "false");
        }
      }
    }

    if (currentUIVersion < 6) {
      // convert tabsontop attribute to pref
      let toolboxResource = this._rdf.GetResource(BROWSER_DOCURL + "navigator-toolbox");
      let tabsOnTopResource = this._rdf.GetResource("tabsontop");
      let tabsOnTopAttribute = this._getPersist(toolboxResource, tabsOnTopResource);
      if (tabsOnTopAttribute)
        Services.prefs.setBoolPref("browser.tabs.onTop", tabsOnTopAttribute == "true");
    }

    // Migration at version 7 only occurred for users who wanted to try the new
    // Downloads Panel feature before its release. Since migration at version
    // 9 adds the button by default, this step has been removed.

    if (currentUIVersion < 8) {
      // Reset homepage pref for users who have it set to google.com/firefox
      let uri = Services.prefs.getComplexValue("browser.startup.homepage",
                                               Ci.nsIPrefLocalizedString).data;
      if (uri && /^https?:\/\/(www\.)?google(\.\w{2,3}){1,2}\/firefox\/?$/.test(uri)) {
        Services.prefs.clearUserPref("browser.startup.homepage");
      }
    }

    if (currentUIVersion < 9) {
      // This code adds the customizable downloads buttons.
      let currentsetResource = this._rdf.GetResource("currentset");
      let toolbarResource = this._rdf.GetResource(BROWSER_DOCURL + "nav-bar");
      let currentset = this._getPersist(toolbarResource, currentsetResource);

      // Since the Downloads button is located in the navigation bar by default,
      // migration needs to happen only if the toolbar was customized using a
      // previous UI version, and the button was not already placed on the
      // toolbar manually.
      if (currentset &&
          currentset.indexOf("downloads-button") == -1) {
        // The element is added either after the search bar or before the home
        // button. As a last resort, the element is added just before the
        // non-customizable window controls.
        if (currentset.indexOf("search-container") != -1) {
          currentset = currentset.replace(/(^|,)search-container($|,)/,
                                          "$1search-container,downloads-button$2")
        } else if (currentset.indexOf("home-button") != -1) {
          currentset = currentset.replace(/(^|,)home-button($|,)/,
                                          "$1downloads-button,home-button$2")
        } else {
          currentset = currentset.replace(/(^|,)window-controls($|,)/,
                                          "$1downloads-button,window-controls$2")
        }
        this._setPersist(toolbarResource, currentsetResource, currentset);
      }

      Services.prefs.clearUserPref("browser.download.useToolkitUI");
      Services.prefs.clearUserPref("browser.library.useNewDownloadsView");
    }

#ifdef XP_WIN
    if (currentUIVersion < 10) {
      // For Windows systems with display set to > 96dpi (i.e. systemDefaultScale
      // will return a value > 1.0), we want to discard any saved full-zoom settings,
      // as we'll now be scaling the content according to the system resolution
      // scale factor (Windows "logical DPI" setting)
      let sm = Cc["@mozilla.org/gfx/screenmanager;1"].getService(Ci.nsIScreenManager);
      if (sm.systemDefaultScale > 1.0) {
        let cps2 = Cc["@mozilla.org/content-pref/service;1"].
                   getService(Ci.nsIContentPrefService2);
        cps2.removeByName("browser.content.full-zoom", null);
      }
    }
#endif

    if (currentUIVersion < 11) {
      Services.prefs.clearUserPref("dom.disable_window_move_resize");
      Services.prefs.clearUserPref("dom.disable_window_flip");
      Services.prefs.clearUserPref("dom.event.contextmenu.enabled");
      Services.prefs.clearUserPref("javascript.enabled");
      Services.prefs.clearUserPref("permissions.default.image");
    }

    if (currentUIVersion < 12) {
      // Remove bookmarks-menu-button-container, then place
      // bookmarks-menu-button into its position.
      let currentsetResource = this._rdf.GetResource("currentset");
      let toolbarResource = this._rdf.GetResource(BROWSER_DOCURL + "nav-bar");
      let currentset = this._getPersist(toolbarResource, currentsetResource);
      // Need to migrate only if toolbar is customized.
      if (currentset) {
        if (currentset.contains("bookmarks-menu-button-container")) {
          currentset = currentset.replace(/(^|,)bookmarks-menu-button-container($|,)/,
                                          "$1bookmarks-menu-button$2");
          this._setPersist(toolbarResource, currentsetResource, currentset);
        }
      }
    }

    if (currentUIVersion < 16) {
      // Migrate Sync from pmsync.palemoon.net to pmsync.palemoon.org
      try {
        let syncURL = Services.prefs.getCharPref("services.sync.clusterURL");
        let newSyncURL = syncURL.replace(/pmsync\.palemoon\.net/i,"pmsync.palemoon.org");
        if (newSyncURL != syncURL) {
          Services.prefs.setCharPref("services.sync.clusterURL", newSyncURL);
        }
      } catch(ex) {
        // Pref not found: Sync not in use, nothing to do.
      }
    }

    if (currentUIVersion < 17) {
      this._notifyNotificationsUpgrade();
    }

    if (currentUIVersion < 18) {
      // Make sure the doNotTrack value conforms to the conversion from
      // three-state to two-state. (This reverts a setting of "please track me"
      // to the default "don't say anything").
      try {
        if (Services.prefs.getBoolPref("privacy.donottrackheader.enabled") &&
            Services.prefs.getIntPref("privacy.donottrackheader.value") != 1) {
          Services.prefs.clearUserPref("privacy.donottrackheader.enabled");
          Services.prefs.clearUserPref("privacy.donottrackheader.value");
        }
      }
      catch (ex) {}
    }

#ifndef MOZ_JXR
    // Until JPEG-XR decoder is implemented (UXP #144)
    if (currentUIVersion < 19) {
      try {
        let ihaPref = "image.http.accept";
        let ihaValue = Services.prefs.getCharPref(ihaPref);
        if (ihaValue.includes("image/jxr,")) {
          Services.prefs.setCharPref(ihaPref, ihaValue.replace("image/jxr,", ""));
        } else if (ihaValue.includes("image/jxr")) {
          Services.prefs.clearUserPref(ihaPref);
        }
      } catch(ex) {}
    }
#endif

    if (currentUIVersion < 20) {
      // HPKP change of UI preference; reset enforcement level
      Services.prefs.clearUserPref("security.cert_pinning.enforcement_level");
    }

    if (currentUIVersion < 21) {
      // Remove key4.db and cert9.db if those files exist
      // XXX: Remove this code block once we actually start using them.
      let dsCertFile = Cc["@mozilla.org/file/directory_service;1"]
                    .getService(Ci.nsIProperties)
                    .get("ProfD", Ci.nsIFile);
      dsKeyFile = dsCertFile.clone();
      dsCertFile.append("cert9.db");
      if (dsCertFile.exists()) {
        try {
          dsCertFile.remove(false);
        } catch(e) {}
      }
      dsKeyFile.append("key4.db");
      if (dsKeyFile.exists()) {
        try {
          dsKeyFile.remove(false);
        } catch(e) {}
      }
    }

    if (currentUIVersion < 23) {
      if (Services.prefs.prefHasUserValue("layers.acceleration.disabled")) {
        let HWADisabled = Services.prefs.getBoolPref("layers.acceleration.disabled");
        Services.prefs.setBoolPref("layers.acceleration.enabled", !HWADisabled);
        Services.prefs.setBoolPref("gfx.direct2d.disabled", HWADisabled);
      }
      if (Services.prefs.getBoolPref("layers.acceleration.force-enabled", false)) {
        Services.prefs.setBoolPref("layers.acceleration.force", true);
      }
      Services.prefs.clearUserPref("layers.acceleration.disabled");
      Services.prefs.clearUserPref("layers.acceleration.force-enabled");
    }  

    if (currentUIVersion < 24) {
      // AbortController's worker signalling was fixed so reset user prefs that
      // might have been set as workaround for web compat issues in the meantime.
      Services.prefs.clearUserPref("dom.abortController.enabled");
    }

    if (currentUIVersion < 26) {
      // DoNotTrack is now GPC. Carry across user preference.
      if (Services.prefs.prefHasUserValue("privacy.donottrackheader.enabled")) {
        let DNTEnabled = Services.prefs.getBoolPref("privacy.donottrackheader.enabled");
        Services.prefs.setBoolPref("privacy.GPCheader.enabled", DNTEnabled);
        Services.prefs.clearUserPref("privacy.donottrackheader.enabled");
      }
    }
    
    // Clear out dirty storage
    if (this._dirty) {
      this._dataSource.QueryInterface(Ci.nsIRDFRemoteDataSource).Flush();
    }

    delete this._rdf;
    delete this._dataSource;

    // Update the migration version.
    Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  },

  _hasExistingNotificationPermission: function() {
    let enumerator = Services.perms.enumerator;
    while (enumerator.hasMoreElements()) {
      let permission = enumerator.getNext().QueryInterface(Ci.nsIPermission);
      if (permission.type == "desktop-notification") {
        return true;
      }
    }
    return false;
  },

  _notifyNotificationsUpgrade: function() {
    if (!this._hasExistingNotificationPermission()) {
      return;
    }
    function clickCallback(subject, topic, data) {
      if (topic != "alertclickcallback") {
        return;
      }
      let win = RecentWindow.getMostRecentBrowserWindow();
      win.openUILinkIn(data, "tab");
    }
    // Show the application icon for XUL notifications. We assume system-level
    // notifications will include their own icon.
    let imageURL = this._hasSystemAlertsService() ? "" :
                   "chrome://branding/content/about-logo.png";
    let title = gBrowserBundle.GetStringFromName("webNotifications.upgradeTitle");
    let text = gBrowserBundle.GetStringFromName("webNotifications.upgradeBody");
    let url = Services.urlFormatter.formatURLPref("browser.push.warning.infoURL");

    try {
      AlertsService.showAlertNotification(imageURL, title, text,
                                          true, url, clickCallback);
    } catch(e) {
      Cu.reportError(e);
    }
  },

  _openPermissions: function(aPrincipal) {
    var win = this.getMostRecentBrowserWindow();
    var url = "about:permissions";
    try {
      url = url + "?filter=" + aPrincipal.URI.host;
    } catch(e) {}
    win.openUILinkIn(url, "tab");
  },

  _hasSystemAlertsService: function() {
    try {
      return !!Cc["@mozilla.org/system-alerts-service;1"].getService(
        Ci.nsIAlertsService);
    } catch(e) {}
    return false;
  },

  _getPersist: function(aSource, aProperty) {
    var target = this._dataSource.GetTarget(aSource, aProperty, true);
    if (target instanceof Ci.nsIRDFLiteral) {
      return target.Value;
    }
    return null;
  },

  _setPersist: function(aSource, aProperty, aTarget) {
    this._dirty = true;
    try {
      var oldTarget = this._dataSource.GetTarget(aSource, aProperty, true);
      if (oldTarget) {
        if (aTarget) {
          this._dataSource.Change(aSource, aProperty, oldTarget, this._rdf.GetLiteral(aTarget));
        } else {
          this._dataSource.Unassert(aSource, aProperty, oldTarget);
        }
      } else {
        this._dataSource.Assert(aSource, aProperty, this._rdf.GetLiteral(aTarget), true);
      }

      // Add the entry to the persisted set for this document if it's not there.
      // This code is mostly borrowed from XULDocument::Persist.
      let docURL = aSource.ValueUTF8.split("#")[0];
      let docResource = this._rdf.GetResource(docURL);
      let persistResource = this._rdf.GetResource("http://home.netscape.com/NC-rdf#persist");
      if (!this._dataSource.HasAssertion(docResource, persistResource, aSource, true)) {
        this._dataSource.Assert(docResource, persistResource, aSource, true);
      }
    } catch(ex) {}
  },

  // ------------------------------
  // public nsIBrowserGlue members
  // ------------------------------

  sanitize: function(aParentWindow) {
    this._sanitizer.sanitize(aParentWindow);
  },

  ensurePlacesDefaultQueriesInitialized:
  function() {
    // This is actual version of the smart bookmarks, must be increased every
    // time smart bookmarks change.
    // When adding a new smart bookmark below, its newInVersion property must
    // be set to the version it has been added in, we will compare its value
    // to users' smartBookmarksVersion and add new smart bookmarks without
    // recreating old deleted ones.
    const SMART_BOOKMARKS_VERSION = 4;
    const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";
    const SMART_BOOKMARKS_PREF = "browser.places.smartBookmarksVersion";
    const SMART_BOOKMARKS_MAX_PREF = "browser.places.smartBookmarks.max";
    const SMART_BOOKMARKS_OLDMAX_PREF = "browser.places.smartBookmarks.old-max";

    const MAX_RESULTS = Services.prefs.getIntPref(SMART_BOOKMARKS_MAX_PREF, 10);
    let OLD_MAX_RESULTS = Services.prefs.getIntPref(SMART_BOOKMARKS_OLDMAX_PREF, 10);

    // Get current smart bookmarks version.  If not set, create them.
    let smartBookmarksCurrentVersion = Services.prefs.getIntPref(SMART_BOOKMARKS_PREF, 0);

    // If version is current and max hasn't changed or smart bookmarks are disabled, just bail out.
    if (smartBookmarksCurrentVersion == -1 ||
        (smartBookmarksCurrentVersion >= SMART_BOOKMARKS_VERSION &&
         OLD_MAX_RESULTS == MAX_RESULTS)) {
      return;
    }
    
    // We're going to recreate the smart bookmarks and set the current max, so store it.
    if (Services.prefs.prefHasUserValue(SMART_BOOKMARKS_MAX_PREF)) {
      Services.prefs.setIntPref(SMART_BOOKMARKS_OLDMAX_PREF, MAX_RESULTS);
    } else {
      // The max value is default, no need to track the temp value.
      Services.prefs.clearUserPref(SMART_BOOKMARKS_OLDMAX_PREF);
    }

    let batch = {
      runBatched: function() {
        let menuIndex = 0;
        let toolbarIndex = 0;
        let bundle = Services.strings.createBundle("chrome://browser/locale/places/places.properties");

        let smartBookmarks = {
          MostVisited: {
            title: bundle.GetStringFromName("mostVisitedTitle"),
            uri: NetUtil.newURI("place:sort=" +
                                Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING +
                                "&maxResults=" + MAX_RESULTS),
            parent: PlacesUtils.toolbarFolderId,
            position: toolbarIndex++,
            newInVersion: 1
          },
          RecentlyBookmarked: {
            title: bundle.GetStringFromName("recentlyBookmarkedTitle"),
            uri: NetUtil.newURI("place:folder=BOOKMARKS_MENU" +
                                "&folder=UNFILED_BOOKMARKS" +
                                "&folder=TOOLBAR" +
                                "&queryType=" +
                                Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS +
                                "&sort=" +
                                Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING +
                                "&maxResults=" + MAX_RESULTS +
                                "&excludeQueries=1"),
            parent: PlacesUtils.bookmarksMenuFolderId,
            position: menuIndex++,
            newInVersion: 1
          },
          RecentTags: {
            title: bundle.GetStringFromName("recentTagsTitle"),
            uri: NetUtil.newURI("place:"+
                                "type=" +
                                Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY +
                                "&sort=" +
                                Ci.nsINavHistoryQueryOptions.SORT_BY_LASTMODIFIED_DESCENDING +
                                "&maxResults=" + MAX_RESULTS),
            parent: PlacesUtils.bookmarksMenuFolderId,
            position: menuIndex++,
            newInVersion: 1
          }
        };

        // Set current itemId, parent and position if Smart Bookmark exists,
        // we will use these informations to create the new version at the same
        // position.
        let smartBookmarkItemIds = PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
        smartBookmarkItemIds.forEach(function(itemId) {
          let queryId = PlacesUtils.annotations.getItemAnnotation(itemId, SMART_BOOKMARKS_ANNO);
          if (queryId in smartBookmarks) {
            let smartBookmark = smartBookmarks[queryId];
            smartBookmark.itemId = itemId;
            smartBookmark.parent = PlacesUtils.bookmarks.getFolderIdForItem(itemId);
            smartBookmark.position = PlacesUtils.bookmarks.getItemIndex(itemId);
          } else {
            // We don't remove old Smart Bookmarks because the user could still
            // find them useful, or could have personalized them.
            // Instead we remove the Smart Bookmark annotation.
            PlacesUtils.annotations.removeItemAnnotation(itemId, SMART_BOOKMARKS_ANNO);
          }
        });

        for (let queryId in smartBookmarks) {
          let smartBookmark = smartBookmarks[queryId];

          // We update or create only changed or new smart bookmarks.
          // Also we respect user choices, so we won't try to create a smart
          // bookmark if it has been removed.
          if (smartBookmarksCurrentVersion > 0 &&
              smartBookmark.newInVersion <= smartBookmarksCurrentVersion &&
              !smartBookmark.itemId) {
            continue;
          }

          // Remove old version of the smart bookmark if it exists, since it
          // will be replaced in place.
          if (smartBookmark.itemId) {
            PlacesUtils.bookmarks.removeItem(smartBookmark.itemId);
          }

          // Create the new smart bookmark and store its updated itemId.
          smartBookmark.itemId =
            PlacesUtils.bookmarks.insertBookmark(smartBookmark.parent,
                                                 smartBookmark.uri,
                                                 smartBookmark.position,
                                                 smartBookmark.title);
          PlacesUtils.annotations.setItemAnnotation(smartBookmark.itemId,
                                                    SMART_BOOKMARKS_ANNO,
                                                    queryId, 0,
                                                    PlacesUtils.annotations.EXPIRE_NEVER);
        }

        // If we are creating all Smart Bookmarks from ground up, add a
        // separator below them in the bookmarks menu.
        if (smartBookmarksCurrentVersion == 0 &&
            smartBookmarkItemIds.length == 0) {
          let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId,
                                                        menuIndex);
          // Don't add a separator if the menu was empty or there is one already.
          if (id != -1 &&
              PlacesUtils.bookmarks.getItemType(id) != PlacesUtils.bookmarks.TYPE_SEPARATOR) {
            PlacesUtils.bookmarks.insertSeparator(PlacesUtils.bookmarksMenuFolderId,
                                                  menuIndex);
          }
        }
      }
    };

    try {
      PlacesUtils.bookmarks.runInBatchMode(batch, null);
    } catch(ex) {
      Components.utils.reportError(ex);
    } finally {
      Services.prefs.setIntPref(SMART_BOOKMARKS_PREF, SMART_BOOKMARKS_VERSION);
      Services.prefs.savePrefFile(null);
    }
  },

  // this returns the most recent non-popup browser window
  getMostRecentBrowserWindow: function() {
    return RecentWindow.getMostRecentBrowserWindow();
  },

#ifdef MOZ_SERVICES_SYNC
  /**
   * Called as an observer when Sync's "display URI" notification is fired.
   *
   * We open the received URI in a background tab.
   *
   * Eventually, this will likely be replaced by a more robust tab syncing
   * feature. This functionality is considered somewhat evil by UX because it
   * opens a new tab automatically without any prompting. However, it is a
   * lesser evil than sending a tab to a specific device (from e.g. Fennec)
   * and having nothing happen on the receiving end.
   */
  _onDisplaySyncURI: function(data) {
    try {
      let tabbrowser = RecentWindow.getMostRecentBrowserWindow({private: false}).gBrowser;

      // The payload is wrapped weirdly because of how Sync does notifications.
      tabbrowser.addTab(data.wrappedJSObject.object.uri);
    } catch(ex) {
      Cu.reportError("Error displaying tab received by Sync: " + ex);
    }
  },
#endif

  // for XPCOM
  classID:          Components.ID("{eab9012e-5f74-4cbc-b2b5-a590235513cc}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIBrowserGlue]),

  // redefine the default factory for XPCOMUtils
  _xpcom_factory: BrowserGlueServiceFactory,
}

function ContentPermissionPrompt() {}
ContentPermissionPrompt.prototype = {
  classID:          Components.ID("{d8903bf6-68d5-4e97-bcd1-e4d3012f721a}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  _getChromeWindow: function(aWindow) {
    var chromeWin = aWindow
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShellTreeItem)
      .rootTreeItem
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindow)
      .QueryInterface(Ci.nsIDOMChromeWindow);
    return chromeWin;
  },

  _getBrowserForRequest: function(aRequest) {
    let requestingWindow = aRequest.window.top;
    // find the requesting browser or iframe
    let browser = requestingWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIWebNavigation)
                                  .QueryInterface(Ci.nsIDocShell)
                                  .chromeEventHandler;
    return browser;
  },

  /**
   * Show a permission prompt.
   *
   * @param aRequest               The permission request.
   * @param aMessage               The message to display on the prompt.
   * @param aPermission            The type of permission to prompt.
   * @param aActions               An array of actions of the form:
   *                               [main action, secondary actions, ...]
   *                               Actions are of the form { stringId, action, expireType, callback }
   *                               Permission is granted if action is null or ALLOW_ACTION.
   * @param aNotificationId        The id of the PopupNotification.
   * @param aAnchorId              The id for the PopupNotification anchor.
   * @param aOptions               Options for the PopupNotification
   */
  _showPrompt: function(aRequest, aMessage, aPermission, aActions,
                        aNotificationId, aAnchorId, aOptions) {
    function onFullScreen() {
      popup.remove();
    }

    var requestingWindow = aRequest.window.top;
    var chromeWin = this._getChromeWindow(requestingWindow).wrappedJSObject;
    var browser = chromeWin.gBrowser.getBrowserForDocument(requestingWindow.document);
    if (!browser) {
      // find the requesting browser or iframe
      browser = requestingWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIWebNavigation)
                                  .QueryInterface(Ci.nsIDocShell)
                                  .chromeEventHandler;
    }
    var requestPrincipal = aRequest.principal;

    // Transform the prompt actions into PopupNotification actions.
    var popupNotificationActions = [];
    for (var i = 0; i < aActions.length; i++) {
      let promptAction = aActions[i];

      // Don't offer action in PB mode if the action remembers permission for more than a session.
      if (PrivateBrowsingUtils.isWindowPrivate(chromeWin) &&
          promptAction.expireType != Ci.nsIPermissionManager.EXPIRE_SESSION &&
          promptAction.action) {
        continue;
      }

      var action = {
        label: gBrowserBundle.GetStringFromName(promptAction.stringId),
        accessKey: gBrowserBundle.GetStringFromName(promptAction.stringId + ".accesskey"),
        callback: function() {
          if (promptAction.callback) {
            promptAction.callback();
          }

          // Remember permissions.
          if (promptAction.action) {
            Services.perms.addFromPrincipal(requestPrincipal, aPermission,
                                            promptAction.action, promptAction.expireType);
          }

          // Grant permission if action is null or ALLOW_ACTION.
          if (!promptAction.action || promptAction.action == Ci.nsIPermissionManager.ALLOW_ACTION) {
            aRequest.allow();
          } else {
            aRequest.cancel();
          }
        },
      };

      popupNotificationActions.push(action);
    }

    var mainAction = popupNotificationActions.length ?
                       popupNotificationActions[0] : null;
    var secondaryActions = popupNotificationActions.splice(1);

    if (aRequest.type == "pointerLock") {
      // If there's no mainAction, this is the autoAllow warning prompt.
      let autoAllow = !mainAction;

      if (!aOptions) {
        aOptions = {};
      }

      aOptions.removeOnDismissal = autoAllow;
      aOptions.eventCallback = type => {
        if (type == "removed") {
          browser.removeEventListener("mozfullscreenchange", onFullScreen, true);
          if (autoAllow) {
            aRequest.allow();
          }
        }
      }

    }

    var popup = chromeWin.PopupNotifications.show(browser, aNotificationId, aMessage, aAnchorId,
                                                  mainAction, secondaryActions, aOptions);
    if (aRequest.type == "pointerLock") {
      // pointerLock is automatically allowed in fullscreen mode (and revoked
      // upon exit), so if the page enters fullscreen mode after requesting
      // pointerLock (but before the user has granted permission), we should
      // remove the now-impotent notification.
      browser.addEventListener("mozfullscreenchange", onFullScreen, true);
    }
  },

  _promptGeo : function(aRequest) {
    var requestingURI = aRequest.principal.URI;

    var message;

    // Share location action.
    var actions = [{ stringId: "geolocation.shareLocation",
                     action: null,
                     expireType: null,
                     callback: function() {
                       // Telemetry stub (left here for safety and compatibility reasons)
                     }
                  }];

    if (requestingURI.schemeIs("file")) {
      message = gBrowserBundle.formatStringFromName("geolocation.shareWithFile",
                                                    [requestingURI.path], 1);
    } else {
      message = gBrowserBundle.formatStringFromName("geolocation.shareWithSite",
                                                    [requestingURI.host], 1);
      // Always share location action.
      actions.push({
        stringId: "geolocation.alwaysShareLocation",
        action: Ci.nsIPermissionManager.ALLOW_ACTION,
        expireType: null,
        callback: function() {
          // Telemetry stub (left here for safety and compatibility reasons)
        }
      });

      // Never share location action.
      actions.push({
        stringId: "geolocation.neverShareLocation",
        action: Ci.nsIPermissionManager.DENY_ACTION,
        expireType: null,
        callback: function() {
          // Telemetry stub (left here for safety and compatibility reasons)
        }
      });
    }

    var options = { learnMoreURL: Services.urlFormatter.formatURLPref("browser.geolocation.warning.infoURL") };

    this._showPrompt(aRequest, message, "geo", actions, "geolocation",
                     "geo-notification-icon", options);
  },

  _promptWebNotifications : function(aRequest) {
    var requestingURI = aRequest.principal.URI;

    var message = gBrowserBundle.formatStringFromName("webNotifications.showFromSite",
                                                      [requestingURI.host], 1);

    var actions;

    var browser = this._getBrowserForRequest(aRequest);
    // Only show "allow for session" in PB mode, we don't
    // support "allow for session" in non-PB mode.
    if (PrivateBrowsingUtils.isBrowserPrivate(browser)) {
      actions = [
        {
          stringId: "webNotifications.showForSession",
          action: Ci.nsIPermissionManager.ALLOW_ACTION,
          expireType: Ci.nsIPermissionManager.EXPIRE_SESSION,
          callback: function() {},
        }
      ];
    } else {
      actions = [
        {
          stringId: "webNotifications.showForSession",
          action: Ci.nsIPermissionManager.ALLOW_ACTION,
          expireType: Ci.nsIPermissionManager.EXPIRE_SESSION,
          callback: function() {},
        },
        {
          stringId: "webNotifications.alwaysShow",
          action: Ci.nsIPermissionManager.ALLOW_ACTION,
          expireType: null,
          callback: function() {},
        },
        {
          stringId: "webNotifications.neverShow",
          action: Ci.nsIPermissionManager.DENY_ACTION,
          expireType: null,
          callback: function() {},
        }
      ];
    }
    var options = {
      learnMoreURL: Services.urlFormatter.formatURLPref("browser.push.warning.infoURL"),
    };

    this._showPrompt(aRequest, message, "desktop-notification", actions,
                     "web-notifications",
                     "web-notifications-notification-icon", options);
  },

  _promptPointerLock: function(aRequest, autoAllow) {
    let requestingURI = aRequest.principal.URI;

    let originString = requestingURI.schemeIs("file") ? requestingURI.path : requestingURI.host;
    let message = gBrowserBundle.formatStringFromName(autoAllow ?
                                 "pointerLock.autoLock.title2" : "pointerLock.title2",
                                 [originString], 1);
    // If this is an autoAllow info prompt, offer no actions.
    // _showPrompt() will allow the request when it's dismissed.
    let actions = [];
    if (!autoAllow) {
      actions = [
        {
          stringId: "pointerLock.allow2",
          action: null,
          expireType: null,
          callback: function() {},
        },
        {
          stringId: "pointerLock.alwaysAllow",
          action: Ci.nsIPermissionManager.ALLOW_ACTION,
          expireType: null,
          callback: function() {},
        },
        {
          stringId: "pointerLock.neverAllow",
          action: Ci.nsIPermissionManager.DENY_ACTION,
          expireType: null,
          callback: function() {},
        }
      ];
    }

    this._showPrompt(aRequest, message, "pointerLock", actions, "pointerLock",
                     "pointerLock-notification-icon", null);
  },

  prompt: function(request) {
    // Only allow exactly one permission rquest here.
    let types = request.types.QueryInterface(Ci.nsIArray);
    if (types.length != 1) {
      request.cancel();
      return;
    }
    let perm = types.queryElementAt(0, Ci.nsIContentPermissionType);
    
    const kFeatureKeys = { "geolocation" : "geo",
                           "desktop-notification" : "desktop-notification",
                           "pointerLock" : "pointerLock",
                         };

    // Make sure that we support the request.
    if (!(perm.type in kFeatureKeys)) {
        return;
    }

    var requestingPrincipal = request.principal;
    var requestingURI = requestingPrincipal.URI;

    // Ignore requests from non-nsIStandardURLs
    if (!(requestingURI instanceof Ci.nsIStandardURL))
      return;

    var autoAllow = false;
    var permissionKey = kFeatureKeys[perm.type];
    var result = Services.perms.testExactPermissionFromPrincipal(requestingPrincipal, permissionKey);

    if (result == Ci.nsIPermissionManager.DENY_ACTION) {
      request.cancel();
      return;
    }

    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      autoAllow = true;
      // For pointerLock, we still want to show a warning prompt.
      if (request.type != "pointerLock") {
        request.allow();
        return;
      }
    }

    // Show the prompt.
    switch (perm.type) {
      case "geolocation":
        this._promptGeo(request);
        break;
      case "desktop-notification":
        this._promptWebNotifications(request);
        break;
      case "pointerLock":
        this._promptPointerLock(request, autoAllow);
        break;
    }
  }
}; // ContentPermissionPrompt

var components = [BrowserGlue, ContentPermissionPrompt];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
