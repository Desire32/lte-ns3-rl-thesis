find_package(Python COMPONENTS Interpreter Development)
if(NOT Python_FOUND)
  message(
    ${HIGHLIGHTED_STATUS}
    "lte-learning disabled: Python not found"
  )
  return()
endif()

find_package(pybind11 CONFIG)
if(NOT pybind11_FOUND)
    message(
      ${HIGHLIGHTED_STATUS}
      "lte-learning disabled: pybind11 not found"
    )
    return()
endif()
include_directories(${pybind11_INCLUDE_DIRS})

build_lib_example(
        NAME defiance-lte-ns3-rl-thesis
        SOURCE_FILES
                pc-simulation.cc
                pc-environment.cc
                device-manager.cc
                base-environment.cc
        HEADER_FILES
                pc-environment.h
                device-manager.h
                base-environment.h
        LIBRARIES_TO_LINK
                ${libdefiance}
                ${liblte}
                ${Python_LIBRARIES}
)
