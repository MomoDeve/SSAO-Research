# SSAO Research

This is research project of multiple SSAO Algorithms.

## Special credit:
- [JoeyDeVries LearnOpenGL](https://github.com/JoeyDeVries/LearnOpenGL) - base application template
- [scanberg HBAO](https://github.com/scanberg/hbao) - base for HBAO shader (with reference check according to the original paper)
- [asylum2010 GTAO](https://github.com/asylum2010/Asylum_Tutorials) - base for GTAO shader (with reference check according to the original paper)

## Building

### Windows
All relevant libraries are found in /libs and all DLLs found in /dlls (pre-)compiled for Windows. 
The CMake script knows where to find the libraries so just run CMake script and generate project of choice.

Keep in mind the supplied libraries were generated with a specific compiler version which may or may not work on your system (generating a large batch of link errors). In that case it's advised to build the libraries yourself from the source.

### Linux
`apt-get install libsoil-dev libglm-dev libassimp-dev libglew-dev libglfw3-dev libxinerama-dev libxcursor-dev  libxi-dev libfreetype-dev libgl1-mesa-dev xorg-dev` .
**Build through Cmake command line:**
```
cd /path/to/LearnOpenGL
mkdir build && cd build
cmake ..
cmake --build .
```

### Mac OS X
```
brew install cmake assimp glm glfw freetype
cmake -S . -B build
cmake --build build -j$(sysctl -n hw.logicalcpu)
```
