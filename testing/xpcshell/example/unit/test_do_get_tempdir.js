/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* This tests that do_get_tempdir returns a directory that we can write to. */

var Ci = Components.interfaces;

function run_test() {
  let tmpd = do_get_tempdir();
  do_check_true(tmpd.exists());
  tmpd.append("testfile");
  tmpd.create(Ci.nsIFile.NORMAL_FILE_TYPE, 600);
  do_check_true(tmpd.exists());
}
