/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'experimental',
  'engines': {
    'Palemoon': '> 27',
    'Firefox': '> 28'
  }
};

const { Cu } = require('chrome');
const { on, off, emit } = require('../../event/core');

const { data } = require('sdk/self');

const { isObject, isNil } = require('../../lang/type');

const { getMostRecentBrowserWindow } = require('../../window/utils');
const { ignoreWindow } = require('../../private-browsing/utils');
#ifdef MC_PALEMOON
const { buttons } = require('../buttons');
#else
const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
const { AREA_PANEL, AREA_NAVBAR } = CustomizableUI;
#endif

const { events: viewEvents } = require('./view/events');

const XUL_NS = 'http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul';

const views = new Map();
#ifndef MC_PALEMOON
const customizedWindows = new WeakMap();

const buttonListener = {
  onCustomizeStart: window => {
    for (let [id, view] of views) {
      setIcon(id, window, view.icon);
      setLabel(id, window, view.label);
    }

    customizedWindows.set(window, true);
  },
  onCustomizeEnd: window => {
    customizedWindows.delete(window);

    for (let [id, ] of views) {
      let placement = CustomizableUI.getPlacementOfWidget(id);

      if (placement)
        emit(viewEvents, 'data', { type: 'update', target: id, window: window });
    }
  },
  onWidgetAfterDOMChange: (node, nextNode, container) => {
    let { id } = node;
    let view = views.get(id);
    let window = node.ownerDocument.defaultView;

    if (view) {
      emit(viewEvents, 'data', { type: 'update', target: id, window: window });
    }
  }
};

CustomizableUI.addListener(buttonListener);

require('../../system/unload').when( _ =>
  CustomizableUI.removeListener(buttonListener)
);
#endif

function getNode(id, window) {
  return !views.has(id) || ignoreWindow(window)
    ? null
#ifdef MC_PALEMOON
    : buttons.getNode(id, window);
#else
    : CustomizableUI.getWidget(id).forWindow(window).node
#endif
};

#ifndef MC_PALEMOON
function isInToolbar(id) {
  let placement = CustomizableUI.getPlacementOfWidget(id);

  return placement && CustomizableUI.getAreaType(placement.area) === 'toolbar';
}
#endif

function getImage(icon, isInToolbar, pixelRatio) {
  let targetSize = (isInToolbar ? 18 : 32) * pixelRatio;
  let bestSize = 0;
  let image = icon;

  if (isObject(icon)) {
    for (let size of Object.keys(icon)) {
      size = +size;
      let offset = targetSize - size;

      if (offset === 0) {
        bestSize = size;
        break;
      }

      let delta = Math.abs(offset) - Math.abs(targetSize - bestSize);

      if (delta < 0)
        bestSize = size;
    }

    image = icon[bestSize];
  }

  if (image.indexOf('./') === 0)
    return data.url(image.substr(2));

  return image;
}

function nodeFor(id, window=getMostRecentBrowserWindow()) {
#ifdef MC_PALEMOON
  return getNode(id, window);
#else
  return customizedWindows.has(window) ? null : getNode(id, window);
#endif
};
exports.nodeFor = nodeFor;

function create(options) {
  let { id, label, icon, type, badge } = options;

  if (views.has(id))
    throw new Error('The ID "' + id + '" seems already used.');

#ifdef MC_PALEMOON
  buttons.createButton({
#else
  CustomizableUI.createWidget({
#endif
    id: id,
    type: 'custom',
#ifdef MC_PALEMOON

    onBuild: function(document, _id) {
#else
    removable: true,
    defaultArea: AREA_NAVBAR,
    allowedAreas: [ AREA_PANEL, AREA_NAVBAR ],

    onBuild: function(document) {
#endif
      let window = document.defaultView;

      let node = document.createElementNS(XUL_NS, 'toolbarbutton');

      let image = getImage(icon, true, window.devicePixelRatio);

      if (ignoreWindow(window))
        node.style.display = 'none';

#ifdef MC_PALEMOON
      node.setAttribute('id', _id);
#else
      node.setAttribute('id', this.id);
#endif
      node.setAttribute('class', 'toolbarbutton-1 chromeclass-toolbar-additional badged-button');
      node.setAttribute('type', type);
      node.setAttribute('label', label);
      node.setAttribute('tooltiptext', label);
      node.setAttribute('image', image);
#ifdef MC_PALEMOON
      node.setAttribute('pmkit-button', 'true');
#else
      node.setAttribute('constrain-size', 'true');
#endif

      views.set(id, {
#ifndef MC_PALEMOON
        area: this.currentArea,
#endif
        icon: icon,
        label: label
      });

      node.addEventListener('command', function(event) {
        if (views.has(id)) {
          emit(viewEvents, 'data', {
            type: 'click',
            target: id,
            window: event.view,
            checked: node.checked
          });
        }
      });

      return node;
    }
  });
};
exports.create = create;

function dispose(id) {
  if (!views.has(id)) return;

  views.delete(id);
#ifdef MC_PALEMOON
  buttons.destroyButton(id);
#else
  CustomizableUI.destroyWidget(id);
#endif
}
exports.dispose = dispose;

function setIcon(id, window, icon) {
  let node = getNode(id, window);

  if (node) {
#ifdef MC_PALEMOON
    let image = getImage(icon, true, window.devicePixelRatio);
#else
    icon = customizedWindows.has(window) ? views.get(id).icon : icon;
    let image = getImage(icon, isInToolbar(id), window.devicePixelRatio);
#endif

    node.setAttribute('image', image);
  }
}
exports.setIcon = setIcon;

function setLabel(id, window, label) {
  let node = nodeFor(id, window);

  if (node) {
    node.setAttribute('label', label);
    node.setAttribute('tooltiptext', label);
  }
}
exports.setLabel = setLabel;

function setDisabled(id, window, disabled) {
  let node = nodeFor(id, window);

  if (node)
    node.disabled = disabled;
}
exports.setDisabled = setDisabled;

function setChecked(id, window, checked) {
  let node = nodeFor(id, window);

  if (node)
    node.checked = checked;
}
exports.setChecked = setChecked;

function setBadge(id, window, badge, color) {
  let node = nodeFor(id, window);

  if (node) {
    // `Array.from` is needed to handle unicode symbol properly:
    // '𝐀𝐁'.length is 4 where Array.from('𝐀𝐁').length is 2
    let text = isNil(badge)
                  ? ''
                  : Array.from(String(badge)).slice(0, 4).join('');

    node.setAttribute('badge', text);

    let badgeNode = node.ownerDocument.getAnonymousElementByAttribute(node,
                                        'class', 'toolbarbutton-badge');

    if (badgeNode)
      badgeNode.style.backgroundColor = isNil(color) ? '' : color;
  }
}
exports.setBadge = setBadge;

function click(id) {
  let node = nodeFor(id);

  if (node)
    node.click();
}
exports.click = click;
