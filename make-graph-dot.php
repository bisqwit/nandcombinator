<?php
class Decoder
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
      $p = strpos('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/', $c);
      $this->cache_data >>= 6;
      $this->cache_data += $p;
      #$this->cache_data += ($p << $this->cache_bits);
      $this->cache_bits += 6;
    }
    $result = $this->cache_data & ((1 << $n)-1);
    $this->cache_data >>= $n;
    $this->cache_bits -= $n;
    return $result;
  }
};

$num_inputs  = 2;
$num_outputs = 1;
#$input = '4 G ABBCACDEH';
#$input = '4 B BBAACDEEH';
$input = '5 J BBABADCEDFI';

$tokens = explode(' ', $input);
$num_gates   = (int)$tokens[0];
$truthtable  = new Decoder($tokens[1]);
$connections = new Decoder($tokens[2]);

// Parse connections
$gate_from    = Array();
$out_from     = Array();

for($n=0; $n<$num_gates*2; ++$n)
{
  $code = $connections->Get(6);
  if($code < $num_inputs) { $gate_from[$n>>1][] = 'i'.($code); }
  else                    { $gate_from[$n>>1][] = 'g'.($code-$num_inputs); }
}
for($n=0; $n<$num_outputs; ++$n)
{
  $code = $connections->Get(5);
  $out_from[$n] = "g$code";
}

$used_sources = Array();
foreach($gate_from as $n=>$inputs) foreach($inputs as $i) $used_sources[$i] = $i;
foreach($out_from as $i) $used_sources[$i] = $i;

ob_start();

print <<<EOF
digraph {
rankdir=LR;
node[fontsize=10, fontname="Helvetica", shape=rect];
edge[fontsize=10, fontname="Helvetica"];

EOF;

// Print nodes
for($i=0; $i<$num_inputs; ++$i)
{
  if(isset($used_sources["i{$i}"])) print "  x$i [shape=square];\n";
}
if(isset($used_sources['0'])) print "  zero [shape=square];\n";
if(isset($used_sources['1'])) print "  one [shape=square];\n";

// Print gates
for($i=0; $i<$num_gates; ++$i) print "  g$i;\n";

// Print connections
foreach($gate_from as $n => $inputs)
{
  foreach($inputs as $input_no => $input)
  {
    $num = (int)substr($input,1);

    if($input[0] == '0')     print "  zero->g{$n};\n";
    elseif($input[0] == '1') print "  one->g{$n};\n";
    elseif($input[0] == 'i') print "  x{$num}->g{$n};\n";
    else                     print "  g{$num}->g{$n};\n";

    if(isset($not_gates[$n])) break; // For NOT gates, only print one input
  }
}
foreach($out_from as $n=>$input)
{
  $num = (int)substr($input,1);

  print "f{$n} [shape=point];\n";
  if($input[0] == '0')     print "  zero->f{$n};\n";
  elseif($input[0] == '1') print "  one->f{$n};\n";
  elseif($input[0] == 'i') print "  x{$num}->f{$n};\n";
  else                     print "  g{$num}->f{$n};\n";
}
print <<<EOF

}

EOF;
