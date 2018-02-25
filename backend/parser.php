<?php

require_once 'base64.php';
require_once 'solver.php';

define('WOLFRAM_ALPHA', false);
#define('WOLFRAM_ALPHA', true);

define('NAME_REGEX', '[A-Za-z_][A-Za-z_0-9]*(?:\[[0-9]+\])?');

define('zero',     0);
define('one',      1);
define('opNOT',   10);
define('opAND',   11);
define('opOR',    12);
define('opXOR',   13);
define('opNAND',  14);
define('opNOR',   15);
define('opXNOR',  16);
define('opIMP',   17);
define('opPARENS',18);

class Parser
{
  var $phrases;
  var $variables;
  var $varnames;
  var $operations;
  var $interpreted_phrases;
  var $output_names;

  var $truthtable;
  var $modified_truthtable;
  
  var $num_inputs;
  var $num_outputs;
  var $unused_variables;
  var $input_mapping;
  var $output_mapping;
  
  var $key;
  var $logic_map;
  var $num_gates;
  var $parse_errors;
  
  function __construct()
  {
    $this->phrases    = Array();
    $this->variables  = Array();
    $this->truthtable = Array();
    $this->operations = Array();
  }
  
  // Parse: Input:  comma-delimited phrases
  //        Output: $phrases[], $operations[], $variables[]
  //                $interpreter_phrases[]
  function Parse($phrase)
  {
    $index = 0;
    foreach(preg_split('/[,;]+/', $phrase) as $sub_phrase)
    {
      $s = trim($sub_phrase);
      if(strlen($s))
       $this->ParseOne($s, $index++);
    }
  }

  function ParseOne($phrase, $index)
  {
    $name = "out{$index}";
    if(preg_match('/^\s*('.NAME_REGEX.')\s*:=\s*(.*)$/', $phrase, $mat))
    {
      $name   = $mat[1];
      $phrase = $mat[2];
    }
    $this->phrases[$index]      = $phrase;
    $this->output_names[$index] = $name;
    $this->operations[$index]   = $this->DoParse($phrase);

    $stack1 = Array();
    $stack2 = Array();
    $errors = 0;
    foreach($this->operations[$index] as $o)
      switch($o)
      {
        case zero:   $stack1[] = '0'; $stack2[] = '0'; break;
        case one:    $stack1[] = '1'; $stack2[] = '1'; break;
        case opNOT:  if(count($stack1) < 1) { ++$errors; break; }
                     $a = array_pop($stack1); $stack1[] = "¬$a";
                     $a = array_pop($stack2); $stack2[] = "NOT $a"; break;
        case opAND:  if(count($stack1) < 2) { ++$errors; break; }
                     $a = array_pop($stack1); $b = array_pop($stack1); $stack1[] = "({$b} ∧ $a)";
                     $a = array_pop($stack2); $b = array_pop($stack2); $stack2[] = "({$b} AND $a)"; break;
        case opNAND: if(count($stack1) < 2) { ++$errors; break; }
                     $a = array_pop($stack1); $b = array_pop($stack1); $stack1[] = "({$b} ∧̄ $a)";
                     $a = array_pop($stack2); $b = array_pop($stack2); $stack2[] = "({$b} NAND $a)"; break;
        case opOR:   if(count($stack1) < 2) { ++$errors; break; }
                     $a = array_pop($stack1); $b = array_pop($stack1); $stack1[] = "({$b} ∨ $a)";
                     $a = array_pop($stack2); $b = array_pop($stack2); $stack2[] = "({$b} OR $a)"; break;
        case opNOR:  if(count($stack1) < 2) { ++$errors; break; }
                     $a = array_pop($stack1); $b = array_pop($stack1); $stack1[] = "({$b} ∨̄ $a)";
                     $a = array_pop($stack2); $b = array_pop($stack2); $stack2[] = "({$b} NOR $a)"; break;
        case opXOR:  if(count($stack1) < 2) { ++$errors; break; }
                     $a = array_pop($stack1); $b = array_pop($stack1); $stack1[] = "({$b} ⊕ $a)";
                     $a = array_pop($stack2); $b = array_pop($stack2); $stack2[] = "({$b} XOR $a)"; break;
        case opXNOR: if(count($stack1) < 2) { ++$errors; break; }
                     $a = array_pop($stack1); $b = array_pop($stack1); $stack1[] = "({$b} ≡ $a)";
                     $a = array_pop($stack2); $b = array_pop($stack2); $stack2[] = "({$b} XNOR $a)"; break;
        case opIMP:  if(count($stack1) < 2) { ++$errors; break; }
                     $a = array_pop($stack1); $b = array_pop($stack1); $stack1[] = "({$b} → $a)";
                     $a = array_pop($stack2); $b = array_pop($stack2); $stack2[] = "({$b} IMP $a)"; break;
        default:     $stack1[] = $this->varnames[$o];
                     $stack2[] = $this->varnames[$o];
                     break;
      }
    if(count($stack1) != 1) { ++$errors; }
    if(empty($stack1)) $this->interpreted_phrases[$name] = Array('?', '?');
    else               $this->interpreted_phrases[$name] = Array(array_pop($stack1), array_pop($stack2));

    $this->parse_errors = $errors;
  }
  
