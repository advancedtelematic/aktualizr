# 2. Use exceptions for error handling

Date: 2019-09-13

## Status

Accepted

## Context

There are number of different approaches to error handling in C++, each of them having own pros and cons. Most widely used are exceptions, C-style error codes and various analogues of Rust's `Result<T, E>` type (see for example [this article](https://hackernoon.com/error-handling-in-c-or-why-you-should-use-eithers-in-favor-of-exceptions-and-error-codes-f0640912eb45)).

## Decision

We decide to use exceptions as the method of error handling in libaktualizr for following reasons:

* This is the most idiomatic to C++ way of error handling. C++ Standard Library, Boost and many others use exceptions. Choosing another approach will require us to introduce interface layers to any of those libraries.
* Using C-style error codes makes it harder to provide descriptive information about the error condition. If not done properly it will increase the amount of code at the same time reducing readability as error handling gets mixed with the main control flow, more function arguments are needed, etc. It is also very easy in C++ to ignore return values, which will result in bugs.
* While analogues of Rust's `Result <T, E>` certainly have benefits, there is currently no single implementation which can be considered a standard. Choosing one now will likely result in rewriting error-handling again in the future. We might also face problems with support on some older embedded systems and compilers.
* We already use exceptions in most of the places and follow RAII principles. This should make the cost of implementation not very high.
* We need to finally choose a single approach and follow it, making the codebase consistent.

## Consequences

We need to come up with a consistent exceptions class structure, to allow users of libaktualizr easily understand the source and type of failure.
