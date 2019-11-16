/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tries to find a configuration for this ISP on the local harddisk, in the
 * application install directory's "isp" subdirectory.
 * Params @see fetchConfigFromISP()
 */

Components.utils.import("resource:///modules/mailServices.js");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/JXON.js");


function fetchConfigFromDisk(domain, successCallback, errorCallback)
{
  return new TimeoutAbortable(runAsync(function()
  {
    try {
      // <TB installdir>/isp/example.com.xml
      var configLocation = Services.dirsvc.get("CurProcD", Ci.nsIFile);
      configLocation.append("isp");
      configLocation.append(sanitize.hostname(domain) + ".xml");

      var contents =
        readURLasUTF8(Services.io.newFileURI(configLocation));
      let domParser = Cc["@mozilla.org/xmlextras/domparser;1"]
                       .createInstance(Ci.nsIDOMParser);
      successCallback(readFromXML(JXON.build(
        domParser.parseFromString(contents, "text/xml"))));
    } catch (e) { errorCallback(e); }
  }));
}

/**
 * Tries to get a configuration from the ISP / mail provider directly.
 *
 * Disclaimers:
 * - To support domain hosters, we cannot use SSL. That means we
 *   rely on insecure DNS and http, which means the results may be
 *   forged when under attack. The same is true for guessConfig(), though.
 *
 * @param domain {String}   The domain part of the user's email address
 * @param emailAddress {String}   The user's email address
 * @param successCallback {Function(config {AccountConfig}})}   A callback that
 *         will be called when we could retrieve a configuration.
 *         The AccountConfig object will be passed in as first parameter.
 * @param errorCallback {Function(ex)}   A callback that
 *         will be called when we could not retrieve a configuration,
 *         for whatever reason. This is expected (e.g. when there's no config
 *         for this domain at this location),
 *         so do not unconditionally show this to the user.
 *         The first paramter will be an exception object or error string.
 */
function fetchConfigFromISP(domain, emailAddress, successCallback,
                            errorCallback)
{
  if (!Services.prefs.getBoolPref(
      "mailnews.auto_config.fetchFromISP.enabled")) {
    errorCallback("ISP fetch disabled per user preference");
    return;
  }

  let conf1 = "autoconfig." + sanitize.hostname(domain) +
              "/mail/config-v1.1.xml";
  // .well-known/ <http://tools.ietf.org/html/draft-nottingham-site-meta-04>
  let conf2 = sanitize.hostname(domain) +
              "/.well-known/autoconfig/mail/config-v1.1.xml";
  // This list is sorted by decreasing priority
  var urls = ["https://" + conf1, "https://" + conf2];
  if (!Services.prefs.getBoolPref("mailnews.auto_config.fetchFromISP.sslOnly")) {
    urls.push("http://" + conf1, "http://" + conf2);
  }
  let sucAbortable = new SuccessiveAbortable();
  var urlArgs = { emailaddress: emailAddress };
  if (!Services.prefs.getBoolPref(
      "mailnews.auto_config.fetchFromISP.sendEmailAddress")) {
    delete urlArgs.emailaddress;
  }
  var time;

  let success = function(result) {
    successCallback(readFromXML(result));
  };

  let error = function(i, e) {
    ddump("fetchisp " + i + " <" + urls[i] + "> took " +
          (Date.now() - time) + "ms and failed with " + e);

    if (i == urls.length - 1 || // implies all fetches failed
        e instanceof CancelledException) {
      errorCallback(e);
      return;
    }
    let fetch = new FetchHTTP(urls[i + 1], urlArgs, false, success,
                              function(e) { error(i + 1, e) });
    sucAbortable.current = fetch;
    time = Date.now();
    fetch.start();
  };

  let fetch = new FetchHTTP(urls[0], urlArgs, false, success,
                            function(e) { error(0, e) });
  sucAbortable.current = fetch;
  time = Date.now();
  fetch.start();
  return sucAbortable;
}

/**
 * Tries to get a configuration for this ISP from a central database at
 * Mozilla servers.
 * Params @see fetchConfigFromISP()
 */

