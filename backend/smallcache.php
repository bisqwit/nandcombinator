<?php
/*** Bisqwit's subcaching class
 *   Copyright (C) 1992,2006 Bisqwit (http://bisqwit.iki.fi/)
 *   Now using Memcached
 *
 * Usage example:
 *   $version = 1;
 *   $s = sprintf('%08X%08X', crc32($parameter1), crc32($parameter2));
 *   $c = new Cache($s, $version);
 *   if(!$c->DumpOrLog())
 *   {
 *     .. here is the side-effectless function to be cached ..
 *   }
 *   $c->_Cache();
 *   unset($c);
 *
 */

if(function_exists('GetCacheObj')) return;
else
{

function GetMemCacheObj()
{
  static $obj;
  if(!$obj)
  {
    $obj = new Memcache;
    $obj->addserver('127.0.0.1',   11211, true, 50);
    #$obj->addserver('10.104.4.83', 11211, true, 10);
    #$obj->addserver('10.104.4.85', 11211, true,  1);
    #$obj->setCompressThreshold(20000, 0.2);
  }
  return $obj;
}

class APC_Cache
{
  public function add($key, $var, $flag, $expire)
  {
    if(!apc_fetch($key))
      apc_store($key, $var, $expire);
  }
  public function set($key, $var, $flag, $expire)
  {
    apc_store($key, $var, $expire);
  }
  public function delete($key)
  {
    apc_delete($key);
  }
  public function get($key)
  {
    return apc_fetch($key);
  }
};

function GetCacheObj()
{
  if(function_exists('apc_fetch'))
  {
    static $obj;
    if(!$obj) { $obj = new APC_Cache(); }
    return $obj;
  }
  return GetMemCacheObj();
}

function CacheDelete($key)
{
  $c = GetCacheObj();
  $c->delete($key);
}

define('SMALLCACHE_IDLE', 0);
define('SMALLCACHE_SAVE_PRINT', 1); // DumpOrLog() + _Cache()
define('SMALLCACHE_SAVE_DATA',  2); // LoadOrLog() + Save()
define('SMALLCACHE_SAVE_HIDE',  3); // LoadOrLog() + _Cache()

class Cache
{
  private $key;
  private $ver;
  private $save;
  private $maxage;
  private $obj;
  
  function Cache($key, $initversion=0, $maxage = 86400)
  {
    $this->key    = $key;
    $this->ver    = $initversion;
    $this->save   = SMALLCACHE_IDLE;
    $this->maxage = $maxage;
    $this->obj = GetCacheObj();
  }
  function _Cache()
  {
    if(!$this->save) return;
    
    $data = ob_get_contents();
    if($this->save == SMALLCACHE_SAVE_DATA) $this->save = SMALLCACHE_SAVE_HIDE;
    $this->Save($data);
  }
  function DumpOrLog()
  {
    if($this->save) return;
    
    $content = $this->obj->get($this->key);
    if($content === false
    || ($this->ver !== false && $content[0] != $this->ver))
    {
      ob_start();
      $this->save = SMALLCACHE_SAVE_PRINT;
      return false;
    }
    print $content[1];
    return true;
  }
  function LoadOrLog()
  {
    if($this->save) return;
    
    $content = $this->obj->get($this->key);
    if($content === false
    || ($this->ver !== false && $content[0] != $this->ver))
    {
      ob_start();
      $this->save = SMALLCACHE_SAVE_DATA;
      return false;
    }
    return $content[1];
  }
  function Save($data)
  {
    if($this->save == 0) return;
    
    $content = Array($this->ver, $data);
    
    if($this->save == SMALLCACHE_SAVE_HIDE)
      ob_end_clean();
    else
      ob_end_flush();
    
    $this->obj->add($this->key, $content, 0, time() + $this->maxage);
    $this->save = 0;
  }
  
  function Found() { return !$this->save; }
  
  function GetKey() { return $this->key; }
};

class BinKeyCache extends Cache
{
  function BinKeyCache($key, $initversion=0, $maxage = 86400)
  {
    return Cache::Cache(md5($key), $initversion, $maxage);
  }
};

}
