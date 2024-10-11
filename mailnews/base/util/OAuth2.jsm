/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides OAuth 2.0 authentication.
 * @see RFC 6749
 */
var EXPORTED_SYMBOLS = ["OAuth2"];

var {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/gloda/log4moz.js");

Cu.importGlobalProperties(["fetch"]);

// Only allow one connecting window per endpoint.
var gConnecting = {};

/**
 * Constructor for the OAuth2 object.
 *
 * @constructor
 * @param {string} aBaseURI - The base URI for authentication and token
 *   requests, oauth2/auth or oauth2/token will be added for the actual
 *   requests.
 * @param {?string} aScope - The scope as specified by RFC 6749 Section 3.3.
 *   Will not be included in the requests if falsy.
 * @param {string} aAppKey - The client_id as specified by RFC 6749 Section
 *   2.3.1.
 * @param {string} [aAppSecret=null] - The client_secret as specified in
 *    RFC 6749 section 2.3.1. Will not be included in the requests if null.
 */
function OAuth2(aBaseURI, aScope, aAppKey, aAppSecret = null) {
    this.authURI = aBaseURI + "oauth2/auth";
    this.tokenURI = aBaseURI + "oauth2/token";
    this.consumerKey = aAppKey;
    this.consumerSecret = aAppSecret;
    this.scope = aScope;
    this.extraAuthParams = [];

    this.log = Log4Moz.getConfiguredLogger("TBOAuth");
}

OAuth2.prototype = {
    consumerKey: null,
    consumerSecret: null,
    completionURI: "http://localhost",
    requestWindowURI: "chrome://messenger/content/browserRequest.xul",
    requestWindowFeatures: "chrome,non-private,centerscreen,width=980,height=600",
    requestWindowTitle: "",
    scope: null,

    accessToken: null,
    refreshToken: null,
    tokenExpires: 0,

    connect: function connect(aSuccess, aFailure, aWithUI, aRefresh) {

        this.connectSuccessCallback = aSuccess;
        this.connectFailureCallback = aFailure;

        if (!aRefresh && this.accessToken) {
            aSuccess();
        } else if (this.refreshToken) {
          this.requestAccessToken(this.refreshToken, true);
        } else {
            if (!aWithUI) {
                aFailure('{ "error": "auth_noui" }');
                return;
            }
            if (gConnecting[this.authURI]) {
                aFailure("Window already open");
                return;
            }
            this.requestAuthorization();
        }
    },

    requestAuthorization: function requestAuthorization() {
    let params = new URLSearchParams({
      response_type: "code",
      client_id: this.consumerKey,
      redirect_uri: this.completionURI,
    });

    // The scope is optional.
        if (this.scope) {
          params.append("scope", this.scope);
        }

    for (let [name, value] of this.extraAuthParams) {
      params.append(name, value);
    }

    let authEndpointURI = this.authURI + "?" + params.toString();
    this.log.info(
      "Interacting with the resource owner to obtain an authorization grant " +
        "from the authorization endpoint: " +
        authEndpointURI
    );

        this._browserRequest = {
            account: this,
            url: authEndpointURI,
            _active: true,
            iconURI: "",
            cancelled: function() {
                if (!this._active) {
                    return;
                }

                this.account.finishAuthorizationRequest();
                this.account.onAuthorizationFailed(Components.results.NS_ERROR_ABORT, '{ "error": "cancelled"}');
            },

            loaded: function (aWindow, aWebProgress) {
                if (!this._active) {
                    return;
                }

                this._listener = {
                    window: aWindow,
                    webProgress: aWebProgress,
                    _parent: this.account,

                    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                                           Ci.nsISupportsWeakReference]),

                    _cleanUp: function() {
                      this.webProgress.removeProgressListener(this);
                      this.window.close();
                      delete this.window;
                    },

                    _checkForRedirect: function(aURL) {
                      if (aURL.indexOf(this._parent.completionURI) != 0)
                        return;

                      this._parent.finishAuthorizationRequest();
                      this._parent.onAuthorizationReceived(aURL);
                    },

                    onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
                      const wpl = Ci.nsIWebProgressListener;
                      if (aStateFlags & (wpl.STATE_START | wpl.STATE_IS_NETWORK))
                        this._checkForRedirect(aRequest.name);
                    },
                    onLocationChange: function(aWebProgress, aRequest, aLocation) {
                      this._checkForRedirect(aLocation.spec);
                    },
                    onProgressChange: function() {},
                    onStatusChange: function() {},
                    onSecurityChange: function() {},
                };
                aWebProgress.addProgressListener(this._listener,
                                                 Ci.nsIWebProgress.NOTIFY_ALL);
                aWindow.document.title = this.account.requestWindowTitle;
            }
        };

        this.wrappedJSObject = this._browserRequest;
        gConnecting[this.authURI] = true;
        Services.ww.openWindow(null, this.requestWindowURI, null, this.requestWindowFeatures, this);
    },
    finishAuthorizationRequest: function() {
        gConnecting[this.authURI] = false;
        if (!("_browserRequest" in this)) {
            return;
        }

        this._browserRequest._active = false;
        if ("_listener" in this._browserRequest) {
            this._browserRequest._listener._cleanUp();
        }
        delete this._browserRequest;
    },

  // @see RFC 6749 section 4.1.2: Authorization Response
  onAuthorizationReceived(aURL) {
    this.log.info("OAuth2 authorization received: url=" + aURL);
    let params = new URLSearchParams(aURL.split("?", 2)[1]);
    if (params.has("code")) {
      this.requestAccessToken(params.get("code"), false);
        } else {
          this.onAuthorizationFailed(null, aURL);
        }
    },

    onAuthorizationFailed: function(aError, aData) {
        this.connectFailureCallback(aData);
    },

  /**
   * Request a new access token, or refresh an existing one.
   * @param {string} aCode - The token issued to the client.
   * @param {boolean} aRefresh - Whether it's a refresh of a token or not.
   */
  requestAccessToken(aCode, aRefresh) {
    // @see RFC 6749 section 4.1.3. Access Token Request
    // @see RFC 6749 section 6. Refreshing an Access Token

       let data = new URLSearchParams();
       data.append("client_id", this.consumerKey);
       if (this.consumerSecret !== null) {
         // Section 2.3.1. of RFC 6749 states that empty secrets MAY be omitted
         // by the client. This OAuth implementation delegates this decission to
         // the caller: If the secret is null, it will be omitted.
         data.append("client_secret", this.consumerSecret);
       }

       if (aRefresh) {
       this.log.info(
         `Making a refresh request to the token endpoint: ${this.tokenURI}`
       );
         data.append("grant_type", "refresh_token");
         data.append("refresh_token", aCode);
       } else {
       this.log.info(
         `Making access token request to the token endpoint: ${this.tokenURI}`
       );
         data.append("grant_type", "authorization_code");
         data.append("code", aCode);
         data.append("redirect_uri", this.completionURI);
        }

    fetch(this.tokenURI, {
      method: "POST",
      cache: "no-cache",
      body: data,
    })
      .then(response => response.json())
      .then(result => {
        if ("error" in result) {
          // RFC 6749 section 5.2. Error Response
          this.log.info(
            `The authorization server returned an error response: ${JSON.stringify(
              result
            )}`
          );
          this.connectFailureCallback(result);
          return;
        }

        // RFC 6749 section 5.1. Successful Response
        this.log.info("The authorization server issued an access token.");
        this.accessToken = result.access_token;
        if ("refresh_token" in result) {
          this.refreshToken = result.refresh_token;
        }
        if ("expires_in" in result) {
          this.tokenExpires = new Date().getTime() + result.expires_in * 1000;
        } else {
          this.tokenExpires = Number.MAX_VALUE;
        }
        this.connectSuccessCallback();
      })
      .catch(err => {
        this.log.info(`Connection to authorization server failed: ${err}`);
        this.connectFailureCallback(err);
      });
    }
};
