# ResourceCompiler
A simple &amp; easy to use resource compiler for C/C++ using JSON.

## Usage

Compile the resources into .c & .h files using the following command:
```
ResourceCompiler.exe path/to/resources [optional: path/to/resource-output]
```
Second argument is optional. If it's not passed, the compiled output files will be outputted to the path given in first argument.

Check the resources folder in the repo for more information on how to set up the resources and JSON files.

There should always be a resources.rc.json file in the path/to/resources folder. This file defines the structure of all the resources and sub-resources.

There's also a json schema file in this repo to use with an IDE for auto-complete and validation.