  // Evalute: Input: $phrases[], $operations[], $variables[]
  //         Output: $truthtable[]
  function Evaluate()
  {
    $num_inputs = count($this->variables);
    $length     = 1 << $num_inputs;
    $this->truthtable = Array();
    for($l=0; $l<$length; ++$l) $this->truthtable[] = 0;

    foreach($this->phrases as $index=>$phrase)
      $this->DoEvaluate($index);
  }
  
  // Analyze: Input: $variables[], $truthtable[]
  //         Output: $num_inputs
  //                 $num_outputs
  //                 $unused_variables[]
  //                 $output_mapping[]
  //                 $input_mapping[]
  //                 $modified_truthtable[]
  function Analyze()
  {
    // Check three things:
    // 1. Outputs that were always 0 or always 1
    // 2. Outputs that were always a copy of a variable
    //
    // 3. Variables that did not affect output in any way
    $num_outputs = count($this->phrases);
    $num_inputs  = count($this->variables);
    $this->output_mapping = Array();

    $has_zero = 0; // bit0: The OR of all output0 = 0 outputs for all inputs
    $has_one  = 0; // bit0: The OR of all output0 = 1 outputs for all inputs
    $has_differing_inputs = Array();
    foreach($this->truthtable as $index => $value)
    {
      $has_one  |= $value;  
      $has_zero |= ~$value;
      // Complexity: (2^N)*N for num inputs
      for($m=0; $m<$num_inputs; ++$m)
        @$has_differing_inputs[$m] |= $value ^ ~((($index >> $m) & 1)-1);
    }
    for($n=0; $n<$num_outputs; ++$n)
    {
      // Step 1
      if(!($has_one & $has_zero & (1 << $n)))
      {
        $this->output_mapping[$n] = ($has_one >> $n) & 1;
      }
      // Step 2
      for($m=0; $m<$num_inputs; ++$m)
        if(!($has_differing_inputs[$m] & (1 << $n)))
        {
          $this->output_mapping[$n] = "i$m";
          break;
        }
    }

    // Step 3
    $has_zero = Array();
    $has_one  = Array();

    $discard = 0;
    foreach($this->output_mapping as $k=>$v) { $discard |= (1 << $k); }
    $meaningful_outputs_mask = ~$discard;
    foreach($this->truthtable as $index => $value)
    {
      $value &= $meaningful_outputs_mask;
      $bit = (int)($value!=0);
      for($m=0; $m<$num_inputs; ++$m)
        if($index & (1 << $m))
          $has_one[$m][]  = $value;
        else
          $has_zero[$m][] = $value;
    }

    $this->unused_variables = Array();
    $discard_variable_mask = 0;
    for($m=0; $m<$num_inputs; ++$m)
      if($has_zero[$m] == $has_one[$m])
      {
        $this->unused_variables[] = $this->varnames[-($m+1)];
        $discard_variable_mask |= 1 << $m;
      }
    
    // Re-render the truth table
    $newtruth = Array();
    $order = Array();
    for($n = 0; $n < $num_outputs; ++$n)
      $order[] = $n;

    foreach($this->truthtable as $index => $value)
    {
      if($index & $discard_variable_mask) continue;
      $w   = $index >> 5; $b = $index & 31;
      for($n = 0; $n < $num_outputs; ++$n)
        @$newtruth[$n][$w] |= (($value >> $n)&1) << $b;
    }
    #print_r($newtruth);
    $alias = Array();
    usort($order, function($a,$b)use(&$alias,&$newtruth)
    {
      $truth1 = $newtruth[$a];
      $truth2 = $newtruth[$b];
      for($w = count($truth1); $w-- > 0; )
        if($truth1[$w] != $truth2[$w])
          return $truth2[$w] - $truth1[$w];
      $alias[max($a,$b)] = min($a,$b);
      return 0;//This should not happen
    });

    $this->num_outputs = 0;
    foreach($order as $n)
      if(!isset($alias[$n]) && !isset($this->output_mapping[$n]))
        $revmapping[$n] = $this->num_outputs++;
    
    #print_r($revmapping);

    $newtruth = Array();
    foreach($this->truthtable as $index => $value)
    {
      if($index & $discard_variable_mask) continue;
      $row = 0;
      $w   = $index >> 5; $b = $index & 31;
      for($n = 0; $n < $num_outputs; ++$n)
        if(isset($revmapping[$n]))
          $row |= (($value >> $n)&1) << $revmapping[$n];
      $newtruth[] = $row;
    }
    $this->modified_truthtable = $newtruth;

    // Add actual outputs into the output mapping
    // Function number -> where it gets the data
    for($n = 0; $n < $num_outputs; ++$n)
      if(!isset($this->output_mapping[$n]))
      {
        if(isset($alias[$n]))
        {
          $this->output_mapping[$n] = $this->output_mapping[$alias[$n]];
        }
        else
        {
          $this->output_mapping[$n] = 'o' . $revmapping[$n];
        }
      }
    
    // Create an input mapping
    // Variable number -> where it's fed in the truth table
    $this->num_inputs  = 0;
    for($n = 0; $n < $num_inputs; ++$n)
    {
      $k = -($n+1);
      if(!($discard_variable_mask & (1 << $n)))
        $this->input_mapping[$k] = $this->num_inputs++;
      else
        $this->input_mapping[$k] = null;
    }
    
    /*
    print "Truth table:\n";
    foreach($this->truthtable as $index=>$v)
      printf("%s: %s\n",
        str_pad(decbin($index), $this->num_inputs,  '0', STR_PAD_LEFT),
        str_pad(decbin($v),     $this->num_outputs, '0', STR_PAD_LEFT));
    */
  }
  
