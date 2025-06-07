################################################################################
## Data analysis project based on J-PET Framework
## Created by J-PET Framework developers 2016-2025
##
## Description:
##   Data reconstruction in Modular J-PET Detector
################################################################################

cmake_minimum_required(VERSION 3.1...3.14)

if(${CMAKE_VERSION} VERSION_LESS 3.14)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
    cmake_policy(VERSION 3.14)
endif()

######################################################################
## Project name
######################################################################
set(projectName ModularDetectorAnalysisMTAB)

## Auxiliary files are copied to build directory after CMake
## You can add any files to this list
set(AUXILLIARY_FILES
  README.md
  userParamsModular.json
)

################################################################################
## Binary, header and source files definitions
set(projectBinary ${projectName}.x)
project(${projectName} CXX) # using only C++

######################################################################
## Name your header files with .h extension and source files with .cpp
## Add your own HEADERS/SOURCES
set(HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/TimeWindowCreator.h
            ${CMAKE_CURRENT_SOURCE_DIR}/SignalFinder.h
            ${CMAKE_CURRENT_SOURCE_DIR}/HitFinder.h
            ${CMAKE_CURRENT_SOURCE_DIR}/EventCategorizer.h
            ${CMAKE_CURRENT_SOURCE_DIR}/EventFinder.h
            ${CMAKE_CURRENT_SOURCE_DIR}/EventCategorizer.h
            ../NTupleExport/NTupler.h
            ../CommonTools/Downscaler.h
            ../CommonTools/TimeWindowCreatorTools.h
            ../CommonTools/SignalFinderTools.h
            ../CommonTools/HitFinderTools.h
            ../CommonTools/EventCategorizerTools.h
            ../CommonTools/CalibrationTools.h
            )

set(SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
            TimeWindowCreator.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/SignalFinder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/HitFinder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/EventCategorizer.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/EventFinder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/EventCategorizer.cpp
            ../NTupleExport/NTupler.cpp
            ../CommonTools/Downscaler.cpp
            ../CommonTools/TimeWindowCreatorTools.cpp
            ../CommonTools/SignalFinderTools.cpp
            ../CommonTools/HitFinderTools.cpp
            ../CommonTools/EventCategorizerTools.cpp
            ../CommonTools/CalibrationTools.cpp
            )

add_executable(${projectBinary} ${SOURCES} ${HEADERS})
target_link_libraries(${projectBinary} JPetFramework::JPetFramework)

add_custom_target(clean_data_${projectName}
  COMMAND rm -f *.tslot.root *.hits.root *.sig.root *evt.root
)

################################################################################
## Copy the example auxiliary files
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${AUXILIARY_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)

################################################################################
## Add new target that depends on copied files
add_custom_target(copy_files_${projectName} DEPENDS ${copy_depends})
## Add dependencies between building project and copy files
add_dependencies(${projectBinary} copy_files_${projectName})

################################################################################
## Unit tests
#option(PACKAGE_TESTS "Build the tests" ON)
#if(PACKAGE_TESTS)
#    enable_testing()
#    add_subdirectory(tests)
#endif()
