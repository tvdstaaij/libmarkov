# libmarkov

C++ library for text generation using a Markov chain model. Lightweight,
but designed and tested with large input sets in mind. It can handle texts up to
an order of 10^8 words given the resources of a decent modern dekstop computer.

## Compilation / linking

The code is light enough to compile directly into another project. It is 
contained in just one header/source pair.

1. Make sure C++11 is supported and enabled
2. Add `libmarkov.cpp` to compilation targets
3. Add this directory to the include path, then `#include "libmarkov.h"`

This repository does not contain build scripts to support static or dynamic
linking.

Note that the tree-based data structure holding the Markov model uses pointers
extensively and may consume significantly more RAM in a 64-bit build. The code
has only been tested with 32-bit builds.

## Documentation

API documentation for the public interface can be generated with [Doxygen][1].

## Known issues

* `MarkovChain::getStateCount()` is not yet meaningfully implemented.
* `MarkovChain::setOutputMode()` only supports the `MARKOV_MODE_RANDOM` mode;
  `MARKOV_MODE_PROBABLE` does not work yet.

[1]: http://www.stack.nl/~dimitri/doxygen/
