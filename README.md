# Transfuse

## Requirements
- CMake
- SQLite 3
- libxml2
- xxhash
- libzip
- pkg-config (for non-vcpkg platforms)

* Debian/Ubuntu: `sudo apt-get install build-essential cmake pkg-config libsqlite3-dev libxml2-dev libxxhash-dev libzip-dev`
* macOS MacPorts: `sudo port install cmake pkgconfig sqlite3 libxml2 xxhash libzip`

## Usage
Given a HTML document, run `tf-extract document.html` or `cat document.html | tf-extract` to extract text blocks with transformed inline tags.
