add_definitions(-DTRANSLATION_DOMAIN=\"kdevserialmonitor\")

set(kdevserialmonitor_SRCS
    serialmonitorplugin.cpp
)

kdevplatform_add_plugin(kdevserialmonitor SOURCES ${kdevserialmonitor_SRCS})
target_link_libraries(kdevserialmonitor
    KDev::Interfaces
    KF6::ConfigCore
    KF6::I18n
    Qt::Widgets
    setupapi
)
