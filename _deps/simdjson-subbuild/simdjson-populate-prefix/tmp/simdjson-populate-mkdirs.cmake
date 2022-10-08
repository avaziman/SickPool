# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/simdjson-src"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/simdjson-build"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/simdjson-subbuild/simdjson-populate-prefix"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/simdjson-subbuild/simdjson-populate-prefix/tmp"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/simdjson-subbuild/simdjson-populate-prefix/src/simdjson-populate-stamp"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/simdjson-subbuild/simdjson-populate-prefix/src"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/simdjson-subbuild/simdjson-populate-prefix/src/simdjson-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/sickguy/Documents/Projects/SickPool/server/_deps/simdjson-subbuild/simdjson-populate-prefix/src/simdjson-populate-stamp/${subDir}")
endforeach()
