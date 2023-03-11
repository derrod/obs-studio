option(ENABLE_WIDGET_ZOO "Enable building custom widget demo window" OFF)

if(ENABLE_WIDGET_ZOO)
  target_sources(obs-studio PRIVATE forms/IdianZoo.ui idian/widget-zoo.hpp idian/widget-zoo.cpp)
  target_enable_feature(obs-studio "Widget Zoo" ENABLE_WIDGET_ZOO)
endif()
