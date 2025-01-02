# Memory safety (memsafe)

A prototype of the concept of safe work with memory in the style 
of reference owner control (somewhat similar to Rust), 
but for the C++ language without breaking the backward compatibility of the code.

The implementation is made in the form of a header file and compiler plugin (clang) 
for checking the semantics of moving objects in the code marked with attributes.

