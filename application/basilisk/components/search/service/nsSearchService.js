/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/debug.js");
Cu.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
  "resource://gre/modules/Deprecated.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SearchStaticData",
  "resource://gre/modules/SearchStaticData.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "clearTimeout",
  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Lz4",
  "resource://gre/modules/lz4.js");

XPCOMUtils.defineLazyServiceGetter(this, "gTextToSubURI",
                                   "@mozilla.org/intl/texttosuburi;1",
                                   "nsITextToSubURI");
XPCOMUtils.defineLazyServiceGetter(this, "gEnvironment",
                                   "@mozilla.org/process/environment;1",
                                   "nsIEnvironment");

Cu.importGlobalProperties(["XMLHttpRequest"]);

// A text encoder to UTF8, used whenever we commit the cache to disk.
XPCOMUtils.defineLazyGetter(this, "gEncoder",
                            function() {
                              return new TextEncoder();
                            });

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

// Directory service keys
const NS_APP_SEARCH_DIR_LIST  = "SrchPluginsDL";
const NS_APP_DISTRIBUTION_SEARCH_DIR_LIST = "SrchPluginsDistDL";
const NS_APP_USER_SEARCH_DIR  = "UsrSrchPlugns";
const NS_APP_SEARCH_DIR       = "SrchPlugns";
const NS_APP_USER_PROFILE_50_DIR = "ProfD";

// Loading plugins from NS_APP_SEARCH_DIR is no longer supported.
// Instead, we now load plugins from APP_SEARCH_PREFIX, where a
// list.txt file needs to exist to list available engines.
const APP_SEARCH_PREFIX = "resource://search-plugins/";

// See documentation in nsIBrowserSearchService.idl.
const SEARCH_ENGINE_TOPIC        = "browser-search-engine-modified";
const QUIT_APPLICATION_TOPIC     = "quit-application";

const SEARCH_ENGINE_REMOVED      = "engine-removed";
const SEARCH_ENGINE_ADDED        = "engine-added";
const SEARCH_ENGINE_CHANGED      = "engine-changed";
const SEARCH_ENGINE_LOADED       = "engine-loaded";
const SEARCH_ENGINE_CURRENT      = "engine-current";
const SEARCH_ENGINE_DEFAULT      = "engine-default";

// The following constants are left undocumented in nsIBrowserSearchService.idl
// For the moment, they are meant for testing/debugging purposes only.

/**
 * Topic used for events involving the service itself.
 */
const SEARCH_SERVICE_TOPIC       = "browser-search-service";

/**
 * Sent whenever the cache is fully written to disk.
 */
const SEARCH_SERVICE_CACHE_WRITTEN  = "write-cache-to-disk-complete";

// Delay for lazy serialization (ms)
const LAZY_SERIALIZE_DELAY = 100;

// Delay for batching invalidation of the JSON cache (ms)
const CACHE_INVALIDATION_DELAY = 1000;

// Current cache version. This should be incremented if the format of the cache
// file is modified.
const CACHE_VERSION = 1;

const CACHE_FILENAME = "search.json.mozlz4";

const NEW_LINES = /(\r\n|\r|\n)/;

// Set an arbitrary cap on the maximum icon size. Without this, large icons can
// cause big delays when loading them at startup.
const MAX_ICON_SIZE   = 32768;

// Default charset to use for sending search parameters. ISO-8859-1 is used to
// match previous nsInternetSearchService behavior.
const DEFAULT_QUERY_CHARSET = "ISO-8859-1";

const SEARCH_BUNDLE = "chrome://global/locale/search/search.properties";
const BRAND_BUNDLE = "chrome://branding/locale/brand.properties";

const OPENSEARCH_NS_10  = "http://a9.com/-/spec/opensearch/1.0/";
const OPENSEARCH_NS_11  = "http://a9.com/-/spec/opensearch/1.1/";

// Although the specification at http://opensearch.a9.com/spec/1.1/description/
// gives the namespace names defined above, many existing OpenSearch engines
// are using the following versions.  We therefore allow either.
const OPENSEARCH_NAMESPACES = [
  OPENSEARCH_NS_11, OPENSEARCH_NS_10,
  "http://a9.com/-/spec/opensearchdescription/1.1/",
  "http://a9.com/-/spec/opensearchdescription/1.0/"
];

const OPENSEARCH_LOCALNAME = "OpenSearchDescription";

const MOZSEARCH_NS_10     = "http://www.mozilla.org/2006/browser/search/";
const MOZSEARCH_LOCALNAME = "SearchPlugin";

const URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";
const URLTYPE_SEARCH_HTML  = "text/html";
const URLTYPE_OPENSEARCH   = "application/opensearchdescription+xml";

const BROWSER_SEARCH_PREF = "browser.search.";
const LOCALE_PREF = "general.useragent.locale";

const USER_DEFINED = "{searchTerms}";

// Custom search parameters
const MOZ_OFFICIAL = AppConstants.MOZ_OFFICIAL_BRANDING ? "official" : "unofficial";

const MOZ_PARAM_LOCALE         = /\{moz:locale\}/g;
const MOZ_PARAM_DIST_ID        = /\{moz:distributionID\}/g;
const MOZ_PARAM_OFFICIAL       = /\{moz:official\}/g;

// Supported OpenSearch parameters
// See http://opensearch.a9.com/spec/1.1/querysyntax/#core
const OS_PARAM_USER_DEFINED    = /\{searchTerms\??\}/g;
const OS_PARAM_INPUT_ENCODING  = /\{inputEncoding\??\}/g;
const OS_PARAM_LANGUAGE        = /\{language\??\}/g;
const OS_PARAM_OUTPUT_ENCODING = /\{outputEncoding\??\}/g;

// Default values
const OS_PARAM_LANGUAGE_DEF         = "*";
const OS_PARAM_OUTPUT_ENCODING_DEF  = "UTF-8";
const OS_PARAM_INPUT_ENCODING_DEF   = "UTF-8";

// "Unsupported" OpenSearch parameters. For example, we don't support
// page-based results, so if the engine requires that we send the "page index"
// parameter, we'll always send "1".
const OS_PARAM_COUNT        = /\{count\??\}/g;
const OS_PARAM_START_INDEX  = /\{startIndex\??\}/g;
const OS_PARAM_START_PAGE   = /\{startPage\??\}/g;

// Default values
const OS_PARAM_COUNT_DEF        = "20"; // 20 results
const OS_PARAM_START_INDEX_DEF  = "1";  // start at 1st result
const OS_PARAM_START_PAGE_DEF   = "1";  // 1st page

// Optional parameter
const OS_PARAM_OPTIONAL     = /\{(?:\w+:)?\w+\?\}/g;

// A array of arrays containing parameters that we don't fully support, and
// their default values. We will only send values for these parameters if
// required, since our values are just really arbitrary "guesses" that should
// give us the output we want.
var OS_UNSUPPORTED_PARAMS = [
  [OS_PARAM_COUNT, OS_PARAM_COUNT_DEF],
  [OS_PARAM_START_INDEX, OS_PARAM_START_INDEX_DEF],
  [OS_PARAM_START_PAGE, OS_PARAM_START_PAGE_DEF],
];

// The default engine update interval, in days. This is only used if an engine
// specifies an updateURL, but not an updateInterval.
const SEARCH_DEFAULT_UPDATE_INTERVAL = 7;

// The default interval before checking again for the name of the
// default engine for the region, in seconds. Only used if the response
// from the server doesn't specify an interval.
const SEARCH_GEO_DEFAULT_UPDATE_INTERVAL = 2592000; // 30 days.

this.__defineGetter__("FileUtils", function() {
  delete this.FileUtils;
  Components.utils.import("resource://gre/modules/FileUtils.jsm");
  return FileUtils;
});

this.__defineGetter__("NetUtil", function() {
  delete this.NetUtil;
  Components.utils.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

this.__defineGetter__("gChromeReg", function() {
  delete this.gChromeReg;
  return this.gChromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                           getService(Ci.nsIChromeRegistry);
});

/**
 * Prefixed to all search debug output.
 */
const SEARCH_LOG_PREFIX = "*** Search: ";

/**
 * Outputs aText to the JavaScript console as well as to stdout.
 */
function DO_LOG(aText) {
  dump(SEARCH_LOG_PREFIX + aText + "\n");
  Services.console.logStringMessage(aText);
}

/**
 * In debug builds, use a live, pref-based (browser.search.log) LOG function
 * to allow enabling/disabling without a restart. Otherwise, don't log at all by
 * default. This can be overridden at startup by the pref, see SearchService's
 * _init method.
 */
var LOG = function() {};

if (AppConstants.DEBUG) {
  LOG = function (aText) {
    if (Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "log", false)) {
      DO_LOG(aText);
    }
  };
}

/**
 * Presents an assertion dialog in non-release builds and throws.
 * @param  message
 *         A message to display
 * @param  resultCode
 *         The NS_ERROR_* value to throw.
 * @throws resultCode
 */
function ERROR(message, resultCode) {
  NS_ASSERT(false, SEARCH_LOG_PREFIX + message);
  throw Components.Exception(message, resultCode);
}

/**
 * Logs the failure message (if browser.search.log is enabled) and throws.
 * @param  message
 *         A message to display
 * @param  resultCode
 *         The NS_ERROR_* value to throw.
 * @throws resultCode or NS_ERROR_INVALID_ARG if resultCode isn't specified.
 */
function FAIL(message, resultCode) {
  LOG(message);
  throw Components.Exception(message, resultCode || Cr.NS_ERROR_INVALID_ARG);
}

/**
 * Truncates big blobs of (data-)URIs to console-friendly sizes
 * @param str
 *        String to tone down
 * @param len
 *        Maximum length of the string to return. Defaults to the length of a tweet.
 */
function limitURILength(str, len) {
  len = len || 140;
  if (str.length > len)
    return str.slice(0, len) + "...";
  return str;
}

/**
 * Ensures an assertion is met before continuing. Should be used to indicate
 * fatal errors.
 * @param  assertion
 *         An assertion that must be met
 * @param  message
 *         A message to display if the assertion is not met
 * @param  resultCode
 *         The NS_ERROR_* value to throw if the assertion is not met
 * @throws resultCode
 */
function ENSURE_WARN(assertion, message, resultCode) {
  NS_ASSERT(assertion, SEARCH_LOG_PREFIX + message);
  if (!assertion)
    throw Components.Exception(message, resultCode);
}

function loadListener(aChannel, aEngine, aCallback) {
  this._channel = aChannel;
  this._bytes = [];
  this._engine = aEngine;
  this._callback = aCallback;
}
loadListener.prototype = {
  _callback: null,
  _channel: null,
  _countRead: 0,
  _engine: null,
  _stream: null,

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIRequestObserver,
    Ci.nsIStreamListener,
    Ci.nsIChannelEventSink,
    Ci.nsIInterfaceRequestor,
    // See FIXME comment below.
    Ci.nsIHttpEventSink,
    Ci.nsIProgressEventSink
  ]),

  // nsIRequestObserver
  onStartRequest: function(aRequest, aContext) {
    LOG("loadListener: Starting request: " + aRequest.name);
    this._stream = Cc["@mozilla.org/binaryinputstream;1"].
                   createInstance(Ci.nsIBinaryInputStream);
  },

  onStopRequest: function(aRequest, aContext, aStatusCode) {
    LOG("loadListener: Stopping request: " + aRequest.name);

    var requestFailed = !Components.isSuccessCode(aStatusCode);
    if (!requestFailed && (aRequest instanceof Ci.nsIHttpChannel))
      requestFailed = !aRequest.requestSucceeded;

    if (requestFailed || this._countRead == 0) {
      LOG("loadListener: request failed!");
      // send null so the callback can deal with the failure
      this._callback(null, this._engine);
    } else
      this._callback(this._bytes, this._engine);
    this._channel = null;
    this._engine  = null;
  },

  // nsIStreamListener
  onDataAvailable: function(aRequest, aContext,
                                                aInputStream, aOffset,
                                                aCount) {
    this._stream.setInputStream(aInputStream);

    // Get a byte array of the data
    this._bytes = this._bytes.concat(this._stream.readByteArray(aCount));
    this._countRead += aCount;
  },

  // nsIChannelEventSink
  asyncOnChannelRedirect: function(aOldChannel, aNewChannel,
                                                      aFlags, callback) {
    this._channel = aNewChannel;
    callback.onRedirectVerifyCallback(Components.results.NS_OK);
  },

  // nsIInterfaceRequestor
  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },

  // FIXME: bug 253127
  // nsIHttpEventSink
  onRedirect: function (aChannel, aNewChannel) {},
  // nsIProgressEventSink
  onProgress: function (aRequest, aContext, aProgress, aProgressMax) {},
  onStatus: function (aRequest, aContext, aStatus, aStatusArg) {}
}

function isPartnerBuild() {
  try {
    let distroID = Services.prefs.getCharPref("distribution.id");

    // Mozilla-provided builds (i.e. funnelcake) are not partner builds
    if (distroID && !distroID.startsWith("mozilla")) {
      return true;
    }
  } catch (e) {}

  return false;
}

function getVerificationHash(aName) {
  let disclaimer = "By modifying this file, I agree that I am doing so " +
    "only within $appName itself, using official, user-driven search " +
    "engine selection processes, and in a way which does not circumvent " +
    "user consent. I acknowledge that any attempt to change this file " +
    "from outside of $appName is a malicious act, and will be responded " +
    "to accordingly."

  let salt = OS.Path.basename(OS.Constants.Path.profileDir) + aName +
             disclaimer.replace(/\$appName/g, Services.appinfo.name);

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";

  // Data is an array of bytes.
  let data = converter.convertToByteArray(salt, {});
  let hasher = Cc["@mozilla.org/security/hash;1"]
                 .createInstance(Ci.nsICryptoHash);
  hasher.init(hasher.SHA256);
  hasher.update(data, data.length);

  return hasher.finish(true);
}

/**
 * Safely close a nsISafeOutputStream.
 * @param aFOS
 *        The file output stream to close.
 */
function closeSafeOutputStream(aFOS) {
  if (aFOS instanceof Ci.nsISafeOutputStream) {
    try {
      aFOS.finish();
      return;
    } catch (e) { }
  }
  aFOS.close();
}

/**
 * Wrapper function for nsIIOService::newURI.
 * @param aURLSpec
 *        The URL string from which to create an nsIURI.
 * @returns an nsIURI object, or null if the creation of the URI failed.
 */
function makeURI(aURLSpec, aCharset) {
  try {
    return NetUtil.newURI(aURLSpec, aCharset);
  } catch (ex) { }

  return null;
}

/**
 * Wrapper function for nsIIOService::newChannel2.
 * @param url
 *        The URL string from which to create an nsIChannel.
 * @returns an nsIChannel object, or null if the url is invalid.
 */
function makeChannel(url) {
  try {
    return NetUtil.newChannel({
             uri: url,
             loadUsingSystemPrincipal: true
           });
  } catch (ex) { }

  return null;
}

/**
 * Gets a directory from the directory service.
 * @param aKey
 *        The directory service key indicating the directory to get.
 */
function getDir(aKey, aIFace) {
  if (!aKey)
    FAIL("getDir requires a directory key!");

  return Services.dirsvc.get(aKey, aIFace || Ci.nsIFile);
}

/**
 * Gets the current value of the locale.  It's possible for this preference to
 * be localized, so we have to do a little extra work here.  Similar code
 * exists in nsHttpHandler.cpp when building the UA string.
 */
function getLocale() {
  let locale = getLocalizedPref(LOCALE_PREF);
  if (locale)
    return locale;

  // Not localized.
  return Services.prefs.getCharPref(LOCALE_PREF);
}

/**
 * Wrapper for nsIPrefBranch::getComplexValue.
 * @param aPrefName
 *        The name of the pref to get.
 * @returns aDefault if the requested pref doesn't exist.
 */
function getLocalizedPref(aPrefName, aDefault) {
  const nsIPLS = Ci.nsIPrefLocalizedString;
  try {
    return Services.prefs.getComplexValue(aPrefName, nsIPLS).data;
  } catch (ex) {}

  return aDefault;
}

/**
 * @return a sanitized name to be used as a filename, or a random name
 *         if a sanitized name cannot be obtained (if aName contains
 *         no valid characters).
 */
function sanitizeName(aName) {
  const maxLength = 60;
  const minLength = 1;
  var name = aName.toLowerCase();
  name = name.replace(/\s+/g, "-");
  name = name.replace(/[^-a-z0-9]/g, "");

  // Use a random name if our input had no valid characters.
  if (name.length < minLength)
    name = Math.random().toString(36).replace(/^.*\./, '');

  // Force max length.
  return name.substring(0, maxLength);
}

/**
 * Retrieve a pref from the search param branch.
 *
 * @param prefName
 *        The name of the pref.
 **/
function getMozParamPref(prefName) {
  let branch = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF + "param.");
  return encodeURIComponent(branch.getCharPref(prefName));
}

/**
 * Notifies watchers of SEARCH_ENGINE_TOPIC about changes to an engine or to
 * the state of the search service.
 *
 * @param aEngine
 *        The nsISearchEngine object to which the change applies.
 * @param aVerb
 *        A verb describing the change.
 *
 * @see nsIBrowserSearchService.idl
 */
