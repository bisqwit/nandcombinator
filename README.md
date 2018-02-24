# NAND combinator

A toolset to calculate the minimal set of 2-input NAND gates
to implement logic functions.

Front-end is at: https://bisqwit.iki.fi/utils/nandcombiner

![Snapshot](images/sample.png)

## Programs

### gatebuilder

The first step is to run gatebuilder.

This program generates all unique combinations of N gates for M inputs,
and saves them into files, that have this name pattern:
`gates-##input-##gate-thread##.dat`.

These files will be huge, but they compress really well.
It is highly recommended to do this on a compressed filesystem,
such as btrfs with zstd compression.

The process will also take a lot of time. NAND networks with few inputs
or few gates will get built in fractions of seconds, but larger networks
may take several days to compute, utilizing all CPU cores at maximum.

### nandcombinator

The second step is to run nandcombinator.

This program calculates the optimal solutions for N inputs to N outputs,
for every possible unique truth table.
Different permutations are only computed once,
and duplicate outputs are discarded.

This program reads the files generated by *gatebuilder*,
and generates a new set of files that have this pattern:
`datalog_#_#.dat`.

These files are smaller than the gatebuilder outputs.

Once you no longer plan to ever run nandcombinator,
you can delete the files generated by gatebuilder in step 1.

Nandcombinator uses the `mmap` system call to map
the gatebuilder files into its address space,
and you really should have a 64-bit system to run this on;
otherwise the process will be seriously hampered by architectural limits.

### import.php

In the third step, the files generated by nandcombinator are
converted into indexed SQLite3 databases by running import.php.

It generates a new set of files that have this pattern:
`db_##in##out.db`.

It reads the datalog files generated by nandcombinator.

Once you no longer plan to run import.php,
you can delete the files generated by nandcombinator in step 2.

#### import_through_shell.php

There is a version that does not require the SQLite3 PHP extension.
Instead, it requires the commandline sqlite3 tool.

As an intermediate step, this script generates two sets of output:

* SQL files that have a pattern `db_##in##out.sql`
* Shell commands that convert the SQL file into SQLite3 databases, and which automatically delete the SQL files when done.

To run import.php, use this command: `import_through_shell.php | bash`

## Data format

### gatebuilder results

Each file consists of fixed-length records.

The record begins with num_gates × 2 bytes describing the gate network.
Each NAND gate has a pair of inputs, and the input is a byte.
An input that has value that is less than `num_inputs` denotes
an input from the function parameters, and an input that is `≥ num_inputs`
denotes an input from another gate.
In this case, the value is less than `(num_inputs + num_gates)`.

After the gate network comes the truth table.
The truth table has `1u << num_inputs` rows.
Each row is a 16-bit integer, where each bit denotes the output of a single
NAND gate for the set of inputs denoted by the bits set in the row number.

### nandcombinator results

Each file is a text file that consists of a number of rows that have three
space-separated fields.
The first field is the number of NAND gates needed to perform this calculation.
The second field is a BASE64’ish string that encodes the search key.
The third field is a BASE64’ish string that encodes the wirings.

The BASE64’ish encoding algorithm is described in the `base64.php` file.
It is basically a storage for an arbitrary number of arbitrary width
bitfields.

The search key is encoded as follows:

* 5 bits: integer, number of inputs
* 5 bits: integer, number of outputs
* `num_outputs` bits: result for each output. This is repeated `1u << num_inputs` times, for each combination of bits set in the input.

The wirings are encoded as follows:

* 5 bits: input to a gate. Repeated `2 × num_gates` times, for each NAND gate in this assembly. The meanings are the same as in the gatebuilder results.
* 5 bits: source to an output. Repeated `num_outputs` times. It indicates which NAND gate this output is pulled from.

### Database fields

Each combination of `num_inputs` and `num_outputs` is their own database file,
with name `db_XXinYYout.db` where XX is the number of inputs and YY is the number of outputs,
in decimal, with zero-padding.

The database schema is as follows:

    CREATE TABLE conundrum
    (
      gates       INT NOT NULL,
      logic       TEXT NOT NULL,
      connections VARCHAR(46) NOT NULL,
      PRIMARY KEY(logic)
    )

These parameters are also encoded in the `logic` field.
The `gates`, `logic` and `connection` fields have the same content
and meaning as the three fields in the nandcombinator results.
