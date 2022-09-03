# #!/bin/bash

# if ($1) then
#     echo "Enter number of threads."
#     exit 0
# fi

# echo "Threads: $1"

# rm -rf sinovate
# git clone https://github.com/SINOVATEblockchain/sinovate --depth=1

# cd sinovate
# ./contrib/install_db4.sh `pwd`

# ./autogen.sh
# # -O2 by default
# ./configure CXXFLAGS="-O3 -march=native"
# make -j $1
# make install