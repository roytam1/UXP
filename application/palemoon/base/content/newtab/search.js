#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

const SEARCH_ENGINES = {
  "DuckDuckGo": {
  	 image: "data:image/png;base64," +
            "iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAMAAACdt4HsAAACT1BMVEXvISn/////9/fvUlr3ra3/" +
            "zs7/7+/va2v/5+f/xsbvMTn/tbX/3t7/vb3vOUL3WmPvQkr/zgDvKTHvSlL3hIT3paX/1tbnISn3" +
            "c3v3e3v3a3P3jIz3nJz/tb33c3PvKSn3lJT39/cAc73vSkr3e4Tv7+/3Yxj3pa3/tQj3jJT3nKX3" +
            "Y2P/xs73hIzvQkL/vQjvQiHn5+f3hBD/ztbvMTH/vcb/3ucIc733lJz/pQilzufe7/fvMSHOzs73" +
            "//cQrUpKvVprxmP3Y2vvShiUzmvWlJRzzmMYtUrvOTnn7/davVrWra3v9//nY2PvISGUxudztd7e" +
            "3t7/76XvKSHea2v/xgDnOUK93vfW5/f/1t73Uhj/52ut3q2l3rXO784pjMZrrdb/rQjera3/5+/e" +
            "paWMxufO79aEazkYrUr/nAj3jBD3axj3lBD///fehIRKpd7/1hCEYzk5vVL3//8ptVLW77UxtVLn" +
            "SlLW1tZCvVp7vef/1gj/3invSkL//+fWtbXvpaX/3kr/97XvnJznWmMxjM5zvefOxsbWnKXWjIzG" +
            "3u/ea3Pn997O5/fnQkqExuf3Whit1u/nUlrnxs7v5+d7zmuU1pT3exDOSjFjrVL/987/pUoQe8b/" +
            "75T/3jFKxnO158bWKSl7zoRSxmtajEK1e0pzxlqcUjH/1iHOMSnOvb33cxDWnJx7td6EzmP/74xz" +
            "azlrcznec3Pe771jxlpzczne78YpvVqEvWPn99YxvWOtSjHee3vG787OOTE5lEK1QjHv9+drzmve" +
            "tbXO772q+r8wAAAFbUlEQVR4Xo2X84PzTBDHN3Zqu2fbemzbNl7atm3btvGHvTNJ2myuyd3NL2mT" +
            "zmdnvjM76RImyGQlH5dCHBeSmscNmQkyfwBrZMLEY2aRF5cMSDYPEx+LZpUlAYRQbVEpnuc1je/M" +
            "SbVwYoVFAbpE0IaLmiwqiVymmE3H84YuGs2mheCEhQH5qPUrje2ONxHKVIkXR2x2MxsMkDnLvftk" +
            "2fSTQNCzSAgngwCCipkXxHiU+BsnCDFE8f6AQgnwaTGhkmDLymW8jPsBeIsth8iCpha618El1wgo" +
            "4FOhWyWLWY+O8pbnAwTI29S1ElncJBmF4L0AGeJSdR4dUpt5w+DL0nAgoUuGGKKCBxDCOxrykaDb" +
            "+yFQjhUylLlXpAB5jGnIqV6uvvWUcAAhLmDBXIAMrkXRdHQ+cerUiWefq1hRrAgg8LikUgdkQUAx" +
            "6+2Ze0WLEO/1BQzrHCFNrAPAeDSD4q/Ln6R3p68MSYzDAUiwIEutJM0bHXE/gpEhJMxaAB3T6aT8" +
            "mfkm+QBiMlwKFqAHvrHu9tvTOLrEdX4hFAkJWQB42qbVyam75ruv3zvF+wBCKJ0MAAV6SAy5+raA" +
            "y+lb9tYBUw9sffKRJh+CDl2SAEAPquaC76swU1c+zlxbA9if/EIY78AcCBODDKjnVzDM0+sb57zq" +
            "N14gdpbg4nraBaxm3NWpIDKNgJIIDTxEAKMyVM9/VrFcpijK52PbNhmk0RQORCA8dhGhIkDA+qPV" +
            "Y/U8No2NHZsUfQCdzYTECSiRSRJKgxYAnK6+tnVrPYL7q2P7GNNnT0L3SQSS61AowK4BAExWq9XJ" +
            "OmDT5D4GtUab7p92W1aD6AFBOjUKcONNKMG2o9vmScmhd+v5SCTS91StDLBwmHR5q0iiM4yv3X5g" +
            "sD1i24tUHc0GQOrOihdw+ZV7drx+8I1IzfpaCQ1oSIGsbqEBdxy8KkLb8dYt7m7AFBpEJI8OUIAd" +
            "Hve+wX509IqYgzLqxKMi5X+r6737wgHfMrZBKGwpQMWP0PN8/8qLn15cSRosEQeI3coxGrzRVfE2" +
            "BEyTAMNpmbA3k2erPOyq+CUCPGvv3OmGykYBQhiYFbynDLu2uyW826qb7bSlv/VCe2R3vQqhIYQQ" +
            "nLmSGKUAT1AqXn7V6p72iUsTThsNuhKUAeKMNFaiW2nG08H90IF1m6DywVdsHgA4bPgRGgAqUgBr" +
            "DwxOtPcdv9RK6yklnaGKOXBMmN7RVCtJJMiUdG2s78dv9HbY7KrI9AQBOHwjaxaA6cKhRLXCHkpF" +
            "PrAJYBz1su7LtSBQIjzozgI5AJDWsQ7gTJxETTHuEh5yW8kR5+1fvQBT5PDdWgPokE6GSuK3Aaby" +
            "2KwNyGFIZ8/NfexVMAGXEfe8MA5QTVdrgGe2M9evev6FMwiAYr308nVzcx/SgHwSlswyLgDLHU0K" +
            "tX5UZwCwZsM1b7516J1333v/g2UAuJoCNMsmZkEDZBXujCoOIfVJxQKsvXnDshvWfrEcAV9RAoqY" +
            "rfdvHjY06R3tVmtjzQYsQ8ByC/C1O0dEzqkAGqELbiZ1W/RvBr51Ad9ZgO8dQCkh4/q5xvMC6hot" +
            "sBl7rP1QT+HHQz9RGoSHhkyMgqEBdNPFWSWMY+1nBPxy+MjvZ2aZxB9n/zz3FwKiOTZfotb3AhhF" +
            "xSUUNmGSjX+vWvPPYacVWJOkUilUT05ymEVb0JFHj9l/AVn+35b/jsx6YzNz8mja+iAEH7rYDntY" +
            "Gaz3dizW080KWaeICx77kiG7lTKG6EEoPb0Wu0lZ9OA5whFH8GxHQjOMQls5HSs5t/glHX2FYtT/" +
            "mGAs/fCtFU0vQJUSQYfvIBvVyukuLhbjuood/H6WCbD/AQSFvIO3JDxgAAAAAElFTkSuQmCC"
  }
};

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
  window.gObserver.observe(document.documentElement, { attributes: true });
});

window.addEventListener("pagehide", function() {
  window.gObserver.disconnect();
});

function onSearchSubmit(aEvent) {
  let searchTerms = document.getElementById("searchText").value;
  let searchURL = document.documentElement.getAttribute("searchEngineURL");

  if (searchURL && searchTerms.length > 0) {
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


function setupSearchEngine() {
  let searchText = document.getElementById("searchText");
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
    logoElt.parentNode.hidden = true;
    searchText.placeholder = searchEngineName;
  }
}
