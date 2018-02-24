<?php

define('BASE64_CSET', 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/');

class BASE64decoder
{
  var $data;
  var $cache_data,$cache_bits, $pos;
  
  function __construct($s) { $this->data = $s; $this->cache_data=0; $this->cache_bits=0; $this->pos=0; }
  function CountBits()
  {
    return strlen($data)*6;
  }
  function Get($n)
  {
    while($this->cache_bits < $n)
    {
      $c = $this->data[$this->pos++];
      $p = strpos(BASE64_CSET, $c);
      #$this->cache_data <<= 6;
      #$this->cache_data += $p;
      $this->cache_data += ($p << $this->cache_bits);
      $this->cache_bits += 6;
    }
    $result = $this->cache_data & ((1 << $n)-1);
    $this->cache_data >>= $n;
    $this->cache_bits -= $n;
    return $result;
  }
};

class BASE64encoder
{
  var $data;
  var $cache_data,$cache_bits;
  function __construct() { $this->cache_data=0; $this->cache_bits=0; $this->data=''; }
  function Put($value, $n)
  {
    $this->cache_data |= $value << $this->cache_bits;
    $this->cache_bits += $n;
    while($this->cache_bits >= 6)
    {
      $this->data .= BASE64_CSET[$this->cache_data & 63 ];
      $this->cache_data >>= 6;
      $this->cache_bits -= 6;
    }
  }
  function Get()
  {
    if($this->cache_bits)
    {
      $this->Put(0, 6 - ($this->cache_bits % 6));
    }
    return $this->data;
  }
};

