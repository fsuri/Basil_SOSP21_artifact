#!/bin/bash
rm -r ~/BFT-DB/src/store/bftsmartstore/library/java-config-*
rm -r ~/BFT-DB/src/store/bftsmartstore/library/remote
mkdir ~/BFT-DB/src/store/bftsmartstore/library/remote
cat ~/BFT-DB/src/scripts/hosts | while read machine
do
    echo "generating config file for machine ${machine}"
    mkdir ~/BFT-DB/src/store/bftsmartstore/library/remote/java-config-${machine}
    cp -r ~/BFT-DB/src/store/bftsmartstore/library/java-config ~/BFT-DB/src/store/bftsmartstore/library/remote/java-config-${machine}/java-config
done

