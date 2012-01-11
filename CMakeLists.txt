CMAKE_MINIMUM_REQUIRED(VERSION 2.4)

cmake_policy(SET CMP0003 NEW)

IF(NOT ParaView_BINARY_DIR)
  FIND_PACKAGE(ParaView REQUIRED)
  INCLUDE(${PARAVIEW_USE_FILE})
ENDIF(NOT ParaView_BINARY_DIR)

if(PARAVIEW_BUILD_QT_GUI)

  if (PARAVIEW_ENABLE_PYTHON)
    include_directories(${PARAVIEW_PYTHON_INCLUDE_DIR})
  endif()

  include_directories(${QT_QTNETWORK_INCLUDE_DIR})

  QT4_WRAP_CPP(MOC_SRCS pqRemoteControl.h pqSocketItem.h pqSocketHandler.h pqPythonSocketHandler.h)
  QT4_WRAP_UI(UI_SRCS pqRemoteControl.ui)

  ADD_PARAVIEW_DOCK_WINDOW(
    OUTIFACES
    OUTSRCS
    CLASS_NAME pqRemoteControl
    DOCK_AREA Right)

  ADD_PARAVIEW_PLUGIN(pqRemoteControl "1.0"
                      REQUIRED_ON_CLIENT
                      GUI_INTERFACES ${OUTIFACES}
                      GUI_SOURCES ${OUTSRCS} ${MOC_SRCS} ${UI_SRCS}
                                   pqRemoteControl.cxx
                                   pqSocketItem.cxx
                                   pqPythonSocketHandler.cxx)
endif()
