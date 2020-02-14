#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * This singleton represents the grid that contains all sites.
 */
var gGrid = {
  /**
   * The DOM node of the grid.
   */
  _node: null,
  _gridDefaultContent: null,
  get node() { return this._node; },

  /**
   * The cached DOM fragment for sites.
   */
  _siteFragment: null,

  /**
   * All cells contained in the grid.
   */
  _cells: [],
  get cells() { return this._cells; },

  /**
   * All sites contained in the grid's cells. Sites may be empty.
   */
  get sites() { 
    // return [for (cell of this.cells) cell.site];
    let aSites = [];
    for (let cell of this.cells) {
      aSites.push(cell.site);
    }
    return aSites;
  },

  // Tells whether the grid has already been initialized.
  get ready() { return !!this._ready; },

  // Returns whether the page has finished loading yet.
  get isDocumentLoaded() { return document.readyState == "complete"; },

  /**
   * Initializes the grid.
   * @param aSelector The query selector of the grid.
   */
  init: function() {
    this._node = document.getElementById("newtab-grid");
    this._gridDefaultContent = this._node.lastChild;
    this._createSiteFragment();

    gLinks.populateCache(() => {
      this._refreshGrid();
      this._ready = true;
    });
  },

  /**
   * Creates a new site in the grid.
   * @param aLink The new site's link.
   * @param aCell The cell that will contain the new site.
   * @return The newly created site.
   */
  createSite: function(aLink, aCell) {
    let node = aCell.node;
    node.appendChild(this._siteFragment.cloneNode(true));
    return new Site(node.firstElementChild, aLink);
  },

  /**
   * Handles all grid events.
   */
  handleEvent: function(aEvent) {
    // Any specific events should go here.
  },

  /**
   * Locks the grid to block all pointer events.
   */
  lock: function() {
    this.node.setAttribute("locked", "true");
  },

  /**
   * Unlocks the grid to allow all pointer events.
   */
  unlock: function() {
    this.node.removeAttribute("locked");
  },

  /**
   * Renders the grid.
   */
  refresh() {
    this._refreshGrid();
  },

  /**
   * Renders the grid, including cells and sites.
   */
  _refreshGrid() {
    let row = document.createElementNS(HTML_NAMESPACE, "div");
    row.classList.add("newtab-row");
    let cell = document.createElementNS(HTML_NAMESPACE, "div");
    cell.classList.add("newtab-cell");

    // Clear the grid
    this._node.innerHTML = "";

    // Creates the structure of one row
    for (let i = 0; i < gGridPrefs.gridColumns; i++) {
      row.appendChild(cell.cloneNode(true));
    }

    // Creates the grid
    for (let j = 0; j < gGridPrefs.gridRows; j++) {
      this._node.appendChild(row.cloneNode(true));
    }      

    // Create cell array.
    let cellElements = this.node.querySelectorAll(".newtab-cell");
    let cells = Array.from(cellElements, (cell) => new Cell(this, cell));

    // Fetch links.
    let links = gLinks.getLinks();

    // Create sites.
    let numLinks = Math.min(links.length, cells.length);
    for (let i = 0; i < numLinks; i++) {
      if (links[i]) {
        this.createSite(links[i], cells[i]);
      }
    }

    this._cells = cells;
  },

  /**
   * Creates the DOM fragment that is re-used when creating sites.
   */
  _createSiteFragment: function() {
    let site = document.createElementNS(HTML_NAMESPACE, "div");
    site.classList.add("newtab-site");
    site.setAttribute("draggable", "true");

    // Create the site's inner HTML code.
    site.innerHTML =
      '<a class="newtab-link">' +
      '  <span class="newtab-thumbnail placeholder"/>' +
      '  <span class="newtab-thumbnail thumbnail"/>' +
      '  <span class="newtab-title"/>' +
      '</a>' +
      '<input type="button" title="' + newTabString("pin") + '"' +
      '       class="newtab-control newtab-control-pin"/>' +
      '<input type="button" title="' + newTabString("block") + '"' +
      '       class="newtab-control newtab-control-block"/>';

    this._siteFragment = document.createDocumentFragment();
    this._siteFragment.appendChild(site);
  },

  /**
   * Test a tile at a given position for being pinned or history
   * @param position Position in sites array
   */
  _isHistoricalTile: function(aPos) {
    let site = this.sites[aPos];
    return site && (site.isPinned() || site.link && site.link.type == "history");
  }

};
