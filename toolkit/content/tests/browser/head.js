"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");

/**
 * A wrapper for the findbar's method "close", which is not synchronous
 * because of animation.
 */
function closeFindbarAndWait(findbar) {
  return new Promise((resolve) => {
    if (findbar.hidden) {
      resolve();
      return;
    }
    findbar.addEventListener("transitionend", function cont(aEvent) {
      if (aEvent.propertyName != "visibility") {
        return;
      }
      findbar.removeEventListener("transitionend", cont);
      resolve();
    });
    findbar.close();
  });
}

function pushPrefs(...aPrefs) {
  let deferred = Promise.defer();
  SpecialPowers.pushPrefEnv({"set": aPrefs}, deferred.resolve);
  return deferred.promise;
}

/**
 * Helper class for testing datetime input picker widget
 */
class DateTimeTestHelper {
  constructor() {
    this.panel = document.getElementById("DateTimePickerPanel");
    this.panel.setAttribute("animate", false);
    this.tab = null;
    this.frame = null;
  }

  /**
   * Opens a new tab with the URL of the test page, and make sure the picker is
   * ready for testing.
   *
   * @param  {String} pageUrl
   */
  async openPicker(pageUrl) {
    this.tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);
    await BrowserTestUtils.synthesizeMouseAtCenter("input", {}, gBrowser.selectedBrowser);
    // If dateTimePopupFrame doesn't exist yet, wait for the binding to be attached
    if (!this.panel.dateTimePopupFrame) {
      await BrowserTestUtils.waitForEvent(this.panel, "DateTimePickerBindingReady")
    }
    this.frame = this.panel.dateTimePopupFrame;
    await this.waitForPickerReady();
  }

  async waitForPickerReady() {
    await BrowserTestUtils.waitForEvent(this.frame, "load", true);
    // Wait for picker elements to be ready
    await BrowserTestUtils.waitForEvent(this.frame.contentDocument, "PickerReady");
  }

  /**
   * Find an element on the picker.
   *
   * @param  {String} selector
   * @return {DOMElement}
   */
  getElement(selector) {
    return this.frame.contentDocument.querySelector(selector);
  }

  /**
   * Find the children of an element on the picker.
   *
   * @param  {String} selector
   * @return {Array<DOMElement>}
   */
  getChildren(selector) {
    return Array.from(this.getElement(selector).children);
  }

  /**
   * Click on an element
   *
   * @param  {DOMElement} element
   */
  click(element) {
    EventUtils.synthesizeMouseAtCenter(element, {}, this.frame.contentWindow);
  }

  /**
   * Close the panel and the tab
   */
  async tearDown() {
    if (!this.panel.hidden) {
      let pickerClosePromise = new Promise(resolve => {
        this.panel.addEventListener("popuphidden", resolve, {once: true});
      });
      this.panel.hidePopup();
      this.panel.closePicker();
      await pickerClosePromise;
    }
    await BrowserTestUtils.removeTab(this.tab);
    this.tab = null;
  }

  /**
   * Clean up after tests. Remove the frame to prevent leak.
   */
  cleanup() {
    this.frame.remove();
    this.frame = null;
    this.panel.removeAttribute("animate");
    this.panel = null;
  }
}
