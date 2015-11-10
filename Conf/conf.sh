#!/bin/bash

# Set the concolic unit testing tool directory
export CONCERTDIR=/absolute/path/to/concert

# Set the absolute path to CREST
# If you set $CRESTDIR correctly, the following command can run 'run_crest'
# $ $CRESTDIR/bin/run_crest
export CRESTDIR=/absolute/path/to/crest

# Set the absolute path to the source code
# All source code files should be in the same directory
export SRCDIR=/absolute/path/to/sources

# Set the list of source code files
# In the source_list.txt file, each line should have one source code file name
# without its directory path. All the source code file should be pre-processed
# You can set any filename for $SRCLIST
export SRCLIST=/absolute/path/to/source_list.txt

# Set the list of function names
# In the function_list.txt file, each line should have one function name
# TestGenerator will generate test drivers for the functions in this list 
# You can set any filename for $FUNCLIST
export FUNCLIST=/absolute/path/to/function_list.txt
