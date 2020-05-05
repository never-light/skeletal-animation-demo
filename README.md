#### Skeletal animation demo

This simple application demonstrates basic implementation of the 3D skeletal animation technique.  
  
For more complex implementation with clips blending and ability to use state machine see [this](https://github.com/n-paukov/swengine) repository.  

[![Demo](https://img.youtube.com/vi/xV0fzxvsOAo/0.jpg)](https://www.youtube.com/watch?v=xV0fzxvsOAo)


#### How to build:

##### Prerequisites

* C++17/20 compiler;
* Conan package manager;
* CMake;

##### Build steps

1. Clone repository.
2. Add the following remote repositories to Conan package manager via `conan remote add`:
    *  https://api.bintray.com/conan/bincrafters/public-conan
3. Install dependencies via Conan package manager, generate `conanbuildinfo.cmake` file and put it in `build/conanbuildinfo.cmake`.
4. Build project with CMake. 
5. Set working directory to directory `bin`.
