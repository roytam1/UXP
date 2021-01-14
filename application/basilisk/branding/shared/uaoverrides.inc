#define GUAO_PREF general.useragent.override

#define GRE_VERSION @MOZILLA_UAVERSION_U@
#define GRE_VERSION_SLICE Goanna/@GRE_VERSION@
#define GRE_DATE_SLICE Goanna/20170101
#define APP_SLICE Basilisk/@MOZ_APP_VERSION@

#define GK_VERSION 52.9
#define GK_SLICE Gecko/20100101
#define FX_SLICE Firefox/@GK_VERSION@

// %OS_SLICE% macro is resolved at runtime, see MoonchildProductions/UXP#1473

// Mozilla overrides
pref("@GUAO_PREF@.accounts.firefox.com", "Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@");
pref("@GUAO_PREF@.addons.mozilla.org", "Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@");

// Required for domains that have proven unresponsive to requests from users
pref("@GUAO_PREF@.youtube.com","Mozilla/5.0 (%OS_SLICE% rv:62.0) @GK_SLICE@ Firefox/62.0 @APP_SLICE@");
pref("@GUAO_PREF@.gaming.youtube.com","Mozilla/5.0 (%OS_SLICE% rv:62.0) @GK_SLICE@ Firefox/62.0");
pref("@GUAO_PREF@.studio.youtube.com","Mozilla/5.0 (%OS_SLICE% rv:71.0) @GK_SLICE@ Firefox/71.0 @APP_SLICE@");
pref("@GUAO_PREF@.netflix.com","Mozilla/5.0 (%OS_SLICE% rv:42.0) @GK_SLICE@ Firefox/42.0 @APP_SLICE@");

// The never-ending Facebook debacle...

// UA-Sniffing domains below are pending responses from their operators - temp workaround
// Daily motion only likes strict Firefox UAs
pref("@GUAO_PREF@.dailymotion.com","Mozilla/5.0 (%OS_SLICE% rv:52.0) @GK_SLICE@ Firefox/52.0");

// The following requires native mode. Or it blocks.. "too old firefox", breakage, etc.

// UA-Sniffing domains below have indicated no interest in supporting Pale Moon (BOO!)
pref("@GUAO_PREF@.whatsapp.com","Mozilla/5.0 (%OS_SLICE% rv:68.0) @GK_SLICE@ Firefox/68.0");

// UA-sniffing domains that are "app/vendor-specific" and do not like Pale Moon

// The following domains do not like the Goanna slice
pref("@GUAO_PREF@.hitbox.tv","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@");
pref("@GUAO_PREF@.yuku.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@ @APP_SLICE@");