var gInitialized = false;
function notifyAction(aEngine, aVerb) {
  if (gInitialized) {
    LOG("NOTIFY: Engine: \"" + aEngine.name + "\"; Verb: \"" + aVerb + "\"");
    Services.obs.notifyObservers(aEngine, SEARCH_ENGINE_TOPIC, aVerb);
  }
}

function parseJsonFromStream(aInputStream) {
  const json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
  const data = json.decodeFromStream(aInputStream, aInputStream.available());
  return data;
}

/**
 * Simple object representing a name/value pair.
 */
function QueryParameter(aName, aValue, aPurpose) {
  if (!aName || (aValue == null))
    FAIL("missing name or value for QueryParameter!");

  this.name = aName;
  this.value = aValue;
  this.purpose = aPurpose;
}

/**
 * Perform OpenSearch parameter substitution on aParamValue.
 *
 * @param aParamValue
 *        A string containing OpenSearch search parameters.
 * @param aSearchTerms
 *        The user-provided search terms. This string will inserted into
 *        aParamValue as the value of the OS_PARAM_USER_DEFINED parameter.
 *        This value must already be escaped appropriately - it is inserted
 *        as-is.
 * @param aEngine
 *        The engine which owns the string being acted on.
 *
 * @see http://opensearch.a9.com/spec/1.1/querysyntax/#core
 */
function ParamSubstitution(aParamValue, aSearchTerms, aEngine) {
  var value = aParamValue;

  var distributionID = Services.prefs.getCharPref(BROWSER_SEARCH_PREF + "distributionID",
                                                Services.appinfo.distributionID || "");
  var official = MOZ_OFFICIAL;
  try {
    if (Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "official"))
      official = "official";
    else
      official = "unofficial";
  }
  catch (ex) { }

  // Custom search parameters. These are only available to default search
  // engines.
  if (aEngine._isDefault) {
    value = value.replace(MOZ_PARAM_LOCALE, getLocale());
    value = value.replace(MOZ_PARAM_DIST_ID, distributionID);
    value = value.replace(MOZ_PARAM_OFFICIAL, official);
  }

  // Insert the OpenSearch parameters we're confident about
  value = value.replace(OS_PARAM_USER_DEFINED, aSearchTerms);
  value = value.replace(OS_PARAM_INPUT_ENCODING, aEngine.queryCharset);
  value = value.replace(OS_PARAM_LANGUAGE,
                        getLocale() || OS_PARAM_LANGUAGE_DEF);
  value = value.replace(OS_PARAM_OUTPUT_ENCODING,
                        OS_PARAM_OUTPUT_ENCODING_DEF);

  // Replace any optional parameters
  value = value.replace(OS_PARAM_OPTIONAL, "");

  // Insert any remaining required params with our default values
  for (var i = 0; i < OS_UNSUPPORTED_PARAMS.length; ++i) {
    value = value.replace(OS_UNSUPPORTED_PARAMS[i][0],
                          OS_UNSUPPORTED_PARAMS[i][1]);
  }

  return value;
}

/**
 * Creates an engineURL object, which holds the query URL and all parameters.
 *
 * @param aType
 *        A string containing the name of the MIME type of the search results
 *        returned by this URL.
 * @param aMethod
 *        The HTTP request method. Must be a case insensitive value of either
 *        "GET" or "POST".
 * @param aTemplate
 *        The URL to which search queries should be sent. For GET requests,
 *        must contain the string "{searchTerms}", to indicate where the user
 *        entered search terms should be inserted.
 * @param aResultDomain
 *        The root domain for this URL.  Defaults to the template's host.
 *
 * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag
 *
 * @throws NS_ERROR_NOT_IMPLEMENTED if aType is unsupported.
 */
function EngineURL(aType, aMethod, aTemplate, aResultDomain) {
  if (!aType || !aMethod || !aTemplate)
    FAIL("missing type, method or template for EngineURL!");

  var method = aMethod.toUpperCase();
  var type   = aType.toLowerCase();

  if (method != "GET" && method != "POST")
    FAIL("method passed to EngineURL must be \"GET\" or \"POST\"");

  this.type     = type;
  this.method   = method;
  this.params   = [];
  this.rels     = [];
  // Don't serialize expanded mozparams
  this.mozparams = {};

  var templateURI = makeURI(aTemplate);
  if (!templateURI)
    FAIL("new EngineURL: template is not a valid URI!", Cr.NS_ERROR_FAILURE);

  switch (templateURI.scheme) {
    case "http":
    case "https":
    // Disable these for now, see bug 295018
    // case "file":
    // case "resource":
      this.template = aTemplate;
      break;
    default:
      FAIL("new EngineURL: template uses invalid scheme!", Cr.NS_ERROR_FAILURE);
  }

  // If no resultDomain was specified in the engine definition file, use the
  // host from the template.
  this.resultDomain = aResultDomain || templateURI.host;
  // We never want to return a "www." prefix, so eventually strip it.
  if (this.resultDomain.startsWith("www.")) {
    this.resultDomain = this.resultDomain.substr(4);
  }
}
EngineURL.prototype = {

  addParam: function(aName, aValue, aPurpose) {
    this.params.push(new QueryParameter(aName, aValue, aPurpose));
  },

  // Note: This method requires that aObj has a unique name or the previous MozParams entry with
  // that name will be overwritten.
  _addMozParam: function(aObj) {
    aObj.mozparam = true;
    this.mozparams[aObj.name] = aObj;
  },

  getSubmission: function(aSearchTerms, aEngine, aPurpose) {
    var url = ParamSubstitution(this.template, aSearchTerms, aEngine);
    // Default to searchbar if the purpose is not provided
    var purpose = aPurpose || "searchbar";

    // If a particular purpose isn't defined in the plugin, fallback to 'searchbar'.
    if (!this.params.some(p => p.purpose !== undefined && p.purpose == purpose))
      purpose = "searchbar";

    // Create an application/x-www-form-urlencoded representation of our params
    // (name=value&name=value&name=value)
    var dataString = "";
    for (var i = 0; i < this.params.length; ++i) {
      var param = this.params[i];

      // If this parameter has a purpose, only add it if the purpose matches
      if (param.purpose !== undefined && param.purpose != purpose)
        continue;

      var value = ParamSubstitution(param.value, aSearchTerms, aEngine);

      dataString += (i > 0 ? "&" : "") + param.name + "=" + value;
    }

    var postData = null;
    if (this.method == "GET") {
      // GET method requests have no post data, and append the encoded
      // query string to the url...
      if (url.indexOf("?") == -1 && dataString)
        url += "?";
      url += dataString;
    } else if (this.method == "POST") {
      // POST method requests must wrap the encoded text in a MIME
      // stream and supply that as POSTDATA.
      var stringStream = Cc["@mozilla.org/io/string-input-stream;1"].
                         createInstance(Ci.nsIStringInputStream);
      stringStream.data = dataString;

      postData = Cc["@mozilla.org/network/mime-input-stream;1"].
                 createInstance(Ci.nsIMIMEInputStream);
      postData.addHeader("Content-Type", "application/x-www-form-urlencoded");
      postData.addContentLength = true;
      postData.setData(stringStream);
    }

    return new Submission(makeURI(url), postData);
  },

  _getTermsParameterName: function() {
    let queryParam = this.params.find(p => p.value == USER_DEFINED);
    return queryParam ? queryParam.name : "";
  },

  _hasRelation: function(aRel) {
    return this.rels.some(e => e == aRel.toLowerCase());
  },

  _initWithJSON: function(aJson, aEngine) {
    if (!aJson.params)
      return;

    this.rels = aJson.rels;

    for (let i = 0; i < aJson.params.length; ++i) {
      let param = aJson.params[i];
      if (param.mozparam) {
        if (param.condition == "pref") {
          let value = getMozParamPref(param.pref);
          this.addParam(param.name, value);
        }
        this._addMozParam(param);
      }
      else
        this.addParam(param.name, param.value, param.purpose || undefined);
    }
  },

  /**
   * Creates a JavaScript object that represents this URL.
   * @returns An object suitable for serialization as JSON.
   **/
  toJSON: function() {
    var json = {
      template: this.template,
      rels: this.rels,
      resultDomain: this.resultDomain
    };

    if (this.type != URLTYPE_SEARCH_HTML)
      json.type = this.type;
    if (this.method != "GET")
      json.method = this.method;

    function collapseMozParams(aParam) {
      return this.mozparams[aParam.name] || aParam;
    }
    json.params = this.params.map(collapseMozParams, this);

    return json;
  }
};

/**
 * nsISearchEngine constructor.
 * @param aLocation
 *        A nsILocalFile or nsIURI object representing the location of the
 *        search engine data file.
 * @param aIsReadOnly
 *        Boolean indicating whether the engine should be treated as read-only.
 */
function Engine(aLocation, aIsReadOnly) {
  this._readOnly = aIsReadOnly;
  this._urls = [];
  this._metaData = {};

  let file, uri;
  if (typeof aLocation == "string") {
    this._shortName = aLocation;
  } else if (aLocation instanceof Ci.nsILocalFile) {
    if (!aIsReadOnly) {
      // This is an engine that was installed in NS_APP_USER_SEARCH_DIR by a
      // previous version. We are converting the file to an engine stored only
      // in JSON, but we need to keep the reference to the profile file to
      // remove it if the user ever removes the engine.
      this._filePath = aLocation.persistentDescriptor;
    }
    file = aLocation;
  } else if (aLocation instanceof Ci.nsIURI) {
    switch (aLocation.scheme) {
      case "https":
      case "http":
      case "ftp":
      case "data":
      case "file":
      case "resource":
      case "chrome":
        uri = aLocation;
        break;
      default:
        ERROR("Invalid URI passed to the nsISearchEngine constructor",
              Cr.NS_ERROR_INVALID_ARG);
    }
  } else
    ERROR("Engine location is neither a File nor a URI object",
          Cr.NS_ERROR_INVALID_ARG);

  if (!this._shortName) {
    // If we don't have a shortName at this point, it's the first time we load
    // this engine, so let's generate the shortName, id and loadPath values.
    let shortName;
    if (file) {
      shortName = file.leafName;
    }
    else if (uri && uri instanceof Ci.nsIURL) {
      if (aIsReadOnly || (gEnvironment.get("XPCSHELL_TEST_PROFILE_DIR") &&
                          uri.scheme == "resource")) {
        shortName = uri.fileName;
      }
    }
    if (shortName && shortName.endsWith(".xml")) {
      this._shortName = shortName.slice(0, -4);
    }
    this._loadPath = this.getAnonymizedLoadPath(file, uri);

    if (!shortName && !aIsReadOnly) {
      // We are in the process of downloading and installing the engine.
      // We'll have the shortName and id once we are done parsing it.
     return;
    }

    // Build the id used for the legacy metadata storage, so that we
    // can do a one-time import of data from old profiles.
    if (this._isDefault ||
        (uri && uri.spec.startsWith(APP_SEARCH_PREFIX))) {
      // The second part of the check is to catch engines from language packs.
      // They aren't default engines (because they aren't app-shipped), but we
      // still need to give their id an [app] prefix for backward compat.
      this._id = "[app]/" + this._shortName + ".xml";
    }
    else if (!aIsReadOnly) {
      this._id = "[profile]/" + this._shortName + ".xml";
    }
    else {
      // If the engine is neither a default one, nor a user-installed one,
      // it must be extension-shipped, so use the full path as id.
      LOG("Setting _id to full path for engine from " + this._loadPath);
      this._id = file ? file.path : uri.spec;
    }
  }
}