function fetchConfigFromDB(domain, successCallback, errorCallback)
{
  let url = Services.prefs.getCharPref("mailnews.auto_config_url");
  domain = sanitize.hostname(domain);

  // If we don't specify a place to put the domain, put it at the end.
  if (!url.includes("{{domain}}"))
    url = url + domain;
  else
    url = url.replace("{{domain}}", domain);
  url = url.replace("{{accounts}}", MailServices.accounts.accounts.length);

  if (!url.length)
    return errorCallback("no fetch url set");
  let fetch = new FetchHTTP(url, null, false,
                            function(result)
                            {
                              successCallback(readFromXML(result));
                            },
                            errorCallback);
  fetch.start();
  return fetch;
}

/**
 * Does a lookup of DNS MX, to get the server who is responsible for
 * recieving mail for this domain. Then it takes the domain of that
 * server, and does another lookup (in ISPDB and possible at ISP autoconfig
 * server) and if such a config is found, returns that.
 *
 * Disclaimers:
 * - DNS is unprotected, meaning the results could be forged.
 *   The same is true for fetchConfigFromISP() and guessConfig(), though.
 * - DNS MX tells us the incoming server, not the mailbox (IMAP) server.
 *   They are different. This mechnism is only an approximation
 *   for hosted domains (yourname.com is served by mx.hoster.com and
 *   therefore imap.hoster.com - that "therefore" is exactly the
 *   conclusional jump we make here.) and alternative domains
 *   (e.g. yahoo.de -> yahoo.com).
 * - We make a look up for the base domain. E.g. if MX is
 *   mx1.incoming.servers.hoster.com, we look up hoster.com.
 *   Thanks to Services.eTLD, we also get bbc.co.uk right.
 *
 * Params @see fetchConfigFromISP()
 */
function fetchConfigForMX(domain, successCallback, errorCallback)
{
  domain = sanitize.hostname(domain);

  var sucAbortable = new SuccessiveAbortable();
  var time = Date.now();
  sucAbortable.current = getMX(domain,
    function(mxHostname) // success
    {
      ddump("getmx took " + (Date.now() - time) + "ms");
      let sld = Services.eTLD.getBaseDomainFromHost(mxHostname);
      ddump("base domain " + sld + " for " + mxHostname);
      if (sld == domain)
      {
        errorCallback("MX lookup would be no different from domain");
        return;
      }
      sucAbortable.current = fetchConfigFromDB(sld, successCallback,
                                               errorCallback);
    },
    errorCallback);
  return sucAbortable;
}

/**
 * Queries the DNS MX for the domain
 *
 * The current implementation goes to a web service to do the
 * DNS resolve for us, because Mozilla unfortunately has no implementation
 * to do it. That's just a workaround. Once bug 545866 is fixed, we make
 * the DNS query directly on the client. The API of this function should not
 * change then.
 *
 * Returns (in successCallback) the hostname of the MX server.
 * If there are several entires with different preference values,
 * only the most preferred (i.e. those with the lowest value)
 * is returned. If there are several most preferred servers (i.e.
 * round robin), only one of them is returned.
 *
 * @param domain @see fetchConfigFromISP()
 * @param successCallback {function(hostname {String})
 *   Called when we found an MX for the domain.
 *   For |hostname|, see description above.
 * @param errorCallback @see fetchConfigFromISP()
 * @returns @see fetchConfigFromISP()
 */
function getMX(domain, successCallback, errorCallback)
{
  domain = sanitize.hostname(domain);

  let url = Services.prefs.getCharPref("mailnews.mx_service_url");
  if (!url)
    errorCallback("no URL for MX service configured");
  url += domain;

  let fetch = new FetchHTTP(url, null, false,
    function(result)
    {
      // result is plain text, with one line per server.
      // So just take the first line
      ddump("MX query result: \n" + result + "(end)");
      assert(typeof(result) == "string");
      let first = result.split("\n")[0];
      first.toLowerCase().replace(/[^a-z0-9\-_\.]*/g, "");
      if (first.length == 0)
      {
        errorCallback("no MX found");
        return;
      }
      successCallback(first);
    },
    errorCallback);
  fetch.start();
  return fetch;
}
