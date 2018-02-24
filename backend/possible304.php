<?php

/*** Bisqwit's 304-reply giver
 *   Version 1.1.0
 *   Copyright (C) 1992,2003 Bisqwit (http://bisqwit.iki.fi/)
 *
 * The purpose of this PHP file is to give a HTTP 304-reply when possible.
 *
 * HTTP 304 -reply tells the browser "the resource has not been changed
 * since you last saw it", and effectively saves bandwidth.
 *
 * Include:
 *   include_once 'possible304.php';
 *
 * Usage:
 *   Check304(<filenamearray>, [etag]);
 *
 * filenamearray is an array construct containing
 * all the filenames which this resource depends on.
 * If any of them has been changed since the browser last cached
 * the resource, the 304-reply is not given.
 *
 * etag is optional. Use it if the same URI refers to multiple
 * resources (by HTTP GET parameters for example). The tag is
 * just a string identifying the resource.
 *
 * An example of usage:
 *   Check304(Array('kifu.php', 'functions.php'));
 *   // 304 check done, now produce something big.
 *   Readfile('kifu.dat');
 *
 */

function Check304($filenames, $etag='BTQ-Etag')
{
  global $CHECK304_MAX_AGE;
  
  $pagetime = 0;
  foreach($filenames as $fn)
  {
    $t = @filemtime($fn);
    if($t > $pagetime)$pagetime=$t;
  }

  if(isset($CHECK304_MAX_AGE))
  {
    $stamp = time() - $CHECK304_MAX_AGE;
    if($pagetime < $stamp) $pagetime = $stamp;
  }

  if(isset($_SERVER['HTTP_IF_MODIFIED_SINCE']))
  {
    if(strtotime($_SERVER['HTTP_IF_MODIFIED_SINCE']) >= $pagetime)
    {
      header('HTTP/1.0 304 Not modified indeed');
      print 'You should use the version you have cached. ';
      print 'Why I am talking English with a web browser?';
      exit;
    }
  }
  header('Last-Modified: '.gmdate('D, d M Y H:i:s', $pagetime).' GMT');
  header('Expires: Tue, Jan 19 2038 05:00:00 GMT');
  header('ETag: '.$etag);
}
