/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const UNKNOWN = nsIPermissionManager.UNKNOWN_ACTION;   // 0
const ALLOW = nsIPermissionManager.ALLOW_ACTION;       // 1
const DENY = nsIPermissionManager.DENY_ACTION;         // 2
const SESSION = nsICookiePermission.ACCESS_SESSION;    // 8

const IMAGE_DENY = 2;

const COOKIE_DENY = 2;
const COOKIE_SESSION = 2;

var gPermURI;
var gPermPrincipal;
var gPrefs;
var gUsageRequest;

var gPermObj = {
  image: function()
  {
    if (gPrefs.getIntPref("permissions.default.image") == IMAGE_DENY) {
      return DENY;
    }
    return ALLOW;
  },
  popup: function()
  {
    if (gPrefs.getBoolPref("dom.disable_open_during_load")) {
      return DENY;
    }
    return ALLOW;
  },
  cookie: function()
  {
    if (gPrefs.getIntPref("network.cookie.cookieBehavior") == COOKIE_DENY) {
      return DENY;
    }
    if (gPrefs.getIntPref("network.cookie.lifetimePolicy") == COOKIE_SESSION) {
      return SESSION;
    }
    return ALLOW;
  },
  "desktop-notification": function()
  {
    if (!gPrefs.getBoolPref("dom.webnotifications.enabled")) {
      return DENY;
    }
    return UNKNOWN;
  },
  install: function()
  {
    if (Services.prefs.getBoolPref("xpinstall.whitelist.required")) {
      return DENY;
    }
    return ALLOW;
  },
  geo: function()
  {
    if (!gPrefs.getBoolPref("geo.enabled")) {
      return DENY;
    }
    return ALLOW;
  },
  plugins: function()
  {
    return UNKNOWN;
  },
};

var permissionObserver = {
  observe: function(aSubject, aTopic, aData)
  {
    if (aTopic == "perm-changed") {
      var permission = aSubject.QueryInterface(
                       Components.interfaces.nsIPermission);
      if (permission.matchesURI(gPermURI, true)) {
        if (permission.type in gPermObj)
          initRow(permission.type);
        else if (permission.type.startsWith("plugin"))
          setPluginsRadioState();
      }
    }
  }
};

function onLoadPermission(principal)
{
  gPrefs = Components.classes[PREFERENCES_CONTRACTID]
                     .getService(Components.interfaces.nsIPrefBranch);

  var uri = gDocument.documentURIObject;
  var permTab = document.getElementById("permTab");
  if (/^https?$/.test(uri.scheme)) {
    gPermURI = uri;
    gPermPrincipal = principal;
    var hostText = document.getElementById("hostText");
    hostText.value = gPermURI.prePath;

    for (var i in gPermObj)
      initRow(i);
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.addObserver(permissionObserver, "perm-changed", false);
    onUnloadRegistry.push(onUnloadPermission);
    permTab.hidden = false;
  }
  else
    permTab.hidden = true;
}

function onUnloadPermission()
{
  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
  os.removeObserver(permissionObserver, "perm-changed");

  if (gUsageRequest) {
    gUsageRequest.cancel();
    gUsageRequest = null;
  }
}

function initRow(aPartId)
{
  if (aPartId == "plugins") {
    initPluginsRow();
    return;
  }

  var permissionManager = Components.classes[PERMISSION_CONTRACTID]
                                    .getService(nsIPermissionManager);

  var checkbox = document.getElementById(aPartId + "Def");
  var command  = document.getElementById("cmd_" + aPartId + "Toggle");
  // Desktop Notification, Geolocation and PointerLock permission consumers
  // use testExactPermission, not testPermission.
  var perm;
  if (aPartId == "desktop-notification" || aPartId == "geo" || aPartId == "pointerLock")
    perm = permissionManager.testExactPermission(gPermURI, aPartId);
  else
    perm = permissionManager.testPermission(gPermURI, aPartId);

  if (perm) {
    checkbox.checked = false;
    command.removeAttribute("disabled");
  }
  else {
    checkbox.checked = true;
    command.setAttribute("disabled", "true");
    perm = gPermObj[aPartId]();
  }
  setRadioState(aPartId, perm);
}

function onCheckboxClick(aPartId)
{
  var permissionManager = Components.classes[PERMISSION_CONTRACTID]
                                    .getService(nsIPermissionManager);

  var command  = document.getElementById("cmd_" + aPartId + "Toggle");
  var checkbox = document.getElementById(aPartId + "Def");
  if (checkbox.checked) {
    permissionManager.remove(gPermURI, aPartId);
    command.setAttribute("disabled", "true");
    var perm = gPermObj[aPartId]();
    setRadioState(aPartId, perm);
  }
  else {
    onRadioClick(aPartId);
    command.removeAttribute("disabled");
  }
}

function onPluginRadioClick(aEvent) {
  onRadioClick(aEvent.originalTarget.getAttribute("id").split('#')[0]);
}

function onRadioClick(aPartId)
{
  var permissionManager = Components.classes[PERMISSION_CONTRACTID]
                                    .getService(nsIPermissionManager);

  var radioGroup = document.getElementById(aPartId + "RadioGroup");
  var id = radioGroup.selectedItem.id;
  var permission = id.split('#')[1];
  if (permission == UNKNOWN) {
    permissionManager.remove(gPermURI, aPartId);
  } else {
    permissionManager.add(gPermURI, aPartId, permission);
  }
}

function setRadioState(aPartId, aValue)
{
  var radio = document.getElementById(aPartId + "#" + aValue);
  radio.radioGroup.selectedItem = radio;
}

