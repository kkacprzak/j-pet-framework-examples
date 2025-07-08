################################################################################
## Data analysis project based on J-PET Framework
## Created by J-PET Framework developers 2016-2015
##
## Description:
##   Basic data reconstruction for Large Barrel setup
################################################################################

cmake_minimum_required(VERSION 3.1...3.14)

if(${CMAKE_VERSION} VERSION_LESS 3.14)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
    cmake_policy(VERSION 3.14)
endif()


################################################################################
## Project name
set(projectName LargeBarrelAnalysis)

## Auxiliary files
set(AUXILIARY_FILES
  userParams.json
  PARAMETERS.md
  README.md
  run.sh
)

################################################################################
## Binary, header and source files definitions
set(projectBinary ${projectName}.x)
project(${projectName} CXX)

set(HEADERS
            ${CMAKE_CURRENT_SOURCE_DIR}/TimeWindowCreator.h
            ../CommonTools/TimeWindowCreatorTools.h
            ${CMAKE_CURRENT_SOURCE_DIR}/SignalFinder.h
            ../CommonTools/SignalFinderTools.h
            ${CMAKE_CURRENT_SOURCE_DIR}/SignalTransformer.h
            ../CommonTools/SignalTransformerTools.h
            ../ModularDetectorAnalysis/HitFinder.h
            ../ModularDetectorAnalysis/EventFinder.h
            ../ModularDetectorAnalysis/EventCategorizer.h
            ../CommonTools/EventCategorizerTools.h
            ../CommonTools/HitFinderTools.h
            ../CommonTools/EventCategorizerTools.h
            ../CommonTools/CalibrationTools.h
            ../CommonTools/ToTEnergyConverter.h
            ../CommonTools/ToTEnergyConverterFactory.h
            )

set(SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/TimeWindowCreator.cpp
            ../CommonTools/TimeWindowCreatorTools.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/SignalFinder.cpp
            ../CommonTools/SignalFinderTools.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/SignalTransformer.cpp
            ../CommonTools/SignalTransformerTools.cpp
            ../ModularDetectorAnalysis/HitFinder.cpp
            ../CommonTools/HitFinderTools.cpp
            ../ModularDetectorAnalysis/EventFinder.cpp
            ../ModularDetectorAnalysis/EventCategorizer.cpp
            ../CommonTools/EventCategorizerTools.cpp
            ../CommonTools/CalibrationTools.cpp
            ../CommonTools/ToTEnergyConverter.cpp
            ../CommonTools/ToTEnergyConverterFactory.cpp
            )


add_executable(${projectBinary} ${SOURCES} ${HEADERS})
target_link_libraries(${projectBinary} JPetFramework::JPetFramework
                                       Boost::program_options)

add_custom_target(clean_data_${projectName}
  COMMAND rm -f *.tslot.*.root *.phys.*.root *.sig.root)

################################################################################
## Copy the example auxiliary files
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${AUXILIARY_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)

################################################################################
## Unit tests
option(PACKAGE_TESTS "Build the tests" ON)
if(PACKAGE_TESTS)
    enable_testing()
    #add_subdirectory(tests)
endif()
