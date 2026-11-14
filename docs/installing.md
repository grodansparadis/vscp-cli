# Install

## Ready built programs

Every push to the repository builds the tools for these platforms:

| Artifact | Platform |
| :------- | :------- |
| `vscp-cli-linux-x86_64` | Linux x86_64 (Debian 12, Ubuntu 22.04 and newer) |
| `vscp-cli-linux-arm64` | Raspberry Pi OS 64-bit (bookworm) and other 64-bit ARM Linux |
| `vscp-cli-linux-armhf` | Raspberry Pi OS 32-bit (bookworm) |
| `vscp-cli-macos-arm64` | macOS on Apple silicon |
| `vscp-cli-windows-x64` | Windows 10/11 x64 |

Open the [Actions](https://github.com/grodansparadis/vscp-cli/actions) page,
pick a successful run of the platform's workflow (or click a build badge in
the README) and download the artifact at the bottom of the run page (GitHub
only serves artifacts to signed in users). Each artifact holds an archive and a
`SHA256SUMS-<artifact>` file to check it with:

```bash
sha256sum -c SHA256SUMS-vscp-cli-linux-arm64
```

### Linux and Raspberry Pi

The archive holds the files as installed below `/usr` (`usr/bin/vscp-*`,
the examples and the documentation). Install the libraries and unpack it:

```bash
sudo apt install libmosquitto1 libexpat1 libcurl4 libssl3
sudo tar -xzf vscp-cli-linux-arm64.tar.gz -C /
vscp-send --version
```

Or unpack it anywhere and run the programs from `usr/bin` there.

### macOS

The tools use OpenSSL, libmosquitto and expat from Homebrew:

```bash
brew install mosquitto openssl@3 expat
mkdir -p ~/vscp-cli && tar -xzf vscp-cli-macos-arm64.tar.gz -C ~/vscp-cli
xattr -dr com.apple.quarantine ~/vscp-cli
export PATH="$HOME/vscp-cli/bin:$PATH"
```

The programs are not signed; the `xattr` command keeps macOS from refusing
to start them.

### Windows

Unpack the zip file, for example to `C:\Program Files\vscp-cli`. The `bin`
folder holds the programs and the DLLs they need (OpenSSL, libmosquitto,
expat, curl, pthreads). Add it to `PATH` or start the programs from there:

```bat
cd "C:\Program Files\vscp-cli\bin"
vscp-dump -i udp
```

The `socketcan` interface is Linux only. On Windows and macOS use `canal`
with a CANAL driver for CAN hardware.

## Building from source

The VSCP library and the header only libraries (nlohmann/json, spdlog,
Mustache, maddy) are fetched by CMake at pinned commits. CMake 3.16 or later
and a C++17 compiler are needed, plus OpenSSL, libmosquitto, expat and curl.

### Linux / Raspberry Pi

```bash
sudo apt install build-essential cmake git libmosquitto-dev libexpat1-dev libssl-dev libcurl4-openssl-dev
git clone https://github.com/grodansparadis/vscp-cli.git
cd vscp-cli
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

### macOS

```bash
xcode-select --install
brew install cmake ninja mosquitto openssl@3 expat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake --build build --parallel
cmake --install build --prefix ~/vscp-cli
```

### Windows

Install Visual Studio 2022 (*Desktop development with C++*) and
[vcpkg](https://vcpkg.io). From a *x64 Native Tools Command Prompt*:

```bat
vcpkg install curl dlfcn-win32 expat mosquitto openssl pthreads --triplet x64-windows
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --parallel
cmake --install build --prefix C:\vscp-cli
copy C:\vcpkg\installed\x64-windows\bin\*.dll C:\vscp-cli\bin
```

### Using a local VSCP checkout

```bash
cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_VSCP=../vscp
```
