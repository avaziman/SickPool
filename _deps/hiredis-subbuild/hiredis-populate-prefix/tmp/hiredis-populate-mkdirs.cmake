# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/hiredis-src"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/hiredis-build"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/hiredis-subbuild/hiredis-populate-prefix"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/hiredis-subbuild/hiredis-populate-prefix/tmp"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/hiredis-subbuild/hiredis-populate-prefix/src/hiredis-populate-stamp"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/hiredis-subbuild/hiredis-populate-prefix/src"
  "/home/sickguy/Documents/Projects/SickPool/server/_deps/hiredis-subbuild/hiredis-populate-prefix/src/hiredis-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/sickguy/Documents/Projects/SickPool/server/_deps/hiredis-subbuild/hiredis-populate-prefix/src/hiredis-populate-stamp/${subDir}")
endforeach()