  function LookUp()
  {
    $enc = new BASE64encoder;
    $enc->Put($this->num_inputs,  5);
    $enc->Put($this->num_outputs, 5);
    for($n = 0; $n < $this->num_outputs; ++$n)
      foreach($this->modified_truthtable as $value)
        $enc->Put( ($value >> $n) & 1, 1);
    
    return $enc->Get();
  }
  
  function Solve()
  {
    $key       = $this->Lookup();
    $this->key = $key;
    $solvition = new Solver($key);
    $solvition->Solve();
    $this->num_gates = $solvition->num_gates;
    
    $input_mapping_reverse = Array();
    foreach($this->input_mapping as $k=>$v)
      if($v !== null)
        $input_mapping_reverse[$v] = $k;
    
    $logic_map = Array();

    if($this->num_gates)
    {
      foreach($solvition->gate_from as $gateno => $inputs)
        foreach($inputs as $index => $input)
        {
          ++$index;
          $n = (int)substr($input, 1);
          if($input[0] == 'i') $logic_map["g{$gateno}.in$index"] = $this->varnames[$input_mapping_reverse[$n]];
          else                 $logic_map["g{$gateno}.in$index"] = "g{$n}.out";
        }
    }
    foreach($this->output_mapping as $outno => $source)
    {
      $n = (int)substr($source, 1);
      $s = (string)$source;
      $name = $this->output_names[$outno];
      switch($s[0])
      {
        case '0': $logic_map[$name] = "0"; break;
        case '1': $logic_map[$name] = "1"; break;
        case 'i': $logic_map[$name] = $this->varnames[-($n+1)]; break;
        case 'o':
          $source = $solvition->out_from[$n];
          if($this->num_gates)
          {
            $n = (int)substr($source, 1);
            $logic_map[$name] = "g{$n}.out";
          }
      }
    }
    $this->logic_map = $logic_map;
  }
  
  private function DoEvaluate($index)
  {
    $num_inputs = count($this->variables);
    $length     = 1 << $num_inputs;
    $op         = $this->operations[$index];
    for($l=0; $l<$length; ++$l)
    {
      $stack = 0;
      foreach($op as $o)
        switch($o)
        {
          case zero:   $stack <<= 1; break;
          case one:    $stack = ($stack<<1) | 1; break;
          case opNOT:  $stack ^= 1; break;
          case opAND:  $stack = (($stack>>2) << 1) | (($stack&1) & (($stack>>1)&1)); break;
          case opNAND: $stack = (($stack>>2) << 1) | ((($stack&1) & (($stack>>1)&1)) ^ 1); break;
          case opOR:   $stack = ($stack>>1) | ($stack&1);       break;
          case opNOR:  $stack = (($stack>>1) | ($stack&1)) ^ 1; break;
          case opXOR:  $stack = ($stack>>1) ^ ($stack&1);       break;
          case opXNOR: $stack = ($stack>>1) ^ ($stack&1) ^ 1;   break;
          case opIMP:  $stack = (($stack>>2) << 1) | ((int)(($stack&1) >= (($stack>>1)&1))); break;
          default:     if(WOLFRAM_ALPHA)
                         $stack = ($stack << 1) | (($l >> ($num_inputs + $o)) & 1);
                       else
                         $stack = ($stack << 1) | (($l >> (-$o-1)) & 1);
                       break;
        }
      if(WOLFRAM_ALPHA)
        $this->truthtable[$l^($length-1)] |= ($stack&1) << $index;
      else
        $this->truthtable[$l] |= ($stack&1) << $index;
    }
  }
  
