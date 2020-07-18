# Transfuse

## Requirements
- CMake
- SQLite 3
- libxml2
- xxhash

* Debian/Ubuntu: `sudo apt-get install build-essential cmake libsqlite3-dev libxml2-dev libxxhash-dev libzip-dev`
* macOS MacPorts: `sudo port install cmake sqlite3 libxml2 xxhash libzip`

## Usage
Given a HTML document, run `tf-extract document.html` or `cat document.html | tf-extract` to extract text blocks with transformed inline tags.
