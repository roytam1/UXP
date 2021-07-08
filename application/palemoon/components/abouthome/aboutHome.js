/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include ../shared/searchenginelogos.js

// This global tracks if the page has been set up before, to prevent double inits
var gInitialized = false;
var gObserver = new MutationObserver(function (mutations) {
  for (let mutation of mutations) {
    if (mutation.attributeName == "searchEngineURL") {
      setupSearchEngine();
      if (!gInitialized) {
        gInitialized = true;
      }
      return;
    }
  }
});

window.addEventListener("pageshow", function () {
  // Delay search engine setup, cause browser.js::BrowserOnAboutPageLoad runs
  // later and may use asynchronous getters.
  window.gObserver.observe(document.documentElement, { attributes: true });
  fitToWidth();
  window.addEventListener("resize", fitToWidth);
});

window.addEventListener("pagehide", function() {
  window.gObserver.disconnect();
  window.removeEventListener("resize", fitToWidth);
});

function onSearchSubmit(aEvent)
{
  let searchTerms = document.getElementById("searchText").value;
  let searchURL = document.documentElement.getAttribute("searchEngineURL");

  if (searchURL && searchTerms.length > 0) {
    // Send an event that a search was performed. This was originally
    // added so Firefox Health Report could record that a search from
    // about:home had occurred.
    let engineName = document.documentElement.getAttribute("searchEngineName");
    let event = new CustomEvent("AboutHomeSearchEvent", {detail: engineName});
    document.dispatchEvent(event);

    const SEARCH_TOKEN = "_searchTerms_";
    let searchPostData = document.documentElement.getAttribute("searchEnginePostData");
    if (searchPostData) {
      // Check if a post form already exists. If so, remove it.
      const POST_FORM_NAME = "searchFormPost";
      let form = document.forms[POST_FORM_NAME];
      if (form) {
        form.parentNode.removeChild(form);
      }

      // Create a new post form.
      form = document.body.appendChild(document.createElement("form"));
      form.setAttribute("name", POST_FORM_NAME);
      // Set the URL to submit the form to.
      form.setAttribute("action", searchURL.replace(SEARCH_TOKEN, searchTerms));
      form.setAttribute("method", "post");

      // Create new <input type=hidden> elements for search param.
      searchPostData = searchPostData.split("&");
      for (let postVar of searchPostData) {
        let [name, value] = postVar.split("=");
        if (value == SEARCH_TOKEN) {
          value = searchTerms;
        }
        let input = document.createElement("input");
        input.setAttribute("type", "hidden");
        input.setAttribute("name", name);
        input.setAttribute("value", value);
        form.appendChild(input);
      }
      // Submit the form.
      form.submit();
   } else {
      searchURL = searchURL.replace(SEARCH_TOKEN, encodeURIComponent(searchTerms));
      window.location.href = searchURL;
    }
  }

  aEvent.preventDefault();
}


function setupSearchEngine()
{
  // The "autofocus" attribute doesn't focus the form element
  // immediately when the element is first drawn, so the
  // attribute is also used for styling when the page first loads.
  let searchText = document.getElementById("searchText");
  searchText.addEventListener("blur", function searchText_onBlur() {
    searchText.removeEventListener("blur", searchText_onBlur);
    searchText.removeAttribute("autofocus");
  });
 
  let searchEngineName = document.documentElement.getAttribute("searchEngineName");
  let searchEngineInfo = SEARCH_ENGINES[searchEngineName];
  let logoElt = document.getElementById("searchEngineLogo");

  // Add search engine logo.
  if (searchEngineInfo && searchEngineInfo.image) {
    logoElt.parentNode.hidden = false;
    logoElt.src = searchEngineInfo.image;
    logoElt.alt = searchEngineName;
    searchText.placeholder = "";
  } else {
    logoElt.parentNode.hidden = false;
    logoElt.src = SEARCH_ENGINES['generic'].image;
    searchText.placeholder = searchEngineName;
  }

}

function fitToWidth() {
  if (window.scrollMaxX) {
    document.body.setAttribute("narrow", "true");
  } else if (document.body.hasAttribute("narrow")) {
    document.body.removeAttribute("narrow");
    fitToWidth();
  }
}
