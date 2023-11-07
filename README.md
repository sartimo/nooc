# nooc

nooc is a self-hosting compiler for the nooc programming language. nooc is a ansi-c derivate language based on a modified tcc backend. nooc focuses mainly on adding oop behaviour. i wrote this piece of software to experiment with one-way transformed code. i think nooc will be particularly interesting when writing code that is not reverse-engineerable. it is additionally intended to simulate the framework in which the notorious worm win32.duqu was written in. since nooc uses the same ansi-c entension as the duqu's framework, we can produce code that behaves fairly similar.

## Install nooc-latest using noocup

```bash
curl -fSsL https://raw.githubusercontent.com/sartimo/nooc/master/util/noocup | bash
noocup install
```

## prerequesities

You need the following packages when building nooc:

- gnu make (make or gmake, depending on your operating system)
- libcurl

## buidling from sources

1. you will need an existing nooc compiler to bootstrap the self-hosting binaries. for this please run this bash snippet.

```bash
wget https://github.com/sartimo/nooc/releases/download/0.0.9/nooc-0.0.9.tar.gz
tar -xvf nooc-0.9.0.tar.gz
cd nooc-0.9.0.tar.gz
chmod +x ./install.sh
./install.sh
```

2. Then you will be able to build the current version from sources using this snippet.

```bash
git clone https://github.com/sartimo/nooc/
cd nooc/
make -j$(nproc)
make install # overwrites the current nooc binaries with the self-hosted compiler binaries
```

and now you are good to call your all new **```nooc```** binary.