  private function DoParse($phrase)
  {
    foreach($this->output_names as $index=>$name)
    {
      $prev = ''; if(ctype_alpha($name[0])) $prev = '\b';
      $aft  = ''; if(ctype_alpha($name[strlen($name)-1])) $aft = '\b';
      $phrase = preg_replace('/'.$prev.preg_quote($name).$aft.'/',
                             '('.$this->phrases[$index].')',
                             $phrase);
    }
    #print "Parsing $phrase\n";

    $precedences   = [opNOT=>8, opNAND=>7, opAND=>6, opXNOR=>5, opXOR=>4, opNOR=>3, opOR=>2, opIMP=>1, opPARENS=>0];
    //$associativity = [opNOT=>1, opNAND=>0, opAND=>0, opXNOR=>0, opXOR=>0, opNOR=>0, opOR=>0, opIMP=>0];
    $operators = [
      'NOT' =>opNOT,  '!'=>opNOT,  '~'=>opNOT, '¬'=>opNOT,
      'AND' =>opAND,  '&&'=>opAND, '&'=>opAND, '*'=>opAND, '×'=>opAND, '∧'=>opAND,
      'OR'  =>opOR,   '||'=>opOR,  '|'=>opOR,  '+'=>opOR,  '∨'=>opOR,
      'XOR' =>opXOR,  '^'=>opXOR,  '⊕'=>opXOR, '≠'=>opXOR, '<>'=>opXOR, '!='=>opXOR,
      'NAND'=>opNAND, '¬∧'=>opNAND, 'ANDN'=>opNAND,
      'NOR' =>opNOR,  '¬∨'=>opNOR,  'ORN'=>opNOR,
      'XNOR'=>opXNOR, '¬⊕'=>opXNOR, 'EQV'=>opXNOR, '≡'=>opXNOR, '=='=>opXNOR, 'NXOR'=>opXNOR, 'XORN'=>opXNOR,
      'IMP' =>opIMP,  '→'=>opIMP, '-->'=>opIMP, '->'=>opIMP
    ];

    $reg = '';
    uksort($operators, function($a,$b){return strlen($b)-strlen($a);});
    foreach($operators as $k=>$v)
    {
      $reg .= '|' . preg_quote($k, '/');
      if(ctype_alpha($k[0])) $reg .= '\b';
    }
    $reg = "/[()]$reg|[01]|".NAME_REGEX."/u";

    $output = Array();
    $stack  = Array();
    preg_match_all($reg, $phrase, $mat);
    foreach($mat[0] as $token)
    {
      $u = strtoupper($token);
      if(isset($operators[$u]))
      {
        $u = $operators[$u];
        $my_precedence     = $precedences[$u];
        $right_associative = ($u == opNOT); //($associativity[$u] != 0);
        while(count($stack))
        {
          $top = end($stack);
          if($top == opPARENS) break;
          if($precedences[$top] < $my_precedence) break;
          if($precedences[$top] == $my_precedence && $right_associative) break;
          $output[] = $top;
          array_pop($stack);
        }
        $stack[] = $u;
      }
      elseif($u == '(')
      {
        $stack[] = opPARENS;
      }
      elseif($u == ')')
      {
        while(end($stack) != opPARENS) { $output[] = array_pop($stack); }
        array_pop($stack);
      }
      elseif($u == '0' || $u == '1' || $u == 'zero' || $u == 'one')
      {
        $output[] = (($u=='1' || $u=='one') ? one : zero);
      }
      else
      {
        $output[] = $this->getvar($token);
      }
    }
    while(count($stack) > 0)
    {
      if(end($stack) == opPARENS)
      {
        // Parse error
        array_pop($stack);
        ++$this->parse_errors;
        continue;
      }
      $output[] = array_pop($stack);
    }
    return $output;
  }
  
  private function getvar($name)
  {
    $c = count($this->variables);
    $r = &$this->variables[$name];
    if(isset($r)) return $r;
    $r = -($c+1);
    $this->varnames[$r] = $name;
    return $r;
  }
};
