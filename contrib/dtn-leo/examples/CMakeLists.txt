# SPDX-License-Identifier: GPL-2.0-only

build_lib_example(
    NAME dtn-leo-cgr-example
    SOURCE_FILES dtn-leo-cgr-example.cc
    LIBRARIES_TO_LINK
        ${libdtn-leo}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libcsma}
        ${libmobility}
        ${libapplications}
)
