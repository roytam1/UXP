/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("startup.homepage_override_url", "https://www.basilisk-browser.org/releasenotes.shtml");
pref("startup.homepage_welcome_url", "http://www.basilisk-browser.org/firstrun/");
pref("startup.homepage_welcome_url.additional", "");

pref("app.update.url", "https://aus.basilisk-browser.org/?application=%PRODUCT%&version=%VERSION%&arch=%BUILD_TARGET%&buildid=%BUILD_ID%&channel=%CHANNEL%");
// Interval: Time between checks for a new version (in seconds)
pref("app.update.interval", 86400); // 1 day
// The time interval between the downloading of mar file chunks in the
// background (in seconds)
// 0 means "download everything at once"
pref("app.update.download.backgroundInterval", 0);
// Give the user x seconds to react before showing the big UI. default=192 hours
pref("app.update.promptWaitTime", 691200);
// URL user can browse to manually if for some reason all update installation
// attempts fail.
pref("app.update.url.manual", "https://www.basilisk-browser.org/");
// A default value for the "More information about this update" link
// supplied in the "An update is available" page of the update wizard.
pref("app.update.url.details", "https://www.basilisk-browser.org/releasenotes.shtml");

// Switch Application Updates off for now
pref("app.update.enabled", false);

// Version release notes
pref("app.releaseNotesURL", "http://www.basilisk-browser.org/releasenotes.shtml");
// Vendor home page
pref("app.vendorURL", "http://www.basilisk-browser.org/");

// The number of days a binary is permitted to be old
// without checking for an update.  This assumes that
// app.update.checkInstallTime is true.
pref("app.update.checkInstallTime.days", 14);

// Give the user x seconds to reboot before showing a badge on the hamburger
// button. default=immediately
pref("app.update.badgeWaitTime", 0);

// Number of usages of the web console or scratchpad.
// If this is less than 5, then pasting code into the web console or scratchpad is disabled
pref("devtools.selfxss.count", 100);

// Disable Google Safebrowsing by default. Without an API key, this won't work.
pref("browser.safebrowsing.phishing.enabled", false);
pref("browser.safebrowsing.malware.enabled", false);
pref("browser.safebrowsing.downloads.enabled", false);
pref("browser.safebrowsing.downloads.remote.enabled", false);
// Disable the UI controls for it as well for Basilisk-official.
pref("browser.safebrowsing.UI.enabled", false);

// FxA override
pref("general.useragent.override.accounts.firefox.com", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:55.0) Gecko/20100101 Goanna/4.0 Basilisk/55.0.0");

