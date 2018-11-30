/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const SEARCH_ENGINES = {
  "DuckDuckGo": {
  	 image: "data:image/png;base64," +
           "iVBORw0KGgoAAAANSUhEUgAAAIwAAAA4CAYAAAAvmxBdAAAVhUlEQVR4Xu3dd5SU1d3A8e/vPs/0" +
           "2crussBSdkHEAgoomEQSUTAW3hRbfMUeUwgSj9FoorGXqDGxBHvMazRGE0KsBQuiEVRUEEEM0pfO" +
           "1tndmZ32PPf3knDCUZAlIYsxOfM553f2v91/vnufOzP33BFV5TOnQFQ1snFN/YCVb88Z6S2dd1B8" +
           "3Qf7lTSv6R9PNle4uXQEVNRxvUy4qL29pPeGRNXA5d6g4fOLhoyYN2C/oe8Vl5QmAoFAnm72GQqm" +
           "oKO9vXj5e/NHtr48/fjq92eOq2xYOsixvuMpKFuhfJywjQMYI5oKF7evrR09t/LE7z3Ze9TYZyPx" +
           "+FpjjPdfEkxBY0ND9ftP//7EkpceOLNm/cJh+J6rylYWcIwSiCHhuEo4ggRdMCLq+UomK5pJq2Y7" +
           "BD8HqoIAAmKhPdKjuX7EMc9WnfCde/YZOfot13Xz/6HBFKi1pdmlCya23Dz5PPeDN/eygCqAqIn3" +
           "ULduiAb2Ha3BfUYgJeUgBhxHRAwgoupbfF/wPcXL461bRX7xm5Jb8q7Yhno0lzUYMIANx9Lh0y99" +
           "svjEc292YkXzAfufE0yBse0tX+qY+uNrOp/+9SGo5yggTlADQw72I4efQGDf4Wg6RW7xO5Jf8g7+" +
           "ulVi21rRXAr8HKpWRBzFCSGRIpyKSnX6701wv0PU7Vunms2RmfO0ZGc/Z/zWjSKiAqJOdV1LyUVT" +
           "7wkdcuQvENP8mQ+mQGPZt2ZelLj2nCl+Q30ZAqijoVFH+rGTJiHROJnXniE75znxN64yms8AKghd" +
           "062DEZVIqQbq9tHwYcdpcL+DNDvvFUlNv1dsywYHA0jAjx512lslF956vkSL5n5Wgymwfq+O/7vx" +
           "jvZfX/0/+FkXC27N3n7xlOvVlFdp8pFfSnbuC0bTbYKqIOw+BcSoKeut0WNPtZEjjtPOx++X1FMP" +
           "GPysAXD777epxy1PXuj2qXsEsJ+hYArUy9e2Xn7GtPTLj44AFVVHY1/7tld0+g8l+cht2vnE/Y7N" +
           "p0S2htJ9FEDUlPWxxZOusE5VjSRunIK3YbkrAhIpzlRMfeGy4P6jbwH8z0AwBZrPDWqacvQzmfkv" +
           "D0ZETbxCS3/wC9/t1ZeWq78t3oZlDqiwp6nRyJiveMXnXEL7fdeTef1JV9UKKlp118wrQgeNvX5X" +
           "0Rj2uMJjqOmik/6UmbclFkSdylrb4/qHfU0naTzvK463fqkLKijo1oGt0/3ESudrT7jNPznTxL8x" +
           "iehXvuUhroJKw6RxV+aWzJ8MyL9vhSmIJm778fT2h244CiPqVg+0Pa64TzPzZtv2X18XUD8jAIiB" +
           "3nWEK6rBDaHZTmyiCb+lGe1MoGpB6FZOWR+/7KJbbXb+n0lOv8tV64mJlnX2mr74ZKei11PshMue" +
           "UmA6X3nyqrbf/uxIAKe4l5ZdcqdNz5vNllhc9TKCAIAaQ6puNLEzzqN86EhQRTs78BvWkX3/bTpf" +
           "mkZm3p/RbAoM3cJrWe+03PB9yn881drOlJd85gHXT7VGG77/1TvK7n1pRThe/MGnuMIU+M2bj91w" +
           "wrBHbUdDnEDUVlx2n29TbbT8/AIXLy18hAQiFJ8wmdD44wnvPwoxZvs9ENlFb9D2qxvIzH0BxNId" +
           "VMGtGuBXXPNrm7j7OskueNkBKDnjkudKp1x7ItD5KQRToNavaLzgGy91vjr9ABAtPuUCL/LFo2m8" +
           "8ETHJlsMwsek9zqEztMvRbw8TjBMqLSU4spKiquqicVjiAgANtVBx8O3kbjvOtTPgPCvUwjufZBX" +
           "ftEt2njBScZv2+gYN5KvfvCN84N7H3DHpxBMQerNmZc3nHvU5ajnBGqHedW3Psam848jv+I9F2FH" +
           "4qA4gIJvkHgZgeGHEvzSUZSMP4FQccnHVpvk0w+Seu73ZN57Hc11guFfo6JFX/+uFzpgNE1XnOUi" +
           "KpEDvriy4p4XxrrB0Jo9GExB0+bNtanvjX/VX7mor6jR6rtmeOk3ZpJ46CZXRKWrx4MTK6fkrB8S" +
           "n3AqTnkVuAFEgO0qU1Xw8ngbVpO462o6ZjyCGMu/RB3tOfUZr+03t5B5+/kAIhq7/g8/rTrqhEv3" +
           "YDAFCx+889qiWyZfahVihx2fL598haw7ebRRmzbshCgEBgyj+rY/Eui/F/8UVVp+eTmt918HRvlX" +
           "hOqGexWX3q4bvn2kg582nZW1awc9vuhL4Whs1R4IpqC1ubnXhm8d/mp45cK9cEK29/0v+22P3Elq" +
           "xsMBhJ3Ssj7U/OYVwv0GsTvU99h03nGkXnsKEXabqqNVV96b75z9vCRf+kPAEWi5+P4fjvzfs2/e" +
           "Ay+rC96f9fzYPqsX11mF2EGH+yYal9TMJ4wCKJ9ILAQmXbXbsWSyeVLpPGUX3ULm3Tfxk43sNrG0" +
           "/eE+Uz7pMk29/Li1Nmeyj917QsexJ9xbVFzcDmDoFgWe5wWysx7/mvq+o1Y0NuEUOp6bpjaXEgV2" +
           "Nuke/Sg6+n8B8H3LklWNzJq7gtXrW7BW6UpzopN7fj+X+6bNZdqCNuKnnof6oOzmqEr2w/cc9fMa" +
           "2OsAtQoVq947YPVfFu/XzStMQWtTU1WPJXNHWwWnR28bHjZKWu+9AUVFlE+mkDxoPEXxCNYq055f" +
           "yKamJGNHD0REUFVA2JlgwOGbJxxMLBKkrSNDONWTjkfvxG/dwO6yXobO2TMl+sVjNPPBO+pmM+FV" +
           "s18cP3T0597oxmAKNqxYtm9R07oaayG0/0HqNW4mt26Vg4LyycSD7N6jcIFM3iMWDTH5lKEEXId/" +
           "RFEsxN+VFkfQWDXxcceReHQqGHaPqnS+NctUXnyzlUBIfS8jzvzXxnieF3ZdN+PSLQo6PlhwcMxa" +
           "Y30IH/h5Mu+/o9bLsCu58l4AhIMuR4/ZG9cx/LNS6RwbGzuorSkjfuTxtP7hLsBntwjkNq0T9TxM" +
           "RV/1Ni2jdPUH+3q5XNFfgzF0hwLHXfmXA3wFcRwN7zuC9HvviKqC0uXkjYsCIrItlpa2TmbM/pCV" +
           "a5tR1a5DTWWZ+MNHuPTWGbwwZxnBQfvi9hwAym6PptvFb20kWDsQtRBNbO6ZSyX7dNcjqUA1HG9a" +
           "308VJF6qblVvydUvQa2KCjtlFGwqScazRAMOAIn2NOdc9kfqN7Ry8jEHcvyRQ6mrKWdn1m5KsHJd" +
           "C9Fw4G97oKMO+SrBQUPIbVgBwu5RJbP8Qwn03UvVn4FR39H21kFUVi0wdIeCYDjRWKkKpqiHqlr1" +
           "WpsEdvGfDLgNa2nPeADbVpctEeD7lufnLGXpqka6MnhAJRMnDKdf7zLO/NpIxA0QqKlF7XZ/a+uA" +
           "bB0UdGcrjKrkN9QT6N0fFVEVcFJt3bXCFKiq6zdtKlYFJxoDL49NZ1GlawLRVYtozfhUFwFA76pi" +
           "vvyFvXnpjWVUlcU4aP8auuI6hovPOQxVRUQAMOE4WFC2MmEI9YaiUUJ0X0F9yKyGxIuW3AZA+DgF" +
           "v61ZnPJKRQEFL9FS3k3BFAjq4uWCqkAoiFormvdF6ZoKRFcupjnt8XfhUIDLJx3BN48/mMqyGPFY" +
           "iF1jWyyqis21E6iGyF5CdD8hMkQI9gYJCFgAiB6oaN7Q8LAFYQeay6iJRFQFVMHx8+HuC6ZAsCoA" +
           "iICqKICyS6H1S9mcaEf7Fm1bIYJBl9qacrqm4DWguTWgafDbIL8O0u9R/qWn6HGEgxMTAFC2soAB" +
           "P6G0zrS0PKEggPIxqqBWQURQUO3mE3cF4uG6nirYnAeOYzGOURB2wSTb8NavJrNPLyIBh11jayTN" +
           "v0TbHgevETQHeKAWALcYQEDZSkBEyayDtlmWttlKvpGthE8WDInN5nRbLMZ43RdMgS/hWEqh3E+m" +
           "RNygEgqqtrNrCsFlC2g79OBdB6OKpl5G10+C7CpAQYRtRPgYB/x2JTlfScxSUksUzW4XirIDtWDi" +
           "ZeolWrEWACQUaeuuYApEck5JeTNKX789gRhHnJJS8pvXIkKX1ED0w3m0ZM+muoguaXYxWj8R/CYQ" +
           "AQSskmsCJw5OVEDA71BSi5S217b+9FOg2/ekXUcc6NmX/MZ1YFUQcGJFm7ormAIh41b1Wm+VAzXZ" +
           "gteR0GDNYNJL39cthF0IL1tIUzIPFXStcy74jSAGAFWl/lpLxzuKBMCJAgb8JKgHOHyMKv8QMUZD" +
           "g4aQnPMiKoCIOqU9VnZbMAWSD9UN+QDlWJvJSeYv7xMeOpzEzD8h7Fpw43Kam5rw+xXjGGGnIsPB" +
           "REHTgGDTkF6tqANY8JJsgwEUAJSPPL0EULoWjGmgujfp5R8KgImVtG0JZhWAoVsUlIz/2jtqRUGl" +
           "8903NDb8EMSEUNjlmM40/pplpHIeXZHwUKTHZMAFwIkJ1acZghWAgNqPjAIGnDhE66DHl4Wacw0D" +
           "LjGE+8FOP7VQcCur1cSKNbe+XhSIjfjCMhONd+cepiBYO/hdU1TW6idbyjvemWuqzv2JBqr62OzG" +
           "FQ67oh7BD9+l/YjDKA4H2CkJID0vJ1OfQJvvI1QjlI8zFB0sZJYr2U3gd4I44JZAsEoI9gS3FCQo" +
           "CEpmDXgZ2PnLftkS+xc0/eH7+Ml2wUB05Ji54jipbgymwEQi6yNDhi1Mvv3KYdk1SyW3ZqUWjz3G" +
           "Njw81QgqdEFVCS9ZQFPGUlNC10yUxBt9aLjXEttHKB4txIcKsf3lb+GgoApYthLAQm6j0vqK0vSs" +
           "Jd8CIjuPsnjcMdoy7TeiqBjj+LERh7wIaDcGUyCO27klkGc7tgSDlzctT/7eVpx8Ng2/uwfVHLsS" +
           "Wv0+ifYUWhVBROiKWh8vBe3v6t/GhCHYE6IDhUidEKoGEwIvCZl6SP1F6Vyh+B2AbB1lRyiEB+zl" +
           "B/v0p+PtOQaBQJ8BqyN77/c2QDcHU1AybsLTm35184Vec0NVYsbjUn3uj6Ro9OFe++szAghdcho3" +
           "0LlpI7naHoRcoStueSXKNvgZSK+GzlWKiO74ASMg0vV7LwCqRstPPlsTzz2Gl2wTMVB82DHPumXl" +
           "mwvXfewB6vvO6h+c/mDLE787Ra1or8mXeMWHHcmHJx3uiPiGLqgE2XTlg3z+xK9THg3SlbZZM1h+" +
           "1gTApzsFq+u8QQ8+ydKTxomX2OSYaFHH4N++OD42YvTcPbDCFIjj+JWnn3tX2ysvTMgnmoo3P3CH" +
           "6XHyWfT46kS/6YmHBFTYCdEcgSXvksh+lfIoXQrVDsKUVOIlNrGdrhaRrlmjvS66yjb+7n7JNW9y" +
           "cUR7njFlRmz4qPl78H6YgtiBo96s/t4lz6iKesmEs/6Gy2yvC66QQGU/q12djbEQWrqI5lSOXa8E" +
           "fQgP2ptP+n1N8SCpoPPPnbBT0dIj/icfrhssmx+611GBQGXftupvnX8bIvk9G0xhlfGqTv/2jZEB" +
           "+zQAND89zU0teFv7Xn6TlUDUdtEMwbVLaG9N4FslmW+gKbOGjN+5wzFNE45QPGY8WFAAC4niEHdM" +
           "GMjJU0bw4Ji+GPsP9qIQqq6zfS6+Rtb85HzRXMqAY/v+6PpH3PKKN9mOc+WVV9K9CiQQ3Bzdd1iw" +
           "afrDX1LNO8m359LzrO+pW1yh7W+/blAr7AjJWzoOPZaaAX2Yu/lWHls1ldc2z2VjOklJsILiQBwR" +
           "wVefXDRAy1N/gnyWv4yu4s4zhzCztox2DAIctaABlF1y4mW29md32y2bdJqfneYCUnzI4cv6XnrD" +
           "d8SYxKd1e0OBaqz+yose23j/z8cBFA3/gjfw9l/Lxjt+rg2P/soFX9iBQ+OP7mTUWWeyoOkaXtv0" +
           "KqtTsDxpSfoVfLn34YzoU8bsxnksb23EeWMxxwRyvDGigqVJWJ5U2vLQvznNA3cuIJLz6YqEiuyA" +
           "a27x1fOov+J8x+bTxo2Xdw6btfDUYK8+j32aN1AViKT6/eS6ye1zXn45tWR+Tce7r7v1V/zQ73/N" +
           "L0R9z2+Y9oCzQzTWx/1wEa1pH8SwlWDE0JBp5oHVv2eB+jQnhdaUoWNQnIE1LmQUUP4uHzDkHEOY" +
           "nQSjYCJFtt9lN/kmFmflxZMdm0sbxbGDpj50+5ZYngT49IMpPJqW7TP9pVPf/fy+T3qJTcUtM59y" +
           "FPEGXHuLOOUV3oZ7fuGieeEjgsvfo7WjE9cN8FECOI5gEEQEgJyFVF7ZnhXBIqiyA1UIlFb5tdff" +
           "ZlFY+aMpjt/ebFSh/yU/nV467pgrAf/fdItmgVtS9uqwF98620TK0mCl5aUn3OWTT6dq4tky8Of3" +
           "eSZSZlXZJrC+nmRTC0aibE/4OFVFAWv4GMcqxirbUysaG3yAN+S3T2i+sYHlF37H8doajSr0Ovv7" +
           "s/qce+E5QPbffO1qQah33+kH/nnhaYHKfq2qKm3vvOYu/to43LIKhr0415aOOTpvNaBWwSSayNav" +
           "QrR0hzhcP86g6H4MjNUyuuJArjrwO9w06hGOesWl3+oOgr5iBEpSecJZH2vZOiqKG7N9Jl3k7f2b" +
           "P7Hp/+7RlZed7/rpdqM4ts+5lz5be+2txyHS/hm62Lkg39x05AenfOWejoVv9hdUkIBWTzzHqznv" +
           "YumYN1fX//JnJvXBItNy7k8lftpgZm28iRVJZXM2yoiKcXx3yERqi3qxvaY/Pcqyb09kc0WQRf3i" +
           "lKY8Rq5IYBF1wnFKDxtva6ZcaHONTdRffZF0Ll/iYsAEI/m6a29/qPq0b56/LZbPVjAFNpMeuvrK" +
           "i2/f+ODdY9TmHXwI1dT6vSedpz3GHyvJhfN1VUMSjhljFrb/UuLBfeRzPY+hX7w/O2PzORYePYbk" +
           "orcQFRXXJVBdo+Vjj7QVx5+MuAHZcPdt2vTsYw54gkKopq55yN2/vano4M/dBmQBPqvBFKiWtc56" +
           "4YJlF3x3Unb96nIEUKOR2sG28usnafmErxOoHUwwGkLEiCDCNgg70paXnmPNjVdr0fCRWjJmLOEB" +
           "daRXraDxj7+j9dUXjc2kBFTEuH7VSWfOrbvqpkvc0rI/Awrw2Q+mwPgdHaPX3X3rj9dNvfEom0kF" +
           "VAEVdYvLtGjoAVo85ggtGf05CfcbqMGqKjGhMB9pRwEBUN/Ha23R9OrlZFatlMRrL2v73NclXb/C" +
           "qJ8XMQCyJaZD1g687hdTi0aMvh+Rlv/AL9gq0Hw+3PbWnMPX3n7jlLY5s8baXDYEgIIiagIh3NIe" +
           "Gqqq1EBVb9zyCtxoXDFGbT5n/PaE5ho2mtzmjeSbW/A720R9X8SwTbimf33Pb5zxUO9vTv5VoKKq" +
           "/r/gK/wKbDYTTi1eNHTzH393SvPzT0/IrF5Zp2KNCFtpF8cqBba/ndVEYqmKCcfP6Xn8xEeLRx78" +
           "rFtS2oCIAvx3BVMgms/H8q3N+zc9/cTYphlPf/6vIWU3ru+jnufySUTULSpujwzca9mWPcy8skMP" +
           "e6Xkc4fODlb32iyOk6cb/T/N+faHj8AX2gAAAABJRU5ErkJggg=="
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
  }
  else {
    logoElt.parentNode.hidden = true;
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