Engine.prototype = {
  // Data set by the user.
  _metaData: null,
  // The data describing the engine, in the form of an XML document element.
  _data: null,
  // Whether or not the engine is readonly.
  _readOnly: true,
  // Anonymized path of where we initially loaded the engine from.
  // This will stay null for engines installed in the profile before we moved
  // to a JSON storage.
  _loadPath: null,
  // The engine's description
  _description: "",
  // Used to store the engine to replace, if we're an update to an existing
  // engine.
  _engineToUpdate: null,
  // Set to true if the engine has a preferred icon (an icon that should not be
  // overridden by a non-preferred icon).
  _hasPreferredIcon: null,
  // The engine's name.
  _name: null,
  // The name of the charset used to submit the search terms.
  _queryCharset: null,
  // The engine's raw SearchForm value (URL string pointing to a search form).
  __searchForm: null,
  get _searchForm() {
    return this.__searchForm;
  },
  set _searchForm(aValue) {
    if (/^https?:/i.test(aValue))
      this.__searchForm = aValue;
    else
      LOG("_searchForm: Invalid URL dropped for " + this._name ||
          "the current engine");
  },
  // Whether to obtain user confirmation before adding the engine. This is only
  // used when the engine is first added to the list.
  _confirm: false,
  // Whether to set this as the current engine as soon as it is loaded.  This
  // is only used when the engine is first added to the list.
  _useNow: false,
  // A function to be invoked when this engine object's addition completes (or
  // fails). Only used for installation via addEngine.
  _installCallback: null,
  // The number of days between update checks for new versions
  _updateInterval: null,
  // The url to check at for a new update
  _updateURL: null,
  // The url to check for a new icon
  _iconUpdateURL: null,
  /* The extension ID if added by an extension. */
  _extensionID: null,

  /**
   * Retrieves the data from the engine's file.
   * The document element is placed in the engine's data field.
   */
  _initFromFile: function(file) {
    if (!file || !file.exists())
      FAIL("File must exist before calling initFromFile!", Cr.NS_ERROR_UNEXPECTED);

    var fileInStream = Cc["@mozilla.org/network/file-input-stream;1"].
                       createInstance(Ci.nsIFileInputStream);

    fileInStream.init(file, MODE_RDONLY, FileUtils.PERMS_FILE, false);

    var domParser = Cc["@mozilla.org/xmlextras/domparser;1"].
                    createInstance(Ci.nsIDOMParser);
    var doc = domParser.parseFromStream(fileInStream, "UTF-8",
                                        file.fileSize,
                                        "text/xml");

    this._data = doc.documentElement;
    fileInStream.close();

    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  },

  /**
   * Retrieves the data from the engine's file asynchronously.
   * The document element is placed in the engine's data field.
   *
   * @param file The file to load the search plugin from.
   *
   * @returns {Promise} A promise, resolved successfully if initializing from
   * data succeeds, rejected if it fails.
   */
  _asyncInitFromFile: Task.async(function* (file) {
    if (!file || !(yield OS.File.exists(file.path)))
      FAIL("File must exist before calling initFromFile!", Cr.NS_ERROR_UNEXPECTED);

    let fileURI = NetUtil.ioService.newFileURI(file);
    yield this._retrieveSearchXMLData(fileURI.spec);

    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  }),

  /**
   * Retrieves the engine data from a URI. Initializes the engine, flushes to
   * disk, and notifies the search service once initialization is complete.
   *
   * @param uri The uri to load the search plugin from.
   */
  _initFromURIAndLoad: function(uri) {
    ENSURE_WARN(uri instanceof Ci.nsIURI,
                "Must have URI when calling _initFromURIAndLoad!",
                Cr.NS_ERROR_UNEXPECTED);

    LOG("_initFromURIAndLoad: Downloading engine from: \"" + uri.spec + "\".");

    var chan = NetUtil.newChannel({
                 uri: uri,
                 loadUsingSystemPrincipal: true
               });

    if (this._engineToUpdate && (chan instanceof Ci.nsIHttpChannel)) {
      var lastModified = this._engineToUpdate.getAttr("updatelastmodified");
      if (lastModified)
        chan.setRequestHeader("If-Modified-Since", lastModified, false);
    }
    this._uri = uri;
    var listener = new loadListener(chan, this, this._onLoad);
    chan.notificationCallbacks = listener;
    chan.asyncOpen2(listener);
  },

  /**
   * Retrieves the engine data from a URI asynchronously and initializes it.
   *
   * @param uri The uri to load the search plugin from.
   *
   * @returns {Promise} A promise, resolved successfully if retrieveing data
   * succeeds.
   */
  _asyncInitFromURI: Task.async(function* (uri) {
    LOG("_asyncInitFromURI: Loading engine from: \"" + uri.spec + "\".");
    yield this._retrieveSearchXMLData(uri.spec);
    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  }),

  /**
   * Retrieves the engine data for a given URI asynchronously.
   *
   * @returns {Promise} A promise, resolved successfully if retrieveing data
   * succeeds.
   */
  _retrieveSearchXMLData: function(aURL) {
    let deferred = Promise.defer();
    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                    createInstance(Ci.nsIXMLHttpRequest);
    request.overrideMimeType("text/xml");
    request.onload = (aEvent) => {
      let responseXML = aEvent.target.responseXML;
      this._data = responseXML.documentElement;
      deferred.resolve();
    };
    request.onerror = function(aEvent) {
      deferred.resolve();
    };
    request.open("GET", aURL, true);
    request.send();

    return deferred.promise;
  },

  _initFromURISync: function(uri) {
    ENSURE_WARN(uri instanceof Ci.nsIURI,
                "Must have URI when calling _initFromURISync!",
                Cr.NS_ERROR_UNEXPECTED);

    ENSURE_WARN(uri.schemeIs("resource"), "_initFromURISync called for non-resource URI",
                Cr.NS_ERROR_FAILURE);

    LOG("_initFromURISync: Loading engine from: \"" + uri.spec + "\".");

    var chan = NetUtil.newChannel({
                 uri: uri,
                 loadUsingSystemPrincipal: true
               });

    var stream = chan.open2();
    var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(Ci.nsIDOMParser);
    var doc = parser.parseFromStream(stream, "UTF-8", stream.available(), "text/xml");

    this._data = doc.documentElement;

    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  },

  /**
   * Attempts to find an EngineURL object in the set of EngineURLs for
   * this Engine that has the given type string.  (This corresponds to the
   * "type" attribute in the "Url" node in the OpenSearch spec.)
   * This method will return the first matching URL object found, or null
   * if no matching URL is found.
   *
   * @param aType string to match the EngineURL's type attribute
   * @param aRel [optional] only return URLs that with this rel value
   */
  _getURLOfType: function(aType, aRel) {
    for (var i = 0; i < this._urls.length; ++i) {
      if (this._urls[i].type == aType && (!aRel || this._urls[i]._hasRelation(aRel)))
        return this._urls[i];
    }

    return null;
  },

  _confirmAddEngine: function() {
    var stringBundle = Services.strings.createBundle(SEARCH_BUNDLE);
    var titleMessage = stringBundle.GetStringFromName("addEngineConfirmTitle");

    // Display only the hostname portion of the URL.
    var dialogMessage =
        stringBundle.formatStringFromName("addEngineConfirmation",
                                          [this._name, this._uri.host], 2);
    var checkboxMessage = null;
    if (!Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "noCurrentEngine", false))
      checkboxMessage = stringBundle.GetStringFromName("addEngineAsCurrentText");

    var addButtonLabel =
        stringBundle.GetStringFromName("addEngineAddButtonLabel");

    var ps = Services.prompt;
    var buttonFlags = (ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0) +
                      (ps.BUTTON_TITLE_CANCEL    * ps.BUTTON_POS_1) +
                       ps.BUTTON_POS_0_DEFAULT;

    var checked = {value: false};
    // confirmEx returns the index of the button that was pressed.  Since "Add"
    // is button 0, we want to return the negation of that value.
    var confirm = !ps.confirmEx(null,
                                titleMessage,
                                dialogMessage,
                                buttonFlags,
                                addButtonLabel,
                                null, null, // button 1 & 2 names not used
                                checkboxMessage,
                                checked);

    return {confirmed: confirm, useNow: checked.value};
  },

  /**
   * Handle the successful download of an engine. Initializes the engine and
   * triggers parsing of the data. The engine is then flushed to disk. Notifies
   * the search service once initialization is complete.
   */
  _onLoad: function(aBytes, aEngine) {
    /**
     * Handle an error during the load of an engine by notifying the engine's
     * error callback, if any.
     */
    function onError(errorCode = Ci.nsISearchInstallCallback.ERROR_UNKNOWN_FAILURE) {
      // Notify the callback of the failure
      if (aEngine._installCallback) {
        aEngine._installCallback(errorCode);
      }
    }

    function promptError(strings = {}, error = undefined) {
      onError(error);

      if (aEngine._engineToUpdate) {
        // We're in an update, so just fail quietly
        LOG("updating " + aEngine._engineToUpdate.name + " failed");
        return;
      }
      var brandBundle = Services.strings.createBundle(BRAND_BUNDLE);
      var brandName = brandBundle.GetStringFromName("brandShortName");

      var searchBundle = Services.strings.createBundle(SEARCH_BUNDLE);
      var msgStringName = strings.error || "error_loading_engine_msg2";
      var titleStringName = strings.title || "error_loading_engine_title";
      var title = searchBundle.GetStringFromName(titleStringName);
      var text = searchBundle.formatStringFromName(msgStringName,
                                                   [brandName, aEngine._location],
                                                   2);

      Services.ww.getNewPrompter(null).alert(title, text);
    }

    if (!aBytes) {
      promptError();
      return;
    }

    var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(Ci.nsIDOMParser);
    var doc = parser.parseFromBuffer(aBytes, aBytes.length, "text/xml");
    aEngine._data = doc.documentElement;

    try {
      // Initialize the engine from the obtained data
      aEngine._initFromData();
    } catch (ex) {
      LOG("_onLoad: Failed to init engine!\n" + ex);
      // Report an error to the user
      promptError();
      return;
    }

    if (aEngine._engineToUpdate) {
      let engineToUpdate = aEngine._engineToUpdate.wrappedJSObject;

      // Make this new engine use the old engine's shortName, and preserve
      // metadata.
      aEngine._shortName = engineToUpdate._shortName;
      Object.keys(engineToUpdate._metaData).forEach(key => {
        aEngine.setAttr(key, engineToUpdate.getAttr(key));
      });
      aEngine._loadPath = engineToUpdate._loadPath;

      // Keep track of the last modified date, so that we can make conditional
      // requests for future updates.
      aEngine.setAttr("updatelastmodified", (new Date()).toUTCString());

      // Set the new engine's icon, if it doesn't yet have one.
      if (!aEngine._iconURI && engineToUpdate._iconURI)
        aEngine._iconURI = engineToUpdate._iconURI;
    } else {
      // Check that when adding a new engine (e.g., not updating an
      // existing one), a duplicate engine does not already exist.
      if (Services.search.getEngineByName(aEngine.name)) {
        // If we're confirming the engine load, then display a "this is a
        // duplicate engine" prompt; otherwise, fail silently.
        if (aEngine._confirm) {
          promptError({ error: "error_duplicate_engine_msg",
                        title: "error_invalid_engine_title"
                      }, Ci.nsISearchInstallCallback.ERROR_DUPLICATE_ENGINE);
        } else {
          onError(Ci.nsISearchInstallCallback.ERROR_DUPLICATE_ENGINE);
        }
        LOG("_onLoad: duplicate engine found, bailing");
        return;
      }

      // If requested, confirm the addition now that we have the title.
      // This property is only ever true for engines added via
      // nsIBrowserSearchService::addEngine.
      if (aEngine._confirm) {
        var confirmation = aEngine._confirmAddEngine();
        LOG("_onLoad: confirm is " + confirmation.confirmed +
            "; useNow is " + confirmation.useNow);
        if (!confirmation.confirmed) {
          onError();
          return;
        }
        aEngine._useNow = confirmation.useNow;
      }

      aEngine._shortName = sanitizeName(aEngine.name);
      aEngine._loadPath = aEngine.getAnonymizedLoadPath(null, aEngine._uri);
      aEngine.setAttr("loadPathHash", getVerificationHash(aEngine._loadPath));
    }

    // Notify the search service of the successful load. It will deal with
    // updates by checking aEngine._engineToUpdate.
    notifyAction(aEngine, SEARCH_ENGINE_LOADED);

    // Notify the callback if needed
    if (aEngine._installCallback) {
      aEngine._installCallback();
    }
  },

  /**
   * Creates a key by serializing an object that contains the icon's width
   * and height.
   *
   * @param aWidth
   *        Width of the icon.
   * @param aHeight
   *        Height of the icon.
   * @returns key string
   */
  _getIconKey: function(aWidth, aHeight) {
    let keyObj = {
     width: aWidth,
     height: aHeight
    };

    return JSON.stringify(keyObj);
  },

  /**
   * Add an icon to the icon map used by getIconURIBySize() and getIcons().
   *
   * @param aWidth
   *        Width of the icon.
   * @param aHeight
   *        Height of the icon.
   * @param aURISpec
   *        String with the icon's URI.
   */
  _addIconToMap: function(aWidth, aHeight, aURISpec) {
    if (aWidth == 16 && aHeight == 16) {
      // The 16x16 icon is stored in _iconURL, we don't need to store it twice.
      return;
    }

    // Use an object instead of a Map() because it needs to be serializable.
    this._iconMapObj = this._iconMapObj || {};
    let key = this._getIconKey(aWidth, aHeight);
    this._iconMapObj[key] = aURISpec;
  },

  /**
   * Sets the .iconURI property of the engine. If both aWidth and aHeight are
   * provided an entry will be added to _iconMapObj that will enable accessing
   * icon's data through getIcons() and getIconURIBySize() APIs.
   *
   *  @param aIconURL
   *         A URI string pointing to the engine's icon. Must have a http[s],
   *         ftp, or data scheme. Icons with HTTP[S] or FTP schemes will be
   *         downloaded and converted to data URIs for storage in the engine
   *         XML files, if the engine is not readonly.
   *  @param aIsPreferred
   *         Whether or not this icon is to be preferred. Preferred icons can
   *         override non-preferred icons.
   *  @param aWidth (optional)
   *         Width of the icon.
   *  @param aHeight (optional)
   *         Height of the icon.
   */
  _setIcon: function(aIconURL, aIsPreferred, aWidth, aHeight) {
    var uri = makeURI(aIconURL);

    // Ignore bad URIs
    if (!uri)
      return;

    LOG("_setIcon: Setting icon url \"" + limitURILength(uri.spec) + "\" for engine \""
        + this.name + "\".");
    // Only accept remote icons from http[s] or ftp
    switch (uri.scheme) {
      case "resource":
      case "chrome":
        // We only allow chrome and resource icon URLs for built-in search engines
        if (!this._isDefault) {
          return;
        }
        // Fall through to the data case
      case "data":
        if (!this._hasPreferredIcon || aIsPreferred) {
          this._iconURI = uri;
          notifyAction(this, SEARCH_ENGINE_CHANGED);
          this._hasPreferredIcon = aIsPreferred;
        }

        if (aWidth && aHeight) {
          this._addIconToMap(aWidth, aHeight, aIconURL)
        }
        break;
      case "http":
      case "https":
      case "ftp":
        LOG("_setIcon: Downloading icon: \"" + uri.spec +
            "\" for engine: \"" + this.name + "\"");
        var chan = NetUtil.newChannel({
                     uri: uri,
                     loadUsingSystemPrincipal: true
                   });

        let iconLoadCallback = function (aByteArray, aEngine) {
          // This callback may run after we've already set a preferred icon,
          // so check again.
          if (aEngine._hasPreferredIcon && !aIsPreferred)
            return;

          if (!aByteArray || aByteArray.length > MAX_ICON_SIZE) {
            LOG("iconLoadCallback: load failed, or the icon was too large!");
            return;
          }

          let type = chan.contentType;
          if (!type.startsWith("image/")) {
            console.warn(
              "Search service: Unable to set icon, content type is not an image",
              contentType);
            return;
          }            
          let dataURL = "data:" + type + ";base64," +
            btoa(String.fromCharCode.apply(null, aByteArray));

          aEngine._iconURI = makeURI(dataURL);

          if (aWidth && aHeight) {
            aEngine._addIconToMap(aWidth, aHeight, dataURL)
          }

          notifyAction(aEngine, SEARCH_ENGINE_CHANGED);
          aEngine._hasPreferredIcon = aIsPreferred;
        };

        // If we're currently acting as an "update engine", then the callback
        // should set the icon on the engine we're updating and not us, since
        // |this| might be gone by the time the callback runs.
        var engineToSet = this._engineToUpdate || this;

        var listener = new loadListener(chan, engineToSet, iconLoadCallback);
        chan.notificationCallbacks = listener;
        chan.asyncOpen2(listener);
        break;
    }
  },

  /**
   * Initialize this Engine object from the collected data.
   */
  _initFromData: function() {
    ENSURE_WARN(this._data, "Can't init an engine with no data!",
                Cr.NS_ERROR_UNEXPECTED);

    // Ensure we have a supported engine type before attempting to parse it.
    let element = this._data;
    if ((element.localName == MOZSEARCH_LOCALNAME &&
         element.namespaceURI == MOZSEARCH_NS_10) ||
        (element.localName == OPENSEARCH_LOCALNAME &&
         OPENSEARCH_NAMESPACES.indexOf(element.namespaceURI) != -1)) {
      LOG("_init: Initing search plugin from " + this._location);

      this._parse();

    } else
      FAIL(this._location + " is not a valid search plugin.", Cr.NS_ERROR_FAILURE);

    // No need to keep a ref to our data (which in some cases can be a document
    // element) past this point
    this._data = null;
  },

  /**
   * Initialize this Engine object from a collection of metadata.
   */
  _initFromMetadata: function(aName, aIconURL, aAlias,
                                                    aDescription, aMethod,
                                                    aTemplate, aExtensionID) {
    ENSURE_WARN(!this._readOnly,
                "Can't call _initFromMetaData on a readonly engine!",
                Cr.NS_ERROR_FAILURE);

    this._urls.push(new EngineURL(URLTYPE_SEARCH_HTML, aMethod, aTemplate));

    this._name = aName;
    this.alias = aAlias;
    this._description = aDescription;
    this._setIcon(aIconURL, true);
    this._extensionID = aExtensionID;
  },

  /**
   * Extracts data from an OpenSearch URL element and creates an EngineURL
   * object which is then added to the engine's list of URLs.
   *
   * @throws NS_ERROR_FAILURE if a URL object could not be created.
   *
   * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag.
   * @see EngineURL()
   */
  _parseURL: function(aElement) {
    var type     = aElement.getAttribute("type");
    // According to the spec, method is optional, defaulting to "GET" if not
    // specified
    var method   = aElement.getAttribute("method") || "GET";
    var template = aElement.getAttribute("template");
    var resultDomain = aElement.getAttribute("resultdomain");

    try {
      var url = new EngineURL(type, method, template, resultDomain);
    } catch (ex) {
      FAIL("_parseURL: failed to add " + template + " as a URL",
           Cr.NS_ERROR_FAILURE);
    }

    if (aElement.hasAttribute("rel"))
      url.rels = aElement.getAttribute("rel").toLowerCase().split(/\s+/);

    for (var i = 0; i < aElement.childNodes.length; ++i) {
      var param = aElement.childNodes[i];
      if (param.localName == "Param") {
        try {
          url.addParam(param.getAttribute("name"), param.getAttribute("value"));
        } catch (ex) {
          // Ignore failure
          LOG("_parseURL: Url element has an invalid param");
        }
      } else if (param.localName == "MozParam" &&
                 // We only support MozParams for default search engines
                 this._isDefault) {
        var value;
        let condition = param.getAttribute("condition");

        // MozParams must have a condition to be valid
        if (!condition) {
          let engineLoc = this._location;
          let paramName = param.getAttribute("name");
          LOG("_parseURL: MozParam (" + paramName + ") without a condition attribute found parsing engine: " + engineLoc);
          continue;
        }

        switch (condition) {
          case "purpose":
            url.addParam(param.getAttribute("name"),
                         param.getAttribute("value"),
                         param.getAttribute("purpose"));
            // _addMozParam is not needed here since it can be serialized fine without. _addMozParam
            // also requires a unique "name" which is not normally the case when @purpose is used.
            break;
          case "pref":
            try {
              value = getMozParamPref(param.getAttribute("pref"), value);
              url.addParam(param.getAttribute("name"), value);
              url._addMozParam({"pref": param.getAttribute("pref"),
                                "name": param.getAttribute("name"),
                                "condition": "pref"});
            } catch (e) { }
            break;
          default:
            let engineLoc = this._location;
            let paramName = param.getAttribute("name");
            LOG("_parseURL: MozParam (" + paramName + ") has an unknown condition: " + condition + ". Found parsing engine: " + engineLoc);
          break;
        }
      }
    }

    this._urls.push(url);
  },

  /**
   * Get the icon from an OpenSearch Image element.
   * @see http://opensearch.a9.com/spec/1.1/description/#image
   */
  _parseImage: function(aElement) {
    LOG("_parseImage: Image textContent: \"" + limitURILength(aElement.textContent) + "\"");

    let width = parseInt(aElement.getAttribute("width"), 10);
    let height = parseInt(aElement.getAttribute("height"), 10);
    let isPrefered = width == 16 && height == 16;

    if (isNaN(width) || isNaN(height) || width <= 0 || height <=0) {
      LOG("OpenSearch image element must have positive width and height.");
      return;
    }

    this._setIcon(aElement.textContent, isPrefered, width, height);
  },

  /**
   * Extract search engine information from the collected data to initialize
   * the engine object.
   */
  _parse: function() {
    var doc = this._data;

    // The OpenSearch spec sets a default value for the input encoding.
    this._queryCharset = OS_PARAM_INPUT_ENCODING_DEF;

    for (var i = 0; i < doc.childNodes.length; ++i) {
      var child = doc.childNodes[i];
      switch (child.localName) {
        case "ShortName":
          this._name = child.textContent;
          break;
        case "Description":
          this._description = child.textContent;
          break;
        case "Url":
          try {
            this._parseURL(child);
          } catch (ex) {
            // Parsing of the element failed, just skip it.
            LOG("_parse: failed to parse URL child: " + ex);
          }
          break;
        case "Image":
          this._parseImage(child);
          break;
        case "InputEncoding":
          this._queryCharset = child.textContent.toUpperCase();
          break;

        // Non-OpenSearch elements
        case "SearchForm":
          this._searchForm = child.textContent;
          break;
        case "UpdateUrl":
          this._updateURL = child.textContent;
          break;
        case "UpdateInterval":
          this._updateInterval = parseInt(child.textContent);
          break;
        case "IconUpdateUrl":
          this._iconUpdateURL = child.textContent;
          break;
        case "ExtensionID":
          this._extensionID = child.textContent;
          break;
      }
    }
    if (!this.name || (this._urls.length == 0))
      FAIL("_parse: No name, or missing URL!", Cr.NS_ERROR_FAILURE);
    if (!this.supportsResponseType(URLTYPE_SEARCH_HTML))
      FAIL("_parse: No text/html result type!", Cr.NS_ERROR_FAILURE);
  },

  /**
   * Init from a JSON record.
   **/
  _initWithJSON: function(aJson) {
    this._name = aJson._name;
    this._shortName = aJson._shortName;
    this._loadPath = aJson._loadPath;
    this._description = aJson.description;
    this._hasPreferredIcon = aJson._hasPreferredIcon == undefined;
    this._queryCharset = aJson.queryCharset || DEFAULT_QUERY_CHARSET;
    this.__searchForm = aJson.__searchForm;
    this._updateInterval = aJson._updateInterval || null;
    this._updateURL = aJson._updateURL || null;
    this._iconUpdateURL = aJson._iconUpdateURL || null;
    this._readOnly = aJson._readOnly == undefined;
    this._iconURI = makeURI(aJson._iconURL);
    this._iconMapObj = aJson._iconMapObj;
    this._metaData = aJson._metaData || {};
    if (aJson.filePath) {
      this._filePath = aJson.filePath;
    }
    if (aJson.dirPath) {
      this._dirPath = aJson.dirPath;
      this._dirLastModifiedTime = aJson.dirLastModifiedTime;
    }
    if (aJson.extensionID) {
      this._extensionID = aJson.extensionID;
    }
    for (let i = 0; i < aJson._urls.length; ++i) {
      let url = aJson._urls[i];
      let engineURL = new EngineURL(url.type || URLTYPE_SEARCH_HTML,
                                    url.method || "GET", url.template,
                                    url.resultDomain || undefined);
      engineURL._initWithJSON(url, this);
      this._urls.push(engineURL);
    }
  },

  /**
   * Creates a JavaScript object that represents this engine.
   * @returns An object suitable for serialization as JSON.
   **/
  toJSON: function() {
    var json = {
      _name: this._name,
      _shortName: this._shortName,
      _loadPath: this._loadPath,
      description: this.description,
      __searchForm: this.__searchForm,
      _iconURL: this._iconURL,
      _iconMapObj: this._iconMapObj,
      _metaData: this._metaData,
      _urls: this._urls
    };

    if (this._updateInterval)
      json._updateInterval = this._updateInterval;
    if (this._updateURL)
      json._updateURL = this._updateURL;
    if (this._iconUpdateURL)
      json._iconUpdateURL = this._iconUpdateURL;
    if (!this._hasPreferredIcon)
      json._hasPreferredIcon = this._hasPreferredIcon;
    if (this.queryCharset != DEFAULT_QUERY_CHARSET)
      json.queryCharset = this.queryCharset;
    if (!this._readOnly)
      json._readOnly = this._readOnly;
    if (this._filePath) {
      // File path is stored so that we can remove legacy xml files
      // from the profile if the user removes the engine.
      json.filePath = this._filePath;
    }
    if (this._dirPath) {
      // The directory path is only stored for extension-shipped engines,
      // it's used to invalidate the cache.
      json.dirPath = this._dirPath;
      json.dirLastModifiedTime = this._dirLastModifiedTime;
    }
    if (this._extensionID) {
      json.extensionID = this._extensionID;
    }

    return json;
  },

  setAttr(name, val) {
    this._metaData[name] = val;
  },

  getAttr(name) {
    return this._metaData[name] || undefined;
  },

  // nsISearchEngine
  get alias() {
    return this.getAttr("alias");
  },
  set alias(val) {
    var value = val ? val.trim() : null;
    this.setAttr("alias", value);
    notifyAction(this, SEARCH_ENGINE_CHANGED);
  },

  /**
   * Return the built-in identifier of app-provided engines.
   *
   * Note that this identifier is substantially similar to _id, with the
   * following exceptions:
   *
   * * There is no trailing file extension.
   * * There is no [app] prefix.
   *
   * @return a string identifier, or null.
   */
  get identifier() {
    // No identifier if If the engine isn't app-provided
    return this._isDefault ? this._shortName : null;
  },

  get description() {
    return this._description;
  },

  get hidden() {
    return this.getAttr("hidden") || false;
  },
  set hidden(val) {
    var value = !!val;
    if (value != this.hidden) {
      this.setAttr("hidden", value);
      notifyAction(this, SEARCH_ENGINE_CHANGED);
    }
  },

  get iconURI() {
    if (this._iconURI)
      return this._iconURI;
    return null;
  },

  get _iconURL() {
    if (!this._iconURI)
      return "";
    return this._iconURI.spec;
  },

  // Where the engine is being loaded from: will return the URI's spec if the
  // engine is being downloaded and does not yet have a file. This is only used
  // for logging and error messages.
  get _location() {
    if (this._uri)
      return this._uri.spec;

    return this._loadPath;
  },

  // This indicates where we found the .xml file to load the engine,
  // and attempts to hide user-identifiable data (such as username).
  getAnonymizedLoadPath(file, uri) {
    /* Examples of expected output:
     *   jar:[app]/omni.ja!browser/engine.xml
     *     'browser' here is the name of the chrome package, not a folder.
     *   [profile]/searchplugins/engine.xml
     *   [distribution]/searchplugins/common/engine.xml
     *   [other]/engine.xml
     */

    const NS_XPCOM_CURRENT_PROCESS_DIR = "XCurProcD";
    const NS_APP_USER_PROFILE_50_DIR = "ProfD";
    const XRE_APP_DISTRIBUTION_DIR = "XREAppDist";

    const knownDirs = {
      app: NS_XPCOM_CURRENT_PROCESS_DIR,
      profile: NS_APP_USER_PROFILE_50_DIR,
      distribution: XRE_APP_DISTRIBUTION_DIR
    };

    let leafName = this._shortName;
    if (!leafName)
      return "null";
    leafName += ".xml";

    let prefix = "", suffix = "";
    if (!file) {
      if (uri.schemeIs("resource")) {
        uri = makeURI(Services.io.getProtocolHandler("resource")
                              .QueryInterface(Ci.nsISubstitutingProtocolHandler)
                              .resolveURI(uri));
      }
      let scheme = uri.scheme;
      let packageName = "";
      if (scheme == "chrome") {
        packageName = uri.hostPort;
        uri = gChromeReg.convertChromeURL(uri);
      }

      if (AppConstants.platform == "android") {
        // On Android the omni.ja file isn't at the same path as the binary
        // used to start the process. We tweak the path here so that the code
        // shared with Desktop will correctly identify files from the omni.ja
        // file as coming from the [app] folder.
        let appPath = Services.io.getProtocolHandler("resource")
                              .QueryInterface(Ci.nsIResProtocolHandler)
                              .getSubstitution("android");
        if (appPath) {
          appPath = appPath.spec;
          let spec = uri.spec;
          if (spec.includes(appPath)) {
            let appURI = Services.io.newFileURI(getDir(knownDirs["app"]));
            uri = NetUtil.newURI(spec.replace(appPath, appURI.spec));
          }
        }
      }

      if (uri instanceof Ci.nsINestedURI) {
        prefix = "jar:";
        suffix = "!" + packageName + "/" + leafName;
        uri = uri.innermostURI;
      }
      if (uri instanceof Ci.nsIFileURL) {
        file = uri.file;
      } else {
        let path = "[" + scheme + "]";
        if (/^(?:https?|ftp)$/.test(scheme)) {
          path += uri.host;
        }
        return path + "/" + leafName;
      }
    }

    let id;
    let enginePath = file.path;

    for (let key in knownDirs) {
      let path;
      try {
        path = getDir(knownDirs[key]).path;
      } catch (e) {
        // Getting XRE_APP_DISTRIBUTION_DIR throws during unit tests.
        continue;
      }
      if (enginePath.startsWith(path)) {
        id = "[" + key + "]" + enginePath.slice(path.length).replace(/\\/g, "/");
        break;
      }
    }

    // If the folder doesn't have a known ancestor, don't record its path to
    // avoid leaking user identifiable data.
    if (!id)
      id = "[other]/" + file.leafName;

    return prefix + id + suffix;
  },

  get _isDefault() {
    // If we don't have a shortName, the engine is being parsed from a
    // downloaded file, so this can't be a default engine.
    if (!this._shortName)
      return false;

    // An engine is a default one if we initially loaded it from the application
    // or distribution directory.
    if (/^(?:jar:)?(?:\[app\]|\[distribution\])/.test(this._loadPath))
      return true;

    // If we are using a non-default locale or in the xpcshell test case,
    // we'll accept as a 'default' engine anything that has been registered at
    // resource://search-plugins/ even if the file doesn't come from the
    // application folder.  If not, skip costly additional checks.
    if (!Services.prefs.prefHasUserValue(LOCALE_PREF) &&
        !gEnvironment.get("XPCSHELL_TEST_PROFILE_DIR"))
      return false;

    // Some xpcshell tests use the search service without registering
    // resource://search-plugins/.
    if (!Services.io.getProtocolHandler("resource")
                 .QueryInterface(Ci.nsIResProtocolHandler)
                 .hasSubstitution("search-plugins"))
      return false;

    let uri = makeURI(APP_SEARCH_PREFIX + this._shortName + ".xml");
    if (this.getAnonymizedLoadPath(null, uri) == this._loadPath) {
      // This isn't a real default engine, but it's very close.
      LOG("_isDefault, pretending " + this._loadPath + " is a default engine");
      return true;
    }

    return false;
  },

  get _hasUpdates() {
    // Whether or not the engine has an update URL
    let selfURL = this._getURLOfType(URLTYPE_OPENSEARCH, "self");
    return !!(this._updateURL || this._iconUpdateURL || selfURL);
  },

  get name() {
    return this._name;
  },

  get searchForm() {
    return this._getSearchFormWithPurpose();
  },

  _getSearchFormWithPurpose(aPurpose = "") {
    // First look for a <Url rel="searchform">
    var searchFormURL = this._getURLOfType(URLTYPE_SEARCH_HTML, "searchform");
    if (searchFormURL) {
      let submission = searchFormURL.getSubmission("", this, aPurpose);

      // If the rel=searchform URL is not type="get" (i.e. has postData),
      // ignore it, since we can only return a URL.
      if (!submission.postData)
        return submission.uri.spec;
    }

    if (!this._searchForm) {
      // No SearchForm specified in the engine definition file, use the prePath
      // (e.g. https://foo.com for https://foo.com/search.php?q=bar).
      var htmlUrl = this._getURLOfType(URLTYPE_SEARCH_HTML);
      ENSURE_WARN(htmlUrl, "Engine has no HTML URL!", Cr.NS_ERROR_UNEXPECTED);
      this._searchForm = makeURI(htmlUrl.template).prePath;
    }

    return ParamSubstitution(this._searchForm, "", this);
  },

  get queryCharset() {
    if (this._queryCharset)
      return this._queryCharset;
    return this._queryCharset = "windows-1252"; // the default
  },

  // from nsISearchEngine
  addParam: function(aName, aValue, aResponseType) {
    if (!aName || (aValue == null))
      FAIL("missing name or value for nsISearchEngine::addParam!");
    ENSURE_WARN(!this._readOnly,
                "called nsISearchEngine::addParam on a read-only engine!",
                Cr.NS_ERROR_FAILURE);
    if (!aResponseType)
      aResponseType = URLTYPE_SEARCH_HTML;

    var url = this._getURLOfType(aResponseType);
    if (!url)
      FAIL("Engine object has no URL for response type " + aResponseType,
           Cr.NS_ERROR_FAILURE);

    url.addParam(aName, aValue);
  },

  get _defaultMobileResponseType() {
    let type = URLTYPE_SEARCH_HTML;

    let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
    let isTablet = sysInfo.get("tablet");
    if (isTablet && this.supportsResponseType("application/x-moz-tabletsearch")) {
      // Check for a tablet-specific search URL override
      type = "application/x-moz-tabletsearch";
    } else if (!isTablet && this.supportsResponseType("application/x-moz-phonesearch")) {
      // Check for a phone-specific search URL override
      type = "application/x-moz-phonesearch";
    }

    delete this._defaultMobileResponseType;
    return this._defaultMobileResponseType = type;
  },

  get _isWhiteListed() {
    let url = this._getURLOfType(URLTYPE_SEARCH_HTML).template;
    let hostname = makeURI(url).host;
    let whitelist = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF)
                            .getCharPref("reset.whitelist")
                            .split(",");
    if (whitelist.includes(hostname)) {
      LOG("The hostname " + hostname + " is white listed, " +
          "we won't show the search reset prompt");
      return true;
    }

    return false;
  },

  // from nsISearchEngine
  getSubmission: function(aData, aResponseType, aPurpose) {
    if (!aResponseType) {
      aResponseType = AppConstants.platform == "android" ? this._defaultMobileResponseType :
                                                           URLTYPE_SEARCH_HTML;
    }

    if (aResponseType == URLTYPE_SEARCH_HTML &&
        Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF).getBoolPref("reset.enabled") &&
        this.name == Services.search.currentEngine.name &&
        !this._isDefault &&
        this.name != Services.search.originalDefaultEngine.name &&
        (!this.getAttr("loadPathHash") ||
         this.getAttr("loadPathHash") != getVerificationHash(this._loadPath)) &&
        !this._isWhiteListed) {
      let url = "about:searchreset";
      let data = [];
      if (aData)
        data.push("data=" + encodeURIComponent(aData));
      if (aPurpose)
        data.push("purpose=" + aPurpose);
      if (data.length)
        url += "?" + data.join("&");
      return new Submission(makeURI(url));
    }

    var url = this._getURLOfType(aResponseType);

    if (!url)
      return null;

    if (!aData) {
      // Return a dummy submission object with our searchForm attribute
      return new Submission(makeURI(this._getSearchFormWithPurpose(aPurpose)));
    }

    LOG("getSubmission: In data: \"" + aData + "\"; Purpose: \"" + aPurpose + "\"");
    var data = "";
    try {
      data = gTextToSubURI.ConvertAndEscape(this.queryCharset, aData);
    } catch (ex) {
      LOG("getSubmission: Falling back to default queryCharset!");
      data = gTextToSubURI.ConvertAndEscape(DEFAULT_QUERY_CHARSET, aData);
    }
    LOG("getSubmission: Out data: \"" + data + "\"");
    return url.getSubmission(data, this, aPurpose);
  },

  // from nsISearchEngine
  supportsResponseType: function(type) {
    return (this._getURLOfType(type) != null);
  },

  // from nsISearchEngine
  getResultDomain: function(aResponseType) {
    if (!aResponseType) {
      aResponseType = AppConstants.platform == "android" ? this._defaultMobileResponseType :
                                                           URLTYPE_SEARCH_HTML;
    }

    LOG("getResultDomain: responseType: \"" + aResponseType + "\"");

    let url = this._getURLOfType(aResponseType);
    if (url)
      return url.resultDomain;
    return "";
  },

  /**
   * Returns URL parsing properties used by _buildParseSubmissionMap.
   */
  getURLParsingInfo: function () {
    let responseType = AppConstants.platform == "android" ? this._defaultMobileResponseType :
                                                            URLTYPE_SEARCH_HTML;

    LOG("getURLParsingInfo: responseType: \"" + responseType + "\"");

    let url = this._getURLOfType(responseType);
    if (!url || url.method != "GET") {
      return null;
    }

    let termsParameterName = url._getTermsParameterName();
    if (!termsParameterName) {
      return null;
    }

    let templateUrl = NetUtil.newURI(url.template).QueryInterface(Ci.nsIURL);
    return {
      mainDomain: templateUrl.host,
      path: templateUrl.filePath.toLowerCase(),
      termsParameterName: termsParameterName,
    };
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISearchEngine]),

  get wrappedJSObject() {
    return this;
  },

  /**
   * Returns a string with the URL to an engine's icon matching both width and
   * height. Returns null if icon with specified dimensions is not found.
   *
   * @param width
   *        Width of the requested icon.
   * @param height
   *        Height of the requested icon.
   */
  getIconURLBySize: function(aWidth, aHeight) {
    if (aWidth == 16 && aHeight == 16)
      return this._iconURL;

    if (!this._iconMapObj)
      return null;

    let key = this._getIconKey(aWidth, aHeight);
    if (key in this._iconMapObj) {
      return this._iconMapObj[key];
    }
    return null;
  },

  /**
   * Gets an array of all available icons. Each entry is an object with
   * width, height and url properties. width and height are numeric and
   * represent the icon's dimensions. url is a string with the URL for
   * the icon.
   */
  getIcons: function() {
    let result = [];
    if (this._iconURL)
      result.push({width: 16, height: 16, url: this._iconURL});

    if (!this._iconMapObj)
      return result;

    for (let key of Object.keys(this._iconMapObj)) {
      let iconSize = JSON.parse(key);
      result.push({
        width: iconSize.width,
        height: iconSize.height,
        url: this._iconMapObj[key]
      });
    }

    return result;
  },

  /**
   * Opens a speculative connection to the engine's search URI
   * (and suggest URI, if different) to reduce request latency
   *
   * @param  options
   *         An object that must contain the following fields:
   *         {window} the content window for the window performing the search
   *
   * @throws NS_ERROR_INVALID_ARG if options is omitted or lacks required
   *         elemeents
   */
  speculativeConnect: function(options) {
    if (!options || !options.window) {
      Cu.reportError("invalid options arg passed to nsISearchEngine.speculativeConnect");
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    let connector =
        Services.io.QueryInterface(Components.interfaces.nsISpeculativeConnect);

    let searchURI = this.getSubmission("dummy").uri;

    let callbacks = options.window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                           .getInterface(Components.interfaces.nsIWebNavigation)
                           .QueryInterface(Components.interfaces.nsILoadContext);

    connector.speculativeConnect(searchURI, callbacks);

    if (this.supportsResponseType(URLTYPE_SUGGEST_JSON)) {
      let suggestURI = this.getSubmission("dummy", URLTYPE_SUGGEST_JSON).uri;
      if (suggestURI.prePath != searchURI.prePath)
        connector.speculativeConnect(suggestURI, callbacks);
    }
  },
};

