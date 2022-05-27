/* Copyright 2013 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* jshint esnext:true, maxlen: 100 */
/* globals Components, Services */

'use strict';

this.EXPORTED_SYMBOLS = ['PdfJsTelemetry'];

this.PdfJsTelemetry = {
  onViewerIsUsed: function () {
  },
  onFallback: function () {
  },
  onDocumentSize: function (size) {
  },
  onDocumentVersion: function (versionId) {
  },
  onDocumentGenerator: function (generatorId) {
  },
  onEmbed: function (isObject) {
  },
  onFontType: function (fontTypeId) {
  },
  onForm: function (isAcroform) {
  },
  onPrint: function () {
  },
  onStreamType: function (streamTypeId) {
  },
  onTimeToView: function (ms) {
  }
};
