/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


[Exposed=Worker]
interface WorkerNavigator {
};

WorkerNavigator includes NavigatorID;
WorkerNavigator includes NavigatorLanguage;
WorkerNavigator includes NavigatorOnLine;
WorkerNavigator includes NavigatorConcurrentHardware;
WorkerNavigator includes NavigatorStorage;
WorkerNavigator includes NavigatorGlobalPrivacyControl;