// nsISearchSubmission
function Submission(aURI, aPostData = null) {
  this._uri = aURI;
  this._postData = aPostData;
}
Submission.prototype = {
  get uri() {
    return this._uri;
  },
  get postData() {
    return this._postData;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISearchSubmission])
}

// nsISearchParseSubmissionResult
function ParseSubmissionResult(aEngine, aTerms, aTermsOffset, aTermsLength) {
  this._engine = aEngine;
  this._terms = aTerms;
  this._termsOffset = aTermsOffset;
  this._termsLength = aTermsLength;
}
ParseSubmissionResult.prototype = {
  get engine() {
    return this._engine;
  },
  get terms() {
    return this._terms;
  },
  get termsOffset() {
    return this._termsOffset;
  },
  get termsLength() {
    return this._termsLength;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISearchParseSubmissionResult]),
}

const gEmptyParseSubmissionResult =
      Object.freeze(new ParseSubmissionResult(null, "", -1, 0));

function executeSoon(func) {
  Services.tm.mainThread.dispatch(func, Ci.nsIThread.DISPATCH_NORMAL);
}

/**
 * Check for sync initialization has completed or not.
 *
 * @param {aPromise} A promise.
 *
 * @returns the value returned by the invoked method.
 * @throws NS_ERROR_ALREADY_INITIALIZED if sync initialization has completed.
 */
