# nooc

nooc is a self-hosting compiler for the nooc programming language. nooc is a ansi-c derivate language based on a modified tcc backend. nooc focuses mainly on adding oop behaviour. i wrote this piece of software to experiment with one-way transformed code. i think nooc will be particularly interesting when writing code that is not reverse-engineerable.

## prerequesities

You need the following packages when building nooc:

- gnu make (make or gmake, depending on your operating system)
- libcurl

## get started

run ```make -j$(nproc)``` and you are good
