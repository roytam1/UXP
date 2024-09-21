/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Details of supported OAuth2 Providers.
 */
var EXPORTED_SYMBOLS = ["OAuth2Providers"];

var {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

// map of hostnames to [issuer, scope]
var kHostnames = new Map([
  ["imap.googlemail.com", ["accounts.google.com", "https://mail.google.com/"]],
  ["smtp.googlemail.com", ["accounts.google.com", "https://mail.google.com/"]],
  ["imap.gmail.com", ["accounts.google.com", "https://mail.google.com/"]],
  ["smtp.gmail.com", ["accounts.google.com", "https://mail.google.com/"]],

  ["imap.mail.ru", ["o2.mail.ru", "mail.imap"]],
  ["smtp.mail.ru", ["o2.mail.ru", "mail.imap"]],

  ["imap.mail.yahoo.com", ["login.yahoo.com", "mail-w"]],
  ["smtp.mail.yahoo.com", ["login.yahoo.com", "mail-w"]],

  ["imap.aol.com", ["login.aol.com", "mail-w"]],
  ["smtp.aol.com", ["login.aol.com", "mail-w"]],

  ["outlook.office365.com", [
      "login.microsoftonline.com",
      "https://outlook.office365.com/IMAP.AccessAsUser.All https://outlook.office365.com/POP.AccessAsUser.All https://outlook.office365.com/SMTP.Send offline_access",
  ]],
  ["smtp.office365.com", [
      "login.microsoftonline.com",
      "https://outlook.office365.com/IMAP.AccessAsUser.All https://outlook.office365.com/POP.AccessAsUser.All https://outlook.office365.com/SMTP.Send offline_access",
  ]],
]);

// map of issuers to appKey, appSecret, authURI, tokenURI

// For the moment, these details are hard-coded, since Google does not
// provide dynamic client registration. Don't copy these values for your
// own application--register it yourself. This code (and possibly even the
// registration itself) will disappear when this is switched to dynamic
// client registration.
var kIssuers = new Map ([
  ["accounts.google.com", [
    '406964657835-aq8lmia8j95dhl1a2bvharmfk3t1hgqj.apps.googleusercontent.com',
    'kSmqreRr0qwBWJgbf5Y-PjSU',
    'https://accounts.google.com/o/oauth2/auth',
    'https://www.googleapis.com/oauth2/v3/token'
  ]],
  ["o2.mail.ru", [
    'thunderbird',
    'I0dCAXrcaNFujaaY',
    'https://o2.mail.ru/login',
    'https://o2.mail.ru/token'
  ]],
  ["login.yahoo.com", [
    'dj0yJmk9NUtCTWFMNVpTaVJmJmQ9WVdrOVJ6UjVTa2xJTXpRbWNHbzlNQS0tJnM9Y29uc3VtZXJzZWNyZXQmeD0yYw--',
    'f2de6a30ae123cdbc258c15e0812799010d589cc',
    'https://api.login.yahoo.com/oauth2/request_auth',
    'https://api.login.yahoo.com/oauth2/get_token'
  ]],
  ["login.aol.com", [
    'dj0yJmk9OXRHc1FqZHRQYzVvJmQ9WVdrOU1UQnJOR0pvTjJrbWNHbzlNQS0tJnM9Y29uc3VtZXJzZWNyZXQmeD02NQ--',
    '79c1c11991d148ddd02a919000d69879942fc278',
    'https://api.login.aol.com/oauth2/request_auth',
    'https://api.login.aol.com/oauth2/get_token'
  ]],
  ["login.microsoftonline.com", [
    '08162f7c-0fd2-4200-a84a-f25a4db0b584', // Application (client) ID
    'TxRBilcHdC6WGBee]fs?QR:SJ8nI[g82', // @see App registrations | Certificates & secrets
    // https://docs.microsoft.com/en-us/azure/active-directory/develop/active-directory-v2-protocols#endpoints
    'https://login.microsoftonline.com/common/oauth2/v2.0/authorize',
    'https://login.microsoftonline.com/common/oauth2/v2.0/token'
  ]],
]);

/**
 *  OAuth2Providers: Methods to lookup OAuth2 parameters for supported
 *                   email providers.
 */
var OAuth2Providers = {

  /**
   * Map a hostname to the relevant issuer and scope.
   *
   * @param aHostname  String representing the url for an imap or smtp
   *                   server (example "imap.googlemail.com").
   *
   * @returns          Array with [issuer, scope] for the hostname if found,
   *                   else undefined. issuer is a string representing the
   *                   organization, scope is an oauth parameter describing\
   *                   the required access level.
   */
  getHostnameDetails: function (aHostname) { return kHostnames.get(aHostname);},

  /**
   * Map an issuer to OAuth2 account details.
   *
   * @param aIssuer    The organization issuing oauth2 parameters, example
   *                   "accounts.google.com".
   *
   * @return           Array containing [appKey, appSecret, authURI, tokenURI]
   *                   where appKey and appDetails are strings representing the
   *                   account registered for Thunderbird with the organization,
   *                   authURI and tokenURI are url strings representing
   *                   endpoints to access OAuth2 authentication.
   */
  getIssuerDetails: function (aIssuer) { return kIssuers.get(aIssuer);}
}