// XXX copied this from browser-plugins.js - is there a way to share?
function makeNicePluginName(aName) {
  if (aName == "Shockwave Flash")
    return "Adobe Flash";

  // Clean up the plugin name by stripping off any trailing version numbers
  // or "plugin". EG, "Foo Bar Plugin 1.23_02" --> "Foo Bar"
  // Do this by first stripping the numbers, etc. off the end, and then
  // removing "Plugin" (and then trimming to get rid of any whitespace).
  // (Otherwise, something like "Java(TM) Plug-in 1.7.0_07" gets mangled)
  let newName = aName.replace(/[\s\d\.\-\_\(\)]+$/, "").replace(/\bplug-?in\b/i, "").trim();
  return newName;
}

function fillInPluginPermissionTemplate(aPermissionString, aPluginObject) {
  let permPluginTemplate = document.getElementById("permPluginTemplate")
                           .cloneNode(true);
  permPluginTemplate.setAttribute("permString", aPermissionString);
  permPluginTemplate.setAttribute("tooltiptext", aPluginObject.description);
  let attrs = [];
  attrs.push([".permPluginTemplateLabel", "value", aPluginObject.name]);
  attrs.push([".permPluginTemplateRadioGroup", "id", aPermissionString + "RadioGroup"]);
  attrs.push([".permPluginTemplateRadioDefault", "id", aPermissionString + "#0"]);
  let permPluginTemplateRadioAsk = ".permPluginTemplateRadioAsk";
  if (Services.prefs.getBoolPref("plugins.click_to_play") ||
      aPluginObject.vulnerable) {
    attrs.push([permPluginTemplateRadioAsk, "id", aPermissionString + "#3"]);
  } else {
    permPluginTemplate.querySelector(permPluginTemplateRadioAsk)
        .setAttribute("disabled", "true");
  }
  attrs.push([".permPluginTemplateRadioAllow", "id", aPermissionString + "#1"]);
  attrs.push([".permPluginTemplateRadioBlock", "id", aPermissionString + "#2"]);

  for (let attr of attrs) {
    permPluginTemplate.querySelector(attr[0]).setAttribute(attr[1], attr[2]);
  }

  return permPluginTemplate;
}

function clearPluginPermissionTemplate() {
  let permPluginTemplate = document.getElementById("permPluginTemplate");
  permPluginTemplate.hidden = true;
  permPluginTemplate.removeAttribute("permString");
  permPluginTemplate.removeAttribute("tooltiptext");
  document.querySelector(".permPluginTemplateLabel").removeAttribute("value");
  document.querySelector(".permPluginTemplateRadioGroup").removeAttribute("id");
  document.querySelector(".permPluginTemplateRadioAsk").removeAttribute("id");
  document.querySelector(".permPluginTemplateRadioAllow").removeAttribute("id");
  document.querySelector(".permPluginTemplateRadioBlock").removeAttribute("id");
}

function initPluginsRow() {
  let vulnerableLabel = document.getElementById("browserBundle")
                        .getString("pluginActivateVulnerable.label");
  let pluginHost = Components.classes["@mozilla.org/plugin/host;1"]
                   .getService(Components.interfaces.nsIPluginHost);
  let tags = pluginHost.getPluginTags();

  let permissionMap = new Map();

  for (let plugin of tags) {
    if (plugin.disabled) {
      continue;
    }
    for (let mimeType of plugin.getMimeTypes()) {
      if (mimeType == "application/x-shockwave-flash" && plugin.name != "Shockwave Flash") {
        continue;
      }
      // XXX: Guard against plug-ins that include an empty MIME type
      // in their list of handled MIME types (e.g. latest Java SE 8 plug-in).
      if (mimeType.length == 0) {
        continue;
      }
      let permString = pluginHost.getPermissionStringForType(mimeType);
      if (!permissionMap.has(permString)) {
        let name = makeNicePluginName(plugin.name) + " " + plugin.version;
        let vulnerable = false;
        if (permString.startsWith("plugin-vulnerable:")) {
          name += " \u2014 " + vulnerableLabel;
          vulnerable = true;
        }
        permissionMap.set(permString, {
          "name": name,
          "description": plugin.description,
          "vulnerable": vulnerable 
        });
      }
    }
  }

  // Tycho:
  // let entries = [
  //   {
  //     "permission": item[0],
  //     "obj": item[1],
  //   }
  //   for (item of permissionMap)
  // ];
  let entries = [];
  for (let item of permissionMap) {
    entries.push({
      "permission": item[0],
      "obj": item[1]
    });
  }    
  entries.sort(function(a, b) {
    return ((a.obj.name < b.obj.name) ? -1 : (a.obj.name == b.obj.name ? 0 : 1));
  });

  // Tycho:
  // let permissionEntries = [
  //   fillInPluginPermissionTemplate(p.permission, p.obj) for (p of entries)
  // ];
  let permissionEntries = [];
  entries.forEach(function(p) {
    permissionEntries.push(fillInPluginPermissionTemplate(p.permission, p.obj));
  });

  let permPluginsRow = document.getElementById("permPluginsRow");
  clearPluginPermissionTemplate();
  if (permissionEntries.length < 1) {
    permPluginsRow.hidden = true;
    return;
  }

  for (let permissionEntry of permissionEntries) {
    permPluginsRow.appendChild(permissionEntry);
  }

  setPluginsRadioState();
}

function setPluginsRadioState() {
  var permissionManager = Components.classes[PERMISSION_CONTRACTID]
                                    .getService(nsIPermissionManager);
  let box = document.getElementById("permPluginsRow");
  for (let permissionEntry of box.childNodes) {
    if (permissionEntry.hasAttribute("permString")) {
      let permString = permissionEntry.getAttribute("permString");
      let permission = permissionManager.testPermission(gPermURI, permString);
      setRadioState(permString, permission);
    }
  }
}
