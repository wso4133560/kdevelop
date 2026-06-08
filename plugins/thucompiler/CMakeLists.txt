add_definitions(-DTRANSLATION_DOMAIN=\"kdevthucompiler\")

set(kdevthucompiler_SRCS
    thucompilerplugin.cpp
)

kdevplatform_add_plugin(kdevthucompiler SOURCES ${kdevthucompiler_SRCS})
target_link_libraries(kdevthucompiler
    KDev::Interfaces
    KDev::Util
    KF6::ConfigCore
    KF6::I18n
    Qt::Widgets
)