function checkForSyncCompletion(aPromise) {
  return aPromise.then(function(aValue) {
    if (gInitialized) {
      throw Components.Exception("Synchronous fallback was called and has " +
                                 "finished so no need to pursue asynchronous " +
                                 "initialization",
                                 Cr.NS_ERROR_ALREADY_INITIALIZED);
    }
    return aValue;
  });
}

// nsIBrowserSearchService
function SearchService() {
  // Replace empty LOG function with the useful one if the log pref is set.
  if (Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "log", false))
    LOG = DO_LOG;

  this._initObservers = Promise.defer();
}

SearchService.prototype = {
  classID: Components.ID("{7319788a-fe93-4db3-9f39-818cf08f4256}"),

  // The current status of initialization. Note that it does not determine if
  // initialization is complete, only if an error has been encountered so far.
  _initRV: Cr.NS_OK,

  // The boolean indicates that the initialization has started or not.
  _initStarted: null,

  // Reading the JSON cache file is the first thing done during initialization.
  // During the async init, we save it in a field so that if we have to do a
  // sync init before the async init finishes, we can avoid reading the cache
  // with sync disk I/O and handling lz4 decompression synchronously.
  // This is set back to null as soon as the initialization is finished.
  _cacheFileJSON: null,

  // If initialization has not been completed yet, perform synchronous
  // initialization.
  // Throws in case of initialization error.
  _ensureInitialized: function() {
    if (gInitialized) {
      if (!Components.isSuccessCode(this._initRV)) {
        LOG("_ensureInitialized: failure");
        throw this._initRV;
      }
      return;
    }

    let performanceWarning =
      "Search service falling back to synchronous initialization. " +
      "This is generally the consequence of an add-on using a deprecated " +
      "search service API.";
    Deprecated.perfWarning(performanceWarning, "https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsIBrowserSearchService#async_warning");
    LOG(performanceWarning);

    this._syncInit();
    if (!Components.isSuccessCode(this._initRV)) {
      throw this._initRV;
    }
  },

  // Synchronous implementation of the initializer.
  // Used by |_ensureInitialized| as a fallback if initialization is not
  // complete.
  _syncInit: function() {
    LOG("_syncInit start");
    this._initStarted = true;

    let cache = this._readCacheFile();
    if (cache.metaData)
      this._metaData = cache.metaData;

    try {
      this._syncLoadEngines(cache);
    } catch (ex) {
      this._initRV = Cr.NS_ERROR_FAILURE;
      LOG("_syncInit: failure loading engines: " + ex);
    }
    this._addObservers();

    gInitialized = true;
    this._cacheFileJSON = null;

    this._initObservers.resolve(this._initRV);

    Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "init-complete");
    Services.telemetry.getHistogramById("SEARCH_SERVICE_INIT_SYNC").add(true);
    this._recordEngineTelemetry();

    LOG("_syncInit end");
  },

  /**
   * Asynchronous implementation of the initializer.
   *
   * @returns {Promise} A promise, resolved successfully if the initialization
   * succeeds.
   */
  _asyncInit: Task.async(function* () {
    LOG("_asyncInit start");

    // See if we have a cache file so we don't have to parse a bunch of XML.
    let cache = {};
    // Not using checkForSyncCompletion here because we want to ensure we
    // fetch the country code and geo specific defaults asynchronously even
    // if a sync init has been forced.
    cache = yield this._asyncReadCacheFile();

    if (!gInitialized && cache.metaData)
      this._metaData = cache.metaData;

    try {
      yield checkForSyncCompletion(this._asyncLoadEngines(cache));
    } catch (ex) {
      if (ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {
        throw ex;
      }
      this._initRV = Cr.NS_ERROR_FAILURE;
      LOG("_asyncInit: failure loading engines: " + ex);
    }
    this._addObservers();
    gInitialized = true;
    this._cacheFileJSON = null;
    this._initObservers.resolve(this._initRV);
    Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "init-complete");
    Services.telemetry.getHistogramById("SEARCH_SERVICE_INIT_SYNC").add(false);
    this._recordEngineTelemetry();

    LOG("_asyncInit: Completed _asyncInit");
  }),

  _metaData: { },
  setGlobalAttr(name, val) {
    this._metaData[name] = val;
    this.batchTask.disarm();
    this.batchTask.arm();
  },
  setVerifiedGlobalAttr(name, val) {
    this.setGlobalAttr(name, val);
    this.setGlobalAttr(name + "Hash", getVerificationHash(val));
  },

  getGlobalAttr(name) {
    return this._metaData[name] || undefined;
  },
  getVerifiedGlobalAttr(name) {
    let val = this.getGlobalAttr(name);
    if (val && this.getGlobalAttr(name + "Hash") != getVerificationHash(val)) {
      LOG("getVerifiedGlobalAttr, invalid hash for " + name);
      return "";
    }
    return val;
  },

  _engines: { },
  __sortedEngines: null,
  _visibleDefaultEngines: [],
  get _sortedEngines() {
    if (!this.__sortedEngines)
      return this._buildSortedEngineList();
    return this.__sortedEngines;
  },

  // Get the original Engine object that is the default for this region,
  // ignoring changes the user may have subsequently made.
  get originalDefaultEngine() {
    let defaultEngine = this.getVerifiedGlobalAttr("searchDefault");
    if (!defaultEngine) {
      let defaultPrefB = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF);
      let nsIPLS = Ci.nsIPrefLocalizedString;

      try {
        defaultEngine = defaultPrefB.getComplexValue("defaultenginename", nsIPLS).data;
      } catch (ex) {
        // If the default pref is invalid (e.g. an add-on set it to a bogus value)
        // getEngineByName will just return null, which is the best we can do.
      }
    }

    return this.getEngineByName(defaultEngine);
  },

  resetToOriginalDefaultEngine: function() {
    this.currentEngine = this.originalDefaultEngine;
  },

  _buildCache: function() {
    if (this._batchTask)
      this._batchTask.disarm();

    let cache = {};
    let locale = getLocale();
    let buildID = Services.appinfo.platformBuildID;

    // Allows us to force a cache refresh should the cache format change.
    cache.version = CACHE_VERSION;
    // We don't want to incur the costs of stat()ing each plugin on every
    // startup when the only (supported) time they will change is during
    // app updates (where the buildID is obviously going to change).
    // Extension-shipped plugins are the only exception to this, but their
    // directories are blown away during updates, so we'll detect their changes.
    cache.buildID = buildID;
    cache.locale = locale;

    cache.visibleDefaultEngines = this._visibleDefaultEngines;
    cache.metaData = this._metaData;
    cache.engines = [];

    for (let name in this._engines) {
      cache.engines.push(this._engines[name]);
    }

    try {
      if (!cache.engines.length)
        throw "cannot write without any engine.";

      LOG("_buildCache: Writing to cache file.");
      let path = OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME);
      let data = gEncoder.encode(JSON.stringify(cache));
      let promise = OS.File.writeAtomic(path, data, {compression: "lz4",
                                                     tmpPath: path + ".tmp"});

      promise.then(
        function onSuccess() {
          Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, SEARCH_SERVICE_CACHE_WRITTEN);
        },
        function onError(e) {
          LOG("_buildCache: failure during writeAtomic: " + e);
        }
      );
    } catch (ex) {
      LOG("_buildCache: Could not write to cache file: " + ex);
    }
  },

  _syncLoadEngines: function(cache) {
    LOG("_syncLoadEngines: start");
    // See if we have a cache file so we don't have to parse a bunch of XML.
    let chromeURIs = this._findJAREngines();

    let distDirs = [];
    let locations;
    try {
      locations = getDir(NS_APP_DISTRIBUTION_SEARCH_DIR_LIST,
                         Ci.nsISimpleEnumerator);
    } catch (e) {
      // NS_APP_DISTRIBUTION_SEARCH_DIR_LIST is defined by each app
      // so this throws during unit tests (but not xpcshell tests).
      locations = {hasMoreElements: () => false};
    }
    while (locations.hasMoreElements()) {
      let dir = locations.getNext().QueryInterface(Ci.nsIFile);
      if (dir.directoryEntries.hasMoreElements())
        distDirs.push(dir);
    }

    let otherDirs = [];
    let userSearchDir = getDir(NS_APP_USER_SEARCH_DIR);
    locations = getDir(NS_APP_SEARCH_DIR_LIST, Ci.nsISimpleEnumerator);
    while (locations.hasMoreElements()) {
      let dir = locations.getNext().QueryInterface(Ci.nsIFile);
      if ((!cache.engines || !dir.equals(userSearchDir)) &&
          dir.directoryEntries.hasMoreElements())
        otherDirs.push(dir);
    }

    function modifiedDir(aDir) {
      return cacheOtherPaths.get(aDir.path) != aDir.lastModifiedTime;
    }

    function notInCacheVisibleEngines(aEngineName) {
      return cache.visibleDefaultEngines.indexOf(aEngineName) == -1;
    }

    let buildID = Services.appinfo.platformBuildID;
    let cacheOtherPaths = new Map();
    if (cache.engines) {
      for (let engine of cache.engines) {
        if (engine._dirPath) {
          cacheOtherPaths.set(engine._dirPath, engine._dirLastModifiedTime);
        }
      }
    }

    let rebuildCache = !cache.engines ||
                       cache.version != CACHE_VERSION ||
                       cache.locale != getLocale() ||
                       cache.buildID != buildID ||
                       cacheOtherPaths.size != otherDirs.length ||
                       otherDirs.some(d => !cacheOtherPaths.has(d.path)) ||
                       cache.visibleDefaultEngines.length != this._visibleDefaultEngines.length ||
                       this._visibleDefaultEngines.some(notInCacheVisibleEngines) ||
                       otherDirs.some(modifiedDir);

    if (rebuildCache) {
      LOG("_loadEngines: Absent or outdated cache. Loading engines from disk.");
      distDirs.forEach(this._loadEnginesFromDir, this);

      this._loadFromChromeURLs(chromeURIs);

      LOG("_loadEngines: load user-installed engines from the obsolete cache");
      this._loadEnginesFromCache(cache, true);

      otherDirs.forEach(this._loadEnginesFromDir, this);

      this._loadEnginesMetadataFromCache(cache);
      this._buildCache();
      return;
    }

    LOG("_loadEngines: loading from cache directories");
    this._loadEnginesFromCache(cache);

    LOG("_loadEngines: done");
  },

  /**
   * Loads engines asynchronously.
   *
   * @returns {Promise} A promise, resolved successfully if loading data
   * succeeds.
   */
  _asyncLoadEngines: Task.async(function* (cache) {
    LOG("_asyncLoadEngines: start");
    Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "find-jar-engines");
    let chromeURIs =
      yield checkForSyncCompletion(this._asyncFindJAREngines());

    // Get the non-empty distribution directories into distDirs...
    let distDirs = [];
    let locations;
    try {
      locations = getDir(NS_APP_DISTRIBUTION_SEARCH_DIR_LIST,
                         Ci.nsISimpleEnumerator);
    } catch (e) {
      // NS_APP_DISTRIBUTION_SEARCH_DIR_LIST is defined by each app
      // so this throws during unit tests (but not xpcshell tests).
      locations = {hasMoreElements: () => false};
    }
    while (locations.hasMoreElements()) {
      let dir = locations.getNext().QueryInterface(Ci.nsIFile);
      let iterator = new OS.File.DirectoryIterator(dir.path,
                                                   { winPattern: "*.xml" });
      try {
        // Add dir to distDirs if it contains any files.
        yield checkForSyncCompletion(iterator.next());
        distDirs.push(dir);
      } catch (ex) {
        // Catch for StopIteration exception.
        if (ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {
          throw ex;
        }
      } finally {
        iterator.close();
      }
    }

    // Add the non-empty directories of NS_APP_SEARCH_DIR_LIST to
    // otherDirs...
    let otherDirs = [];
    let userSearchDir = getDir(NS_APP_USER_SEARCH_DIR);
    locations = getDir(NS_APP_SEARCH_DIR_LIST, Ci.nsISimpleEnumerator);
    while (locations.hasMoreElements()) {
      let dir = locations.getNext().QueryInterface(Ci.nsIFile);
      if (cache.engines && dir.equals(userSearchDir))
        continue;
      let iterator = new OS.File.DirectoryIterator(dir.path,
                                                   { winPattern: "*.xml" });
      try {
        // Add dir to otherDirs if it contains any files.
        yield checkForSyncCompletion(iterator.next());
        otherDirs.push(dir);
      } catch (ex) {
        // Catch for StopIteration exception.
        if (ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {
          throw ex;
        }
      } finally {
        iterator.close();
      }
    }

    let hasModifiedDir = Task.async(function* (aList) {
      let modifiedDir = false;

      for (let dir of aList) {
        let lastModifiedTime = cacheOtherPaths.get(dir.path);
        if (!lastModifiedTime) {
          continue;
        }

        let info = yield OS.File.stat(dir.path);
        if (lastModifiedTime != info.lastModificationDate.getTime()) {
          modifiedDir = true;
          break;
        }
      }
      return modifiedDir;
    });

    function notInCacheVisibleEngines(aEngineName) {
      return cache.visibleDefaultEngines.indexOf(aEngineName) == -1;
    }

    let buildID = Services.appinfo.platformBuildID;
    let cacheOtherPaths = new Map();
    if (cache.engines) {
      for (let engine of cache.engines) {
        if (engine._dirPath) {
          cacheOtherPaths.set(engine._dirPath, engine._dirLastModifiedTime);
        }
      }
    }

    let rebuildCache = !cache.engines ||
                       cache.version != CACHE_VERSION ||
                       cache.locale != getLocale() ||
                       cache.buildID != buildID ||
                       cacheOtherPaths.size != otherDirs.length ||
                       otherDirs.some(d => !cacheOtherPaths.has(d.path)) ||
                       cache.visibleDefaultEngines.length != this._visibleDefaultEngines.length ||
                       this._visibleDefaultEngines.some(notInCacheVisibleEngines) ||
                       (yield checkForSyncCompletion(hasModifiedDir(otherDirs)));

    if (rebuildCache) {
      LOG("_asyncLoadEngines: Absent or outdated cache. Loading engines from disk.");
      for (let loadDir of distDirs) {
        let enginesFromDir =
          yield checkForSyncCompletion(this._asyncLoadEnginesFromDir(loadDir));
        enginesFromDir.forEach(this._addEngineToStore, this);
      }
      let enginesFromURLs =
        yield checkForSyncCompletion(this._asyncLoadFromChromeURLs(chromeURIs));
      enginesFromURLs.forEach(this._addEngineToStore, this);

      LOG("_asyncLoadEngines: loading user-installed engines from the obsolete cache");
      this._loadEnginesFromCache(cache, true);

      for (let loadDir of otherDirs) {
        let enginesFromDir =
          yield checkForSyncCompletion(this._asyncLoadEnginesFromDir(loadDir));
        enginesFromDir.forEach(this._addEngineToStore, this);
      }

      this._loadEnginesMetadataFromCache(cache);
      this._buildCache();
      return;
    }

    LOG("_asyncLoadEngines: loading from cache directories");
    this._loadEnginesFromCache(cache);

    LOG("_asyncLoadEngines: done");
  }),

  _asyncReInit: function () {
    LOG("_asyncReInit");
    // Start by clearing the initialized state, so we don't abort early.
    gInitialized = false;

    Task.spawn(function* () {
      try {
        if (this._batchTask) {
          LOG("finalizing batch task");
          let task = this._batchTask;
          this._batchTask = null;
          yield task.finalize();
        }

        // Clear the engines, too, so we don't stick with the stale ones.
        this._engines = {};
        this.__sortedEngines = null;
        this._currentEngine = null;
        this._visibleDefaultEngines = [];
        this._metaData = {};
        this._cacheFileJSON = null;

        // Tests that want to force a synchronous re-initialization need to
        // be notified when we are done uninitializing.
        Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC,
                                     "uninit-complete");

        let cache = {};
        cache = yield this._asyncReadCacheFile();
        if (!gInitialized && cache.metaData)
          this._metaData = cache.metaData;

        yield this._asyncLoadEngines(cache);

        // Typically we'll re-init as a result of a pref observer,
        // so signal to 'callers' that we're done.
        Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "init-complete");
        this._recordEngineTelemetry();
        gInitialized = true;
      } catch (err) {
        LOG("Reinit failed: " + err);
        Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "reinit-failed");
      } finally {
        Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "reinit-complete");
      }
    }.bind(this));
  },

  /**
   * Read the cache file synchronously. This also imports data from the old
   * search-metadata.json file if needed.
   *
   * @returns A JS object containing the cached data.
   */
  _readCacheFile: function() {
    if (this._cacheFileJSON) {
      return this._cacheFileJSON;
    }

    let cacheFile = getDir(NS_APP_USER_PROFILE_50_DIR);
    cacheFile.append(CACHE_FILENAME);

    let stream;
    try {
      stream = Cc["@mozilla.org/network/file-input-stream;1"].
                 createInstance(Ci.nsIFileInputStream);
      stream.init(cacheFile, MODE_RDONLY, FileUtils.PERMS_FILE, 0);

      let bis = Cc["@mozilla.org/binaryinputstream;1"]
                  .createInstance(Ci.nsIBinaryInputStream);
      bis.setInputStream(stream);

      let count = stream.available();
      let array = new Uint8Array(count);
      bis.readArrayBuffer(count, array.buffer);

      let bytes = Lz4.decompressFileContent(array);
      let json = JSON.parse(new TextDecoder().decode(bytes));
      if (!json.engines || !json.engines.length)
        throw "no engine in the file";
      return json;
    } catch (ex) {
      LOG("_readCacheFile: Error reading cache file: " + ex);
    } finally {
      stream.close();
    }

    try {
      cacheFile.leafName = "search-metadata.json";
      stream = Cc["@mozilla.org/network/file-input-stream;1"].
                 createInstance(Ci.nsIFileInputStream);
      stream.init(cacheFile, MODE_RDONLY, FileUtils.PERMS_FILE, 0);
      let metadata = parseJsonFromStream(stream);
      let json = {};
      if ("[global]" in metadata) {
        LOG("_readCacheFile: migrating metadata from search-metadata.json");
        let data = metadata["[global]"];
        json.metaData = {};
        let fields = ["searchDefault", "searchDefaultHash", "searchDefaultExpir",
                      "current", "hash",
                      "visibleDefaultEngines", "visibleDefaultEnginesHash"];
        for (let field of fields) {
          let name = field.toLowerCase();
          if (name in data)
            json.metaData[field] = data[name];
        }
      }
      delete metadata["[global]"];
      json._oldMetadata = metadata;

      return json;
    } catch (ex) {
      LOG("_readCacheFile: failed to read old metadata: " + ex);
      return {};
    } finally {
      stream.close();
    }
  },

  /**
   * Read the cache file asynchronously. This also imports data from the old
   * search-metadata.json file if needed.
   *
   * @returns {Promise} A promise, resolved successfully if retrieveing data
   * succeeds.
   */
  _asyncReadCacheFile: Task.async(function* () {
    let json;
    try {
      let cacheFilePath = OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME);
      let bytes = yield OS.File.read(cacheFilePath, {compression: "lz4"});
      json = JSON.parse(new TextDecoder().decode(bytes));
      if (!json.engines || !json.engines.length)
        throw "no engine in the file";
      this._cacheFileJSON = json;
    } catch (ex) {
      LOG("_asyncReadCacheFile: Error reading cache file: " + ex);
      json = {};

      let oldMetadata =
        OS.Path.join(OS.Constants.Path.profileDir, "search-metadata.json");
      try {
        let bytes = yield OS.File.read(oldMetadata);
        let metadata = JSON.parse(new TextDecoder().decode(bytes));
        if ("[global]" in metadata) {
          LOG("_asyncReadCacheFile: migrating metadata from search-metadata.json");
          let data = metadata["[global]"];
          json.metaData = {};
          let fields = ["searchDefault", "searchDefaultHash", "searchDefaultExpir",
                        "current", "hash",
                        "visibleDefaultEngines", "visibleDefaultEnginesHash"];
          for (let field of fields) {
            let name = field.toLowerCase();
            if (name in data)
              json.metaData[field] = data[name];
          }
        }
        delete metadata["[global]"];
        json._oldMetadata = metadata;
      } catch (ex) {}
    }
    return json;
  }),

  _batchTask: null,
  get batchTask() {
    if (!this._batchTask) {
      let task = function taskCallback() {
        LOG("batchTask: Invalidating engine cache");
        this._buildCache();
      }.bind(this);
      this._batchTask = new DeferredTask(task, CACHE_INVALIDATION_DELAY);
    }
    return this._batchTask;
  },

  _addEngineToStore: function(aEngine) {
    LOG("_addEngineToStore: Adding engine: \"" + aEngine.name + "\"");

    // See if there is an existing engine with the same name. However, if this
    // engine is updating another engine, it's allowed to have the same name.
    var hasSameNameAsUpdate = (aEngine._engineToUpdate &&
                               aEngine.name == aEngine._engineToUpdate.name);
    if (aEngine.name in this._engines && !hasSameNameAsUpdate) {
      LOG("_addEngineToStore: Duplicate engine found, aborting!");
      return;
    }

    if (aEngine._engineToUpdate) {
      // We need to replace engineToUpdate with the engine that just loaded.
      var oldEngine = aEngine._engineToUpdate;

      // Remove the old engine from the hash, since it's keyed by name, and our
      // name might change (the update might have a new name).
      delete this._engines[oldEngine.name];

      // Hack: we want to replace the old engine with the new one, but since
      // people may be holding refs to the nsISearchEngine objects themselves,
      // we'll just copy over all "private" properties (those without a getter
      // or setter) from one object to the other.
      for (var p in aEngine) {
        if (!(aEngine.__lookupGetter__(p) || aEngine.__lookupSetter__(p)))
          oldEngine[p] = aEngine[p];
      }
      aEngine = oldEngine;
      aEngine._engineToUpdate = null;

      // Add the engine back
      this._engines[aEngine.name] = aEngine;
      notifyAction(aEngine, SEARCH_ENGINE_CHANGED);
    } else {
      // Not an update, just add the new engine.
      this._engines[aEngine.name] = aEngine;
      // Only add the engine to the list of sorted engines if the initial list
      // has already been built (i.e. if this.__sortedEngines is non-null). If
      // it hasn't, we're loading engines from disk and the sorted engine list
      // will be built once we need it.
      if (this.__sortedEngines) {
        this.__sortedEngines.push(aEngine);
        this._saveSortedEngineList();
      }
      notifyAction(aEngine, SEARCH_ENGINE_ADDED);
    }

    if (aEngine._hasUpdates) {
      // Schedule the engine's next update, if it isn't already.
      if (!aEngine.getAttr("updateexpir"))
        engineUpdateService.scheduleNextUpdate(aEngine);
    }
  },

  _loadEnginesMetadataFromCache: function(cache) {
    if (cache._oldMetadata) {
      // If we have old metadata in the cache, we had no valid cache
      // file and read data from search-metadata.json.
      for (let name in this._engines) {
        let engine = this._engines[name];
        if (engine._id && cache._oldMetadata[engine._id])
          engine._metaData = cache._oldMetadata[engine._id];
      }
      return;
    }

    if (!cache.engines)
      return;

    for (let engine of cache.engines) {
      let name = engine._name;
      if (name in this._engines) {
        LOG("_loadEnginesMetadataFromCache, transfering metadata for " + name);
        this._engines[name]._metaData = engine._metaData;
      }
    }
  },

  _loadEnginesFromCache: function(cache,
                                                                 skipReadOnly) {
    if (!cache.engines)
      return;

    LOG("_loadEnginesFromCache: Loading " +
        cache.engines.length + " engines from cache");

    let skippedEngines = 0;
    for (let engine of cache.engines) {
      if (skipReadOnly && engine._readOnly == undefined) {
        ++skippedEngines;
        continue;
      }

      this._loadEngineFromCache(engine);
    }

    if (skippedEngines) {
      LOG("_loadEnginesFromCache: skipped " + skippedEngines + " read-only engines.");
    }
  },

  _loadEngineFromCache: function(json) {
    try {
      let engine = new Engine(json._shortName, json._readOnly == undefined);
      engine._initWithJSON(json);
      this._addEngineToStore(engine);
    } catch (ex) {
      LOG("Failed to load " + json._name + " from cache: " + ex);
      LOG("Engine JSON: " + json.toSource());
    }
  },

  _loadEnginesFromDir: function(aDir) {
    LOG("_loadEnginesFromDir: Searching in " + aDir.path + " for search engines.");

    // Check whether aDir is the user profile dir
    var isInProfile = aDir.equals(getDir(NS_APP_USER_SEARCH_DIR));

    var files = aDir.directoryEntries
                    .QueryInterface(Ci.nsIDirectoryEnumerator);

    while (files.hasMoreElements()) {
      var file = files.nextFile;

      // Ignore hidden and empty files, and directories
      if (!file.isFile() || file.fileSize == 0 || file.isHidden())
        continue;

      var fileURL = NetUtil.ioService.newFileURI(file).QueryInterface(Ci.nsIURL);
      var fileExtension = fileURL.fileExtension.toLowerCase();

      if (fileExtension != "xml") {
        // Not an engine
        continue;
      }

      var addedEngine = null;
      try {
        addedEngine = new Engine(file, !isInProfile);
        addedEngine._initFromFile(file);
        if (!isInProfile && !addedEngine._isDefault) {
          addedEngine._dirPath = aDir.path;
          addedEngine._dirLastModifiedTime = aDir.lastModifiedTime;
        }
      } catch (ex) {
        LOG("_loadEnginesFromDir: Failed to load " + file.path + "!\n" + ex);
        continue;
      }

      this._addEngineToStore(addedEngine);
    }
  },

  /**
   * Loads engines from a given directory asynchronously.
   *
   * @param aDir the directory.
   *
   * @returns {Promise} A promise, resolved successfully if retrieveing data
   * succeeds.
   */
  _asyncLoadEnginesFromDir: Task.async(function* (aDir) {
    LOG("_asyncLoadEnginesFromDir: Searching in " + aDir.path + " for search engines.");

    // Check whether aDir is the user profile dir
    let isInProfile = aDir.equals(getDir(NS_APP_USER_SEARCH_DIR));
    let dirPath = aDir.path;
    let iterator = new OS.File.DirectoryIterator(dirPath);

    let osfiles = yield iterator.nextBatch();
    iterator.close();

    let engines = [];
    for (let osfile of osfiles) {
      if (osfile.isDir || osfile.isSymLink)
        continue;

      let fileInfo = yield OS.File.stat(osfile.path);
      if (fileInfo.size == 0)
        continue;

      let parts = osfile.path.split(".");
      if (parts.length <= 1 || (parts.pop()).toLowerCase() != "xml") {
        // Not an engine
        continue;
      }

      let addedEngine = null;
      try {
        let file = new FileUtils.File(osfile.path);
        addedEngine = new Engine(file, !isInProfile);
        yield checkForSyncCompletion(addedEngine._asyncInitFromFile(file));
        if (!isInProfile && !addedEngine._isDefault) {
          addedEngine._dirPath = dirPath;
          let info = yield OS.File.stat(dirPath);
          addedEngine._dirLastModifiedTime =
            info.lastModificationDate.getTime();
        }
        engines.push(addedEngine);
      } catch (ex) {
        if (ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {
          throw ex;
        }
        LOG("_asyncLoadEnginesFromDir: Failed to load " + osfile.path + "!\n" + ex);
      }
    }
    return engines;
  }),

  _loadFromChromeURLs: function(aURLs) {
    aURLs.forEach(function (url) {
      try {
        LOG("_loadFromChromeURLs: loading engine from chrome url: " + url);

        let uri = makeURI(url);
        let engine = new Engine(uri, true);

        engine._initFromURISync(uri);

        this._addEngineToStore(engine);
      } catch (ex) {
        LOG("_loadFromChromeURLs: failed to load engine: " + ex);
      }
    }, this);
  },

  /**
   * Loads engines from Chrome URLs asynchronously.
   *
   * @param aURLs a list of URLs.
   *
   * @returns {Promise} A promise, resolved successfully if loading data
   * succeeds.
   */
  _asyncLoadFromChromeURLs: Task.async(function* (aURLs) {
    let engines = [];
    for (let url of aURLs) {
      try {
        LOG("_asyncLoadFromChromeURLs: loading engine from chrome url: " + url);
        let uri = NetUtil.newURI(url);
        let engine = new Engine(uri, true);
        yield checkForSyncCompletion(engine._asyncInitFromURI(uri));
        engines.push(engine);
      } catch (ex) {
        if (ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {
          throw ex;
        }
        LOG("_asyncLoadFromChromeURLs: failed to load engine: " + ex);
      }
    }
    return engines;
  }),

  _convertChannelToFile: function(chan) {
    let fileURI = chan.URI;
    while (fileURI instanceof Ci.nsIJARURI)
      fileURI = fileURI.JARFile;
    fileURI.QueryInterface(Ci.nsIFileURL);

    return fileURI.file;
  },

  _findJAREngines: function() {
    LOG("_findJAREngines: looking for engines in JARs")

    let chan = makeChannel(APP_SEARCH_PREFIX + "list.json");
    if (!chan) {
      LOG("_findJAREngines: " + APP_SEARCH_PREFIX + " isn't registered");
      return [];
    }

    let uris = [];

    let sis = Cc["@mozilla.org/scriptableinputstream;1"].
                createInstance(Ci.nsIScriptableInputStream);
    try {
      sis.init(chan.open2());
      this._parseListJSON(sis.read(sis.available()), uris);
      // parseListJSON will catch its own errors, so we
      // should only go into this catch if list.json
      // doesn't exist
    } catch (e) {
      chan = makeChannel(APP_SEARCH_PREFIX + "list.txt");
      sis.init(chan.open2());
      this._parseListTxt(sis.read(sis.available()), uris);
    }
    return uris;
  },

  /**
   * Loads jar engines asynchronously.
   *
   * @returns {Promise} A promise, resolved successfully if finding jar engines
   * succeeds.
   */
  _asyncFindJAREngines: Task.async(function* () {
    LOG("_asyncFindJAREngines: looking for engines in JARs")

    let listURL = APP_SEARCH_PREFIX + "list.json";
    let chan = makeChannel(listURL);
    if (!chan) {
      LOG("_asyncFindJAREngines: " + APP_SEARCH_PREFIX + " isn't registered");
      return [];
    }

    let uris = [];

    // Read list.json to find the engines we need to load.
    let deferred = Promise.defer();
    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                    createInstance(Ci.nsIXMLHttpRequest);
    request.overrideMimeType("text/plain");
    request.onload = function(aEvent) {
      deferred.resolve(aEvent.target.responseText);
    };
    request.onerror = function(aEvent) {
      LOG("_asyncFindJAREngines: failed to read " + listURL);
      // Couldn't find list.json, try list.txt
      request.onerror = function(aEvent) {
        LOG("_asyncFindJAREngines: failed to read " + APP_SEARCH_PREFIX + "list.txt");
        deferred.resolve("");
      }
      request.open("GET", NetUtil.newURI(APP_SEARCH_PREFIX + "list.txt").spec, true);
      request.send();
    };
    request.open("GET", NetUtil.newURI(listURL).spec, true);
    request.send();
    let list = yield deferred.promise;

    if (request.responseURL.endsWith(".txt")) {
      this._parseListTxt(list, uris);
    } else {
      this._parseListJSON(list, uris);
    }
    return uris;
  }),

  _parseListJSON: function(list, uris) {
    let searchSettings;
    try {
      searchSettings = JSON.parse(list);
    } catch (e) {
      LOG("failing to parse list.json: " + e);
      return;
    }

    let jarNames = new Set();
    for (let region in searchSettings) {
      // Artifact builds use the full list.json which parses
      // slightly differently
      if (!("visibleDefaultEngines" in searchSettings[region])) {
        continue;
      }
      for (let engine of searchSettings[region]["visibleDefaultEngines"]) {
        jarNames.add(engine);
      }
    }

    // Check if we have a useable country specific list of visible default engines.
    let engineNames;
    let visibleDefaultEngines = this.getVerifiedGlobalAttr("visibleDefaultEngines");
    if (visibleDefaultEngines) {
      engineNames = visibleDefaultEngines.split(",");
      for (let engineName of engineNames) {
        // If all engineName values are part of jarNames,
        // then we can use the country specific list, otherwise ignore it.
        // The visibleDefaultEngines string containing the name of an engine we
        // don't ship indicates the server is misconfigured to answer requests
        // from the specific Firefox version we are running, so ignoring the
        // value altogether is safer.
        if (!jarNames.has(engineName)) {
          LOG("_parseListJSON: ignoring visibleDefaultEngines value because " +
              engineName + " is not in the jar engines we have found");
          engineNames = null;
          break;
        }
      }
    }

    // Fallback to building a list based on the regions in the JSON
    if (!engineNames || !engineNames.length) {
      engineNames = searchSettings["default"]["visibleDefaultEngines"];
    }

    for (let name of engineNames) {
      uris.push(APP_SEARCH_PREFIX + name + ".xml");
    }

    // Store this so that it can be used while writing the cache file.
    this._visibleDefaultEngines = engineNames;
  },

  _parseListTxt: function(list, uris) {
    let names = list.split("\n").filter(n => !!n);
    // This maps the names of our built-in engines to a boolean
    // indicating whether it should be hidden by default.
    let jarNames = new Map();
    for (let name of names) {
      if (name.endsWith(":hidden")) {
        name = name.split(":")[0];
        jarNames.set(name, true);
      } else {
        jarNames.set(name, false);
      }
    }

    // Check if we have a useable country specific list of visible default engines.
    let engineNames;
    let visibleDefaultEngines = this.getVerifiedGlobalAttr("visibleDefaultEngines");
    if (visibleDefaultEngines) {
      engineNames = visibleDefaultEngines.split(",");

      for (let engineName of engineNames) {
        // If all engineName values are part of jarNames,
        // then we can use the country specific list, otherwise ignore it.
        // The visibleDefaultEngines string containing the name of an engine we
        // don't ship indicates the server is misconfigured to answer requests
        // from the specific Firefox version we are running, so ignoring the
        // value altogether is safer.
        if (!jarNames.has(engineName)) {
          LOG("_parseListTxt: ignoring visibleDefaultEngines value because " +
              engineName + " is not in the jar engines we have found");
          engineNames = null;
          break;
        }
      }
    }

    // Fallback to building a list based on the :hidden suffixes found in list.txt.
    if (!engineNames) {
      engineNames = [];
      for (let [name, hidden] of jarNames) {
        if (!hidden)
          engineNames.push(name);
      }
    }

    for (let name of engineNames) {
      uris.push(APP_SEARCH_PREFIX + name + ".xml");
    }

    // Store this so that it can be used while writing the cache file.
    this._visibleDefaultEngines = engineNames;
  },


  _saveSortedEngineList: function() {
    LOG("SRCH_SVC_saveSortedEngineList: starting");

    // Set the useDB pref to indicate that from now on we should use the order
    // information stored in the database.
    Services.prefs.setBoolPref(BROWSER_SEARCH_PREF + "useDBForOrder", true);

    var engines = this._getSortedEngines(true);

    for (var i = 0; i < engines.length; ++i) {
      engines[i].setAttr("order", i + 1);
    }

    LOG("SRCH_SVC_saveSortedEngineList: done");
  },

  _buildSortedEngineList: function() {
    LOG("_buildSortedEngineList: building list");
    var addedEngines = { };
    this.__sortedEngines = [];
    var engine;

    // If the user has specified a custom engine order, read the order
    // information from the metadata instead of the default prefs.
    if (Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "useDBForOrder", false)) {
      LOG("_buildSortedEngineList: using db for order");

      // Flag to keep track of whether or not we need to call _saveSortedEngineList.
      let needToSaveEngineList = false;

      for (let name in this._engines) {
        let engine = this._engines[name];
        var orderNumber = engine.getAttr("order");

        // Since the DB isn't regularly cleared, and engine files may disappear
        // without us knowing, we may already have an engine in this slot. If
        // that happens, we just skip it - it will be added later on as an
        // unsorted engine.
        if (orderNumber && !this.__sortedEngines[orderNumber-1]) {
          this.__sortedEngines[orderNumber-1] = engine;
          addedEngines[engine.name] = engine;
        } else {
          // We need to call _saveSortedEngineList so this gets sorted out.
          needToSaveEngineList = true;
        }
      }

      // Filter out any nulls for engines that may have been removed
      var filteredEngines = this.__sortedEngines.filter(function(a) { return !!a; });
      if (this.__sortedEngines.length != filteredEngines.length)
        needToSaveEngineList = true;
      this.__sortedEngines = filteredEngines;

      if (needToSaveEngineList)
        this._saveSortedEngineList();
    } else {
      // The DB isn't being used, so just read the engine order from the prefs
      var i = 0;
      var engineName;
      var prefName;

      try {
        var extras =
          Services.prefs.getChildList(BROWSER_SEARCH_PREF + "order.extra.");

        for (prefName of extras) {
          engineName = Services.prefs.getCharPref(prefName);

          engine = this._engines[engineName];
          if (!engine || engine.name in addedEngines)
            continue;

          this.__sortedEngines.push(engine);
          addedEngines[engine.name] = engine;
        }
      }
      catch (e) { }

      while (true) {
        engineName = getLocalizedPref(BROWSER_SEARCH_PREF + "order." + (++i));
        if (!engineName)
          break;

        engine = this._engines[engineName];
        if (!engine || engine.name in addedEngines)
          continue;

        this.__sortedEngines.push(engine);
        addedEngines[engine.name] = engine;
      }
    }

    // Array for the remaining engines, alphabetically sorted.
    let alphaEngines = [];

    for (let name in this._engines) {
      let engine = this._engines[name];
      if (!(engine.name in addedEngines))
        alphaEngines.push(this._engines[engine.name]);
    }

    let locale = Cc["@mozilla.org/intl/nslocaleservice;1"]
                   .getService(Ci.nsILocaleService)
                   .newLocale(getLocale());
    let collation = Cc["@mozilla.org/intl/collation-factory;1"]
                      .createInstance(Ci.nsICollationFactory)
                      .CreateCollation(locale);
    const strength = Ci.nsICollation.kCollationCaseInsensitiveAscii;
    let comparator = (a, b) => collation.compareString(strength, a.name, b.name);
    alphaEngines.sort(comparator);
    return this.__sortedEngines = this.__sortedEngines.concat(alphaEngines);
  },

  /**
   * Get a sorted array of engines.
   * @param aWithHidden
   *        True if hidden plugins should be included in the result.
   */
  _getSortedEngines: function(aWithHidden) {
    if (aWithHidden)
      return this._sortedEngines;

    return this._sortedEngines.filter(function (engine) {
                                        return !engine.hidden;
                                      });
  },

  // nsIBrowserSearchService
  init: function(observer) {
    LOG("SearchService.init");
    let self = this;
    if (!this._initStarted) {
      this._initStarted = true;
      Task.spawn(function* task() {
        try {
          // Complete initialization by calling asynchronous initializer.
          yield self._asyncInit();
        } catch (ex) {
          if (ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {
            // No need to pursue asynchronous because synchronous fallback was
            // called and has finished.
          } else {
            self._initObservers.reject(ex);
          }
        }
      });
    }
    if (observer) {
      this._initObservers.promise.then(
        function onSuccess() {
          try {
            observer.onInitComplete(self._initRV);
          } catch (e) {
            Cu.reportError(e);
          }
        },
        function onError(aReason) {
          Cu.reportError("Internal error while initializing SearchService: " + aReason);
          observer.onInitComplete(Components.results.NS_ERROR_UNEXPECTED);
        }
      );
    }
  },

  get isInitialized() {
    return gInitialized;
  },

  getEngines: function(aCount) {
    this._ensureInitialized();
    LOG("getEngines: getting all engines");
    var engines = this._getSortedEngines(true);
    aCount.value = engines.length;
    return engines;
  },

  getVisibleEngines: function(aCount) {
    this._ensureInitialized();
    LOG("getVisibleEngines: getting all visible engines");
    var engines = this._getSortedEngines(false);
    aCount.value = engines.length;
    return engines;
  },

  getDefaultEngines: function(aCount) {
    this._ensureInitialized();
    function isDefault(engine) {
      return engine._isDefault;
    }
    var engines = this._sortedEngines.filter(isDefault);
    var engineOrder = {};
    var engineName;
    var i = 1;

    // Build a list of engines which we have ordering information for.
    // We're rebuilding the list here because _sortedEngines contain the
    // current order, but we want the original order.

    // First, look at the "browser.search.order.extra" branch.
    try {
      var extras = Services.prefs.getChildList(BROWSER_SEARCH_PREF + "order.extra.");

      for (var prefName of extras) {
        engineName = Services.prefs.getCharPref(prefName);

        if (!(engineName in engineOrder))
          engineOrder[engineName] = i++;
      }
    } catch (e) {
      LOG("Getting extra order prefs failed: " + e);
    }

    // Now look through the "browser.search.order" branch.
    for (var j = 1; ; j++) {
      engineName = getLocalizedPref(BROWSER_SEARCH_PREF + "order." + j);
      if (!engineName)
        break;

      if (!(engineName in engineOrder))
        engineOrder[engineName] = i++;
    }

    LOG("getDefaultEngines: engineOrder: " + engineOrder.toSource());

    function compareEngines (a, b) {
      var aIdx = engineOrder[a.name];
      var bIdx = engineOrder[b.name];

      if (aIdx && bIdx)
        return aIdx - bIdx;
      if (aIdx)
        return -1;
      if (bIdx)
        return 1;

      return a.name.localeCompare(b.name);
    }
    engines.sort(compareEngines);

    aCount.value = engines.length;
    return engines;
  },

  getEngineByName: function(aEngineName) {
    this._ensureInitialized();
    return this._engines[aEngineName] || null;
  },

  getEngineByAlias: function(aAlias) {
    this._ensureInitialized();
    for (var engineName in this._engines) {
      var engine = this._engines[engineName];
      if (engine && engine.alias == aAlias)
        return engine;
    }
    return null;
  },

  addEngineWithDetails: function(aName, aIconURL, aAlias,
                                                 aDescription, aMethod,
                                                 aTemplate, aExtensionID) {
    this._ensureInitialized();
    if (!aName)
      FAIL("Invalid name passed to addEngineWithDetails!");
    if (!aMethod)
      FAIL("Invalid method passed to addEngineWithDetails!");
    if (!aTemplate)
      FAIL("Invalid template passed to addEngineWithDetails!");
    if (this._engines[aName])
      FAIL("An engine with that name already exists!", Cr.NS_ERROR_FILE_ALREADY_EXISTS);

    var engine = new Engine(sanitizeName(aName), false);
    engine._initFromMetadata(aName, aIconURL, aAlias, aDescription,
                             aMethod, aTemplate, aExtensionID);
    engine._loadPath = "[other]addEngineWithDetails";
    this._addEngineToStore(engine);
  },

  addEngine: function(aEngineURL, aDataType, aIconURL,
                                         aConfirm, aCallback) {
    LOG("addEngine: Adding \"" + aEngineURL + "\".");
    this._ensureInitialized();
    try {
      var uri = makeURI(aEngineURL);
      var engine = new Engine(uri, false);
      if (aCallback) {
        engine._installCallback = function (errorCode) {
          try {
            if (errorCode == null)
              aCallback.onSuccess(engine);
            else
              aCallback.onError(errorCode);
          } catch (ex) {
            Cu.reportError("Error invoking addEngine install callback: " + ex);
          }
          // Clear the reference to the callback now that it's been invoked.
          engine._installCallback = null;
        };
      }
      engine._initFromURIAndLoad(uri);
    } catch (ex) {
      // Drop the reference to the callback, if set
      if (engine)
        engine._installCallback = null;
      FAIL("addEngine: Error adding engine:\n" + ex, Cr.NS_ERROR_FAILURE);
    }
    engine._setIcon(aIconURL, false);
    engine._confirm = aConfirm;
  },

  removeEngine: function(aEngine) {
    this._ensureInitialized();
    if (!aEngine)
      FAIL("no engine passed to removeEngine!");

    var engineToRemove = null;
    for (var e in this._engines) {
      if (aEngine.wrappedJSObject == this._engines[e])
        engineToRemove = this._engines[e];
    }

    if (!engineToRemove)
      FAIL("removeEngine: Can't find engine to remove!", Cr.NS_ERROR_FILE_NOT_FOUND);

    if (engineToRemove == this.currentEngine) {
      this._currentEngine = null;
    }

    if (engineToRemove._readOnly) {
      // Just hide it (the "hidden" setter will notify) and remove its alias to
      // avoid future conflicts with other engines.
      engineToRemove.hidden = true;
      engineToRemove.alias = null;
    } else {
      // Remove the engine file from disk if we had a legacy file in the profile.
      if (engineToRemove._filePath) {
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
        file.persistentDescriptor = engineToRemove._filePath;
        if (file.exists()) {
          file.remove(false);
        }
        engineToRemove._filePath = null;
      }

      // Remove the engine from _sortedEngines
      var index = this._sortedEngines.indexOf(engineToRemove);
      if (index == -1)
        FAIL("Can't find engine to remove in _sortedEngines!", Cr.NS_ERROR_FAILURE);
      this.__sortedEngines.splice(index, 1);

      // Remove the engine from the internal store
      delete this._engines[engineToRemove.name];

      // Since we removed an engine, we need to update the preferences.
      this._saveSortedEngineList();
    }
    notifyAction(engineToRemove, SEARCH_ENGINE_REMOVED);
  },

  moveEngine: function(aEngine, aNewIndex) {
    this._ensureInitialized();
    if ((aNewIndex > this._sortedEngines.length) || (aNewIndex < 0))
      FAIL("SRCH_SVC_moveEngine: Index out of bounds!");
    if (!(aEngine instanceof Ci.nsISearchEngine))
      FAIL("SRCH_SVC_moveEngine: Invalid engine passed to moveEngine!");
    if (aEngine.hidden)
      FAIL("moveEngine: Can't move a hidden engine!", Cr.NS_ERROR_FAILURE);

    var engine = aEngine.wrappedJSObject;

    var currentIndex = this._sortedEngines.indexOf(engine);
    if (currentIndex == -1)
      FAIL("moveEngine: Can't find engine to move!", Cr.NS_ERROR_UNEXPECTED);

    // Our callers only take into account non-hidden engines when calculating
    // aNewIndex, but we need to move it in the array of all engines, so we
    // need to adjust aNewIndex accordingly. To do this, we count the number
    // of hidden engines in the list before the engine that we're taking the
    // place of. We do this by first finding newIndexEngine (the engine that
    // we were supposed to replace) and then iterating through the complete
    // engine list until we reach it, increasing aNewIndex for each hidden
    // engine we find on our way there.
    //
    // This could be further simplified by having our caller pass in
    // newIndexEngine directly instead of aNewIndex.
    var newIndexEngine = this._getSortedEngines(false)[aNewIndex];
    if (!newIndexEngine)
      FAIL("moveEngine: Can't find engine to replace!", Cr.NS_ERROR_UNEXPECTED);

    for (var i = 0; i < this._sortedEngines.length; ++i) {
      if (newIndexEngine == this._sortedEngines[i])
        break;
      if (this._sortedEngines[i].hidden)
        aNewIndex++;
    }

    if (currentIndex == aNewIndex)
      return; // nothing to do!

    // Move the engine
    var movedEngine = this.__sortedEngines.splice(currentIndex, 1)[0];
    this.__sortedEngines.splice(aNewIndex, 0, movedEngine);

    notifyAction(engine, SEARCH_ENGINE_CHANGED);

    // Since we moved an engine, we need to update the preferences.
    this._saveSortedEngineList();
  },

  restoreDefaultEngines: function() {
    this._ensureInitialized();
    for (let name in this._engines) {
      let e = this._engines[name];
      // Unhide all default engines
      if (e.hidden && e._isDefault)
        e.hidden = false;
    }
  },

  get defaultEngine() { return this.currentEngine; },

  set defaultEngine(val) {
    this.currentEngine = val;
  },

  get currentEngine() {
    this._ensureInitialized();
    if (!this._currentEngine) {
      let name = this.getGlobalAttr("current");
      let engine = this.getEngineByName(name);
      if (engine && (this.getGlobalAttr("hash") == getVerificationHash(name) ||
                     engine._isDefault)) {
        // If the current engine is a default one, we can relax the
        // verification hash check to reduce the annoyance for users who
        // backup/sync their profile in custom ways.
        this._currentEngine = engine;
      }
      if (!name)
        this._currentEngine = this.originalDefaultEngine;
    }

    // If the current engine is not set or hidden, we fallback...
    if (!this._currentEngine || this._currentEngine.hidden) {
      // first to the original default engine
      let originalDefault = this.originalDefaultEngine;
      if (!originalDefault || originalDefault.hidden) {
        // then to the first visible engine
        let firstVisible = this._getSortedEngines(false)[0];
        if (firstVisible && !firstVisible.hidden) {
          this.currentEngine = firstVisible;
          return firstVisible;
        }
        // and finally as a last resort we unhide the original default engine.
        if (originalDefault)
          originalDefault.hidden = false;
      }
      if (!originalDefault)
        return null;

      // If the current engine wasn't set or was hidden, we used a fallback
      // to pick a new current engine. As soon as we return it, this new
      // current engine will become user-visible, so we should persist it.
      // by calling the setter.
      this.currentEngine = originalDefault;
    }

    return this._currentEngine;
  },

  set currentEngine(val) {
    this._ensureInitialized();
    // Sometimes we get wrapped nsISearchEngine objects (external XPCOM callers),
    // and sometimes we get raw Engine JS objects (callers in this file), so
    // handle both.
    if (!(val instanceof Ci.nsISearchEngine) && !(val instanceof Engine))
      FAIL("Invalid argument passed to currentEngine setter");

    var newCurrentEngine = this.getEngineByName(val.name);
    if (!newCurrentEngine)
      FAIL("Can't find engine in store!", Cr.NS_ERROR_UNEXPECTED);

    if (!newCurrentEngine._isDefault) {
      // If a non default engine is being set as the current engine, ensure
      // its loadPath has a verification hash.
      if (!newCurrentEngine._loadPath)
        newCurrentEngine._loadPath = "[other]unknown";
      let loadPathHash = getVerificationHash(newCurrentEngine._loadPath);
      let currentHash = newCurrentEngine.getAttr("loadPathHash");
      if (!currentHash || currentHash != loadPathHash) {
        newCurrentEngine.setAttr("loadPathHash", loadPathHash);
        notifyAction(newCurrentEngine, SEARCH_ENGINE_CHANGED);
      }
    }

    if (newCurrentEngine == this._currentEngine)
      return;

    this._currentEngine = newCurrentEngine;

    // If we change the default engine in the future, that change should impact
    // users who have switched away from and then back to the build's "default"
    // engine. So clear the user pref when the currentEngine is set to the
    // build's default engine, so that the currentEngine getter falls back to
    // whatever the default is.
    let newName = this._currentEngine.name;
    if (this._currentEngine == this.originalDefaultEngine) {
      newName = "";
    }

    this.setGlobalAttr("current", newName);
    this.setGlobalAttr("hash", getVerificationHash(newName));

    notifyAction(this._currentEngine, SEARCH_ENGINE_DEFAULT);
    notifyAction(this._currentEngine, SEARCH_ENGINE_CURRENT);
  },

  getDefaultEngineInfo() {
    let result = {};

    let engine;
    try {
      engine = this.defaultEngine;
    } catch (e) {
      // The defaultEngine getter will throw if there's no engine at all,
      // which shouldn't happen unless an add-on or a test deleted all of them.
      // Our preferences UI doesn't let users do that.
      Cu.reportError("getDefaultEngineInfo: No default engine");
    }

    if (!engine) {
      result.name = "NONE";
    } else {
      if (engine.name)
        result.name = engine.name;

      result.loadPath = engine._loadPath;

      let origin;
      if (engine._isDefault)
        origin = "default";
      else {
        let currentHash = engine.getAttr("loadPathHash");
        if (!currentHash)
          origin = "unverified";
        else {
          let loadPathHash = getVerificationHash(engine._loadPath);
          origin = currentHash == loadPathHash ? "verified" : "invalid";
        }
      }
      result.origin = origin;

      // For privacy, we only collect the submission URL for default engines...
      let sendSubmissionURL = engine._isDefault;

      // ... or engines sorted by default near the top of the list.
      if (!sendSubmissionURL) {
        let extras =
          Services.prefs.getChildList(BROWSER_SEARCH_PREF + "order.extra.");

        for (let prefName of extras) {
          try {
            if (result.name == Services.prefs.getCharPref(prefName)) {
              sendSubmissionURL = true;
              break;
            }
          } catch (e) {}
        }

        let i = 0;
        while (!sendSubmissionURL) {
          let engineName = getLocalizedPref(BROWSER_SEARCH_PREF + "order." + (++i));
          if (!engineName)
            break;
          if (result.name == engineName) {
            sendSubmissionURL = true;
            break;
          }
        }
      }

      if (sendSubmissionURL) {
        let uri = engine._getURLOfType("text/html")
                        .getSubmission("", engine, "searchbar").uri;
        uri.userPass = ""; // Avoid reporting a username or password.
        result.submissionURL = uri.spec;
      }
    }

    return result;
  },

  _recordEngineTelemetry: function() {
    Services.telemetry.getHistogramById("SEARCH_SERVICE_ENGINE_COUNT")
            .add(Object.keys(this._engines).length);
    let hasUpdates = false;
    let hasIconUpdates = false;
    for (let name in this._engines) {
      let engine = this._engines[name];
      if (engine._hasUpdates) {
        hasUpdates = true;
        if (engine._iconUpdateURL) {
          hasIconUpdates = true;
          break;
        }
      }
    }
    Services.telemetry.getHistogramById("SEARCH_SERVICE_HAS_UPDATES").add(hasUpdates);
    Services.telemetry.getHistogramById("SEARCH_SERVICE_HAS_ICON_UPDATES").add(hasIconUpdates);
  },

  /**
   * This map is built lazily after the available search engines change.  It
   * allows quick parsing of an URL representing a search submission into the
   * search engine name and original terms.
   *
   * The keys are strings containing the domain name and lowercase path of the
   * engine submission, for example "www.google.com/search".
   *
   * The values are objects with these properties:
   * {
   *   engine: The associated nsISearchEngine.
   *   termsParameterName: Name of the URL parameter containing the search
   *                       terms, for example "q".
   * }
   */
  _parseSubmissionMap: null,

  _buildParseSubmissionMap: function() {
    LOG("_buildParseSubmissionMap");
    this._parseSubmissionMap = new Map();

    // Used only while building the map, indicates which entries do not refer to
    // the main domain of the engine but to an alternate domain, for example
    // "www.google.fr" for the "www.google.com" search engine.
    let keysOfAlternates = new Set();

    for (let engine of this._sortedEngines) {
      LOG("Processing engine: " + engine.name);

      if (engine.hidden) {
        LOG("Engine is hidden.");
        continue;
      }

      let urlParsingInfo = engine.getURLParsingInfo();
      if (!urlParsingInfo) {
        LOG("Engine does not support URL parsing.");
        continue;
      }

      // Store the same object on each matching map key, as an optimization.
      let mapValueForEngine = {
        engine: engine,
        termsParameterName: urlParsingInfo.termsParameterName,
      };

      let processDomain = (domain, isAlternate) => {
        let key = domain + urlParsingInfo.path;

        // Apply the logic for which main domains take priority over alternate
        // domains, even if they are found later in the ordered engine list.
        let existingEntry = this._parseSubmissionMap.get(key);
        if (!existingEntry) {
          LOG("Adding new entry: " + key);
          if (isAlternate) {
            keysOfAlternates.add(key);
          }
        } else if (!isAlternate && keysOfAlternates.has(key)) {
          LOG("Overriding alternate entry: " + key +
              " (" + existingEntry.engine.name + ")");
          keysOfAlternates.delete(key);
        } else {
          LOG("Keeping existing entry: " + key +
              " (" + existingEntry.engine.name + ")");
          return;
        }

        this._parseSubmissionMap.set(key, mapValueForEngine);
      };

      processDomain(urlParsingInfo.mainDomain, false);
      SearchStaticData.getAlternateDomains(urlParsingInfo.mainDomain)
                      .forEach(d => processDomain(d, true));
    }
  },

  /**
   * Checks to see if any engine has an EngineURL of type URLTYPE_SEARCH_HTML
   * for this request-method, template URL, and query params.
   */
  hasEngineWithURL: function(method, template, formData) {
    this._ensureInitialized();

    // Quick helper method to ensure formData filtered/sorted for compares.
    let getSortedFormData = data => {
      return data.filter(a => a.name && a.value).sort((a, b) => {
        if (a.name > b.name) {
          return 1;
        } else if (b.name > a.name) {
          return -1;
        } else if (a.value > b.value) {
          return 1;
        }
        return (b.value > a.value) ? -1 : 0;
      });
    };

    // Sanitize method, ensure formData is pre-sorted.
    let methodUpper = method.toUpperCase();
    let sortedFormData = getSortedFormData(formData);
    let sortedFormLength = sortedFormData.length;

    return this._getSortedEngines(false).some(engine => {
      return engine._urls.some(url => {
        // Not an engineURL match if type, method, url, #params don't match.
        if (url.type != URLTYPE_SEARCH_HTML ||
            url.method != methodUpper ||
            url.template != template ||
            url.params.length != sortedFormLength) {
          return false;
        }

        // Ensure engineURL formData is pre-sorted. Then, we're
        // not an engineURL match if any queryParam doesn't compare.
        let sortedParams = getSortedFormData(url.params);
        for (let i = 0; i < sortedFormLength; i++) {
          let formData = sortedFormData[i];
          let param = sortedParams[i];
          if (param.name != formData.name ||
              param.value != formData.value ||
              param.purpose != formData.purpose) {
            return false;
          }
        }
        // Else we're a match.
        return true;
      });
    });
  },

  parseSubmissionURL: function(aURL) {
    this._ensureInitialized();
    LOG("parseSubmissionURL: Parsing \"" + aURL + "\".");

    if (!this._parseSubmissionMap) {
      this._buildParseSubmissionMap();
    }

    // Extract the elements of the provided URL first.
    let soughtKey, soughtQuery;
    try {
      let soughtUrl = NetUtil.newURI(aURL).QueryInterface(Ci.nsIURL);

      // Exclude any URL that is not HTTP or HTTPS from the beginning.
      if (soughtUrl.scheme != "http" && soughtUrl.scheme != "https") {
        LOG("The URL scheme is not HTTP or HTTPS.");
        return gEmptyParseSubmissionResult;
      }

      // Reading these URL properties may fail and raise an exception.
      soughtKey = soughtUrl.host + soughtUrl.filePath.toLowerCase();
      soughtQuery = soughtUrl.query;
    } catch (ex) {
      // Errors while parsing the URL or accessing the properties are not fatal.
      LOG("The value does not look like a structured URL.");
      return gEmptyParseSubmissionResult;
    }

    // Look up the domain and path in the map to identify the search engine.
    let mapEntry = this._parseSubmissionMap.get(soughtKey);
    if (!mapEntry) {
      LOG("No engine associated with domain and path: " + soughtKey);
      return gEmptyParseSubmissionResult;
    }

    // Extract the search terms from the parameter, for example "caff%C3%A8"
    // from the URL "https://www.google.com/search?q=caff%C3%A8&client=firefox".
    let encodedTerms = null;
    for (let param of soughtQuery.split("&")) {
      let equalPos = param.indexOf("=");
      if (equalPos != -1 &&
          param.substr(0, equalPos) == mapEntry.termsParameterName) {
        // This is the parameter we are looking for.
        encodedTerms = param.substr(equalPos + 1);
        break;
      }
    }
    if (encodedTerms === null) {
      LOG("Missing terms parameter: " + mapEntry.termsParameterName);
      return gEmptyParseSubmissionResult;
    }

    let length = 0;
    let offset = aURL.indexOf("?") + 1;
    let query = aURL.slice(offset);
    // Iterate a second time over the original input string to determine the
    // correct search term offset and length in the original encoding.
    for (let param of query.split("&")) {
      let equalPos = param.indexOf("=");
      if (equalPos != -1 &&
          param.substr(0, equalPos) == mapEntry.termsParameterName) {
        // This is the parameter we are looking for.
        offset += equalPos + 1;
        length = param.length - equalPos - 1;
        break;
      }
      offset += param.length + 1;
    }

    // Decode the terms using the charset defined in the search engine.
    let terms;
    try {
      terms = gTextToSubURI.UnEscapeAndConvert(
                                       mapEntry.engine.queryCharset,
                                       encodedTerms.replace(/\+/g, " "));
    } catch (ex) {
      // Decoding errors will cause this match to be ignored.
      LOG("Parameter decoding failed. Charset: " +
          mapEntry.engine.queryCharset);
      return gEmptyParseSubmissionResult;
    }

    LOG("Match found. Terms: " + terms);
    return new ParseSubmissionResult(mapEntry.engine, terms, offset, length);
  },

  // nsIObserver
  observe: function(aEngine, aTopic, aVerb) {
    switch (aTopic) {
      case SEARCH_ENGINE_TOPIC:
        switch (aVerb) {
          case SEARCH_ENGINE_LOADED:
            var engine = aEngine.QueryInterface(Ci.nsISearchEngine);
            LOG("nsSearchService::observe: Done installation of " + engine.name
                + ".");
            this._addEngineToStore(engine.wrappedJSObject);
            if (engine.wrappedJSObject._useNow) {
              LOG("nsSearchService::observe: setting current");
              this.currentEngine = aEngine;
            }
            // The addition of the engine to the store always triggers an ADDED
            // or a CHANGED notification, that will trigger the task below.
            break;
          case SEARCH_ENGINE_ADDED:
          case SEARCH_ENGINE_CHANGED:
          case SEARCH_ENGINE_REMOVED:
            this.batchTask.disarm();
            this.batchTask.arm();
            // Invalidate the map used to parse URLs to search engines.
            this._parseSubmissionMap = null;
            break;
        }
        break;

      case QUIT_APPLICATION_TOPIC:
        this._removeObservers();
        break;

      case "nsPref:changed":
        if (aVerb == LOCALE_PREF) {
          // Locale changed. Re-init. We rely on observers, because we can't
          // return this promise to anyone.
          this._asyncReInit();
          break;
        }
    }
  },

  // nsITimerCallback
  notify: function(aTimer) {
    LOG("_notify: checking for updates");

    if (!Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "update", true))
      return;

    // Our timer has expired, but unfortunately, we can't get any data from it.
    // Therefore, we need to walk our engine-list, looking for expired engines
    var currentTime = Date.now();
    LOG("currentTime: " + currentTime);
    for (let name in this._engines) {
      let engine = this._engines[name].wrappedJSObject;
      if (!engine._hasUpdates)
        continue;

      LOG("checking " + engine.name);

      var expirTime = engine.getAttr("updateexpir");
      LOG("expirTime: " + expirTime + "\nupdateURL: " + engine._updateURL +
          "\niconUpdateURL: " + engine._iconUpdateURL);

      var engineExpired = expirTime <= currentTime;

      if (!expirTime || !engineExpired) {
        LOG("skipping engine");
        continue;
      }

      LOG(engine.name + " has expired");

      engineUpdateService.update(engine);

      // Schedule the next update
      engineUpdateService.scheduleNextUpdate(engine);

    } // end engine iteration
  },

  _addObservers: function() {
    if (this._observersAdded) {
      // There might be a race between synchronous and asynchronous
      // initialization for which we try to register the observers twice.
      return;
    }
    this._observersAdded = true;

    Services.obs.addObserver(this, SEARCH_ENGINE_TOPIC, false);
    Services.obs.addObserver(this, QUIT_APPLICATION_TOPIC, false);

    // The current stage of shutdown. Used to help analyze crash
    // signatures in case of shutdown timeout.
    let shutdownState = {
      step: "Not started",
      latestError: {
        message: undefined,
        stack: undefined
      }
    };
    OS.File.profileBeforeChange.addBlocker(
      "Search service: shutting down",
      () => Task.spawn(function* () {
        if (this._batchTask) {
          shutdownState.step = "Finalizing batched task";
          try {
            yield this._batchTask.finalize();
            shutdownState.step = "Batched task finalized";
          } catch (ex) {
            shutdownState.step = "Batched task failed to finalize";

            shutdownState.latestError.message = "" + ex;
            if (ex && typeof ex == "object") {
              shutdownState.latestError.stack = ex.stack || undefined;
            }

            // Ensure that error is reported and that it causes tests
            // to fail.
            Promise.reject(ex);
          }
        }
      }.bind(this)),

      () => shutdownState
    );
  },
  _observersAdded: false,

  _removeObservers: function() {
    Services.obs.removeObserver(this, SEARCH_ENGINE_TOPIC);
    Services.obs.removeObserver(this, QUIT_APPLICATION_TOPIC);
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIBrowserSearchService,
    Ci.nsIObserver,
    Ci.nsITimerCallback
  ])
};


