/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution
#filter emptyLines

// Set defines to construct URLs
#define BRANDING_BASEURL basilisk-browser.org
#define BRANDING_SITEURL www.@BRANDING_BASEURL@
#define BRANDING_RELNOTESPATH releasenotes.shtml
#define BRANDING_FIRSTRUNPATH firstrun/
#define BRANDING_APPUPDATEURL aus.@BRANDING_BASEURL@
#define BRANDING_APPUPDATEPATH ?application=%PRODUCT%&version=%VERSION%&arch=%BUILD_TARGET%&buildid=%BUILD_ID%&channel=%CHANNEL%

// Shared Branding Preferences
// XXX: These should REALLY go back to application preferences
#include ../../shared/preferences.inc

// Branding Specific Preferences
pref("startup.homepage_override_url", "https://@BRANDING_SITEURL@/@BRANDING_RELNOTESPATH@");
pref("startup.homepage_welcome_url", "http://@BRANDING_SITEURL@/@BRANDING_FIRSTRUNPATH@");
pref("startup.homepage_welcome_url.additional", "");

// Version release notes
pref("app.releaseNotesURL", "http://@BRANDING_SITEURL@/@BRANDING_RELNOTESPATH@");

// Vendor home page
pref("app.vendorURL", "http://@BRANDING_SITEURL@/");

pref("app.update.url", "https://@BRANDING_APPUPDATEURL@/@BRANDING_APPUPDATEPATH@");

// URL user can browse to manually if for some reason all update installation
// attempts fail.
pref("app.update.url.manual", "https://@BRANDING_SITEURL@/");
// A default value for the "More information about this update" link
// supplied in the "An update is available" page of the update wizard.
pref("app.update.url.details", "https://@BRANDING_SITEURL@/@BRANDING_RELNOTESPATH@");

// Switch Application Updates off for now
pref("app.update.enabled", false);

// Shared User Agent Overrides
#include ../../shared/uaoverrides.inc
