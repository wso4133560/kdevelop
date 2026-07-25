add_definitions(-DTRANSLATION_DOMAIN="kdevpetimeline")

set(kdevpetimeline_SRCS
    petimelineplugin.cpp
    petimelinemodel.cpp
)

qt_add_resources(kdevpetimeline_SRCS petimeline.qrc)
kdevplatform_add_plugin(kdevpetimeline SOURCES ${kdevpetimeline_SRCS})
target_link_libraries(kdevpetimeline
    KDev::Interfaces
    KF6::ConfigCore
    KF6::XmlGui
    Qt::Widgets
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