const SEARCH_UPDATE_LOG_PREFIX = "*** Search update: ";

/**
 * Outputs aText to the JavaScript console as well as to stdout, if the search
 * logging pref (browser.search.update.log) is set to true.
 */
function ULOG(aText) {
  if (Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "update.log", false)) {
    dump(SEARCH_UPDATE_LOG_PREFIX + aText + "\n");
    Services.console.logStringMessage(aText);
  }
}

var engineUpdateService = {
  scheduleNextUpdate: function(aEngine) {
    var interval = aEngine._updateInterval || SEARCH_DEFAULT_UPDATE_INTERVAL;
    var milliseconds = interval * 86400000; // |interval| is in days
    aEngine.setAttr("updateexpir", Date.now() + milliseconds);
  },

  update: function(aEngine) {
    let engine = aEngine.wrappedJSObject;
    ULOG("update called for " + aEngine._name);
    if (!Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "update", true) || !engine._hasUpdates)
      return;

    let testEngine = null;
    let updateURL = engine._getURLOfType(URLTYPE_OPENSEARCH);
    let updateURI = (updateURL && updateURL._hasRelation("self")) ?
                     updateURL.getSubmission("", engine).uri :
                     makeURI(engine._updateURL);
    if (updateURI) {
      if (engine._isDefault && !updateURI.schemeIs("https")) {
        ULOG("Invalid scheme for default engine update");
        return;
      }

      ULOG("updating " + engine.name + " from " + updateURI.spec);
      testEngine = new Engine(updateURI, false);
      testEngine._engineToUpdate = engine;
      testEngine._initFromURIAndLoad(updateURI);
    } else
      ULOG("invalid updateURI");

    if (engine._iconUpdateURL) {
      // If we're updating the engine too, use the new engine object,
      // otherwise use the existing engine object.
      (testEngine || engine)._setIcon(engine._iconUpdateURL, true);
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SearchService]);
