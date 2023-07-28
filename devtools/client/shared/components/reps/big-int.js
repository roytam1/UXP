/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const React = require("devtools/client/shared/vendor/react");

  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders a BigInt
   */
  const BigInt = React.createClass({
    displayName: "BigInt",

    propTypes: {
      object: React.PropTypes.object.isRequired
    },

    render: function () {
      let {object} = this.props;
      let {text} = object;

      return (
        span({className: "objectBox objectBox-number"},
          `${text}n`
        )
      );
    }
  });

  function supportsObject(object, type) {
    return (type == "BigInt");
  }

  // Exports from this module

  exports.BigInt = {
    rep: BigInt,
    supportsObject: supportsObject
  };
});
