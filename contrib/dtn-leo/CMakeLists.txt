# SPDX-License-Identifier: GPL-2.0-only
#
# ns-3.48 CMake build description for the dtn-leo contrib module.
#
# Place this module at:  <ns-3-dev>/contrib/dtn-leo/
# then configure & build:
#   ./ns3 configure --enable-examples --enable-tests
#   ./ns3 build

build_lib(
    LIBNAME dtn-leo
    SOURCE_FILES
        model/cbor.cc
        model/bp-endpoint-id.cc
        model/bp-bundle.cc
        model/bp-header.cc
        model/contact-plan.cc
        model/cgr-router.cc
        model/eclipse-power-model.cc
        model/scf-scheduler.cc
        model/bundle-protocol-agent.cc
        helper/contact-plan-generator.cc
        helper/dtn-leo-helper.cc
    HEADER_FILES
        model/cbor.h
        model/bp-endpoint-id.h
        model/bp-bundle.h
        model/bp-header.h
        model/contact-plan.h
        model/cgr-router.h
        model/eclipse-power-model.h
        model/scf-scheduler.h
        model/bundle-protocol-agent.h
        helper/contact-plan-generator.h
        helper/dtn-leo-helper.h
        helper/json-trace-logger.h
    LIBRARIES_TO_LINK
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libmobility}
        ${libapplications}
        ${libcsma}
    TEST_SOURCES
        test/dtn-leo-test-suite.cc
)

# Examples (guarded by --enable-examples)
if(${ENABLE_EXAMPLES})
    add_subdirectory(examples)
endif()
