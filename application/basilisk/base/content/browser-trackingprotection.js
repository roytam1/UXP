/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var TrackingProtection = {
  // If the user ignores the doorhanger, we stop showing it after some time.
  PREF_ENABLED_GLOBALLY: "privacy.trackingprotection.enabled",
  PREF_ENABLED_IN_PRIVATE_WINDOWS: "privacy.trackingprotection.pbmode.enabled",
  enabledGlobally: false,
  enabledInPrivateWindows: false,
  container: null,
  content: null,
  icon: null,
  activeTooltipText: null,
  disabledTooltipText: null,

  init() {
    let $ = selector => document.querySelector(selector);
    this.container = $("#tracking-protection-container");
    this.content = $("#tracking-protection-content");
    this.icon = $("#tracking-protection-icon");

    this.updateEnabled();
    Services.prefs.addObserver(this.PREF_ENABLED_GLOBALLY, this, false);
    Services.prefs.addObserver(this.PREF_ENABLED_IN_PRIVATE_WINDOWS, this, false);

    this.activeTooltipText =
      gNavigatorBundle.getString("trackingProtection.icon.activeTooltip");
    this.disabledTooltipText =
      gNavigatorBundle.getString("trackingProtection.icon.disabledTooltip");
  },

  uninit() {
    Services.prefs.removeObserver(this.PREF_ENABLED_GLOBALLY, this);
    Services.prefs.removeObserver(this.PREF_ENABLED_IN_PRIVATE_WINDOWS, this);
  },

  observe() {
    this.updateEnabled();
  },

  get enabled() {
    return this.enabledGlobally ||
           (this.enabledInPrivateWindows &&
            PrivateBrowsingUtils.isWindowPrivate(window));
  },

  updateEnabled() {
    this.enabledGlobally =
      Services.prefs.getBoolPref(this.PREF_ENABLED_GLOBALLY);
    this.enabledInPrivateWindows =
      Services.prefs.getBoolPref(this.PREF_ENABLED_IN_PRIVATE_WINDOWS);
    this.container.hidden = !this.enabled;
  },

  onSecurityChange(state, isSimulated) {
    if (!this.enabled) {
      return;
    }

    // Only animate the shield if the event was not fired directly from
    // the tabbrowser (due to a browser change).
    if (isSimulated) {
      this.icon.removeAttribute("animate");
    } else {
      this.icon.setAttribute("animate", "true");
    }

    let isBlocking = state & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT;
    let isAllowing = state & Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT;

    if (isBlocking) {
      this.icon.setAttribute("tooltiptext", this.activeTooltipText);
      this.icon.setAttribute("state", "blocked-tracking-content");
      this.content.setAttribute("state", "blocked-tracking-content");
    } else if (isAllowing) {
      this.icon.setAttribute("tooltiptext", this.disabledTooltipText);
      this.icon.setAttribute("state", "loaded-tracking-content");
      this.content.setAttribute("state", "loaded-tracking-content");
    } else {
      this.icon.removeAttribute("tooltiptext");
      this.icon.removeAttribute("state");
      this.content.removeAttribute("state");
    }
  },

  disableForCurrentPage() {
    // Convert document URI into the format used by
    // nsChannelClassifier::ShouldEnableTrackingProtection.
    // Any scheme turned into https is correct.
    let normalizedUrl = Services.io.newURI(
      "https://" + gBrowser.selectedBrowser.currentURI.hostPort,
      null, null);

    // Add the current host in the 'trackingprotection' consumer of
    // the permission manager using a normalized URI. This effectively
    // places this host on the tracking protection allowlist.
    if (PrivateBrowsingUtils.isBrowserPrivate(gBrowser.selectedBrowser)) {
      PrivateBrowsingUtils.addToTrackingAllowlist(normalizedUrl);
    } else {
      Services.perms.add(normalizedUrl,
        "trackingprotection", Services.perms.ALLOW_ACTION);
    }

    // Hide the control center.
    document.getElementById("identity-popup").hidePopup();

    BrowserReload();
  },

  enableForCurrentPage() {
    // Remove the current host from the 'trackingprotection' consumer
    // of the permission manager. This effectively removes this host
    // from the tracking protection allowlist.
    let normalizedUrl = Services.io.newURI(
      "https://" + gBrowser.selectedBrowser.currentURI.hostPort,
      null, null);

    if (PrivateBrowsingUtils.isBrowserPrivate(gBrowser.selectedBrowser)) {
      PrivateBrowsingUtils.removeFromTrackingAllowlist(normalizedUrl);
    } else {
      Services.perms.remove(normalizedUrl, "trackingprotection");
    }

    // Hide the control center.
    document.getElementById("identity-popup").hidePopup();

    BrowserReload();
  },
};
