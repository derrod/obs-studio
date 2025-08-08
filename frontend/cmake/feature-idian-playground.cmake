option(ENABLE_WIDGET_PLAYGROUND "Enable building custom widget demo window" OFF)

if(ENABLE_WIDGET_PLAYGROUND)
  target_sources(
    obs-studio
    PRIVATE forms/OBSIdianPlayground.ui dialogs/OBSIdianPlayground.hpp dialogs/OBSIdianPlayground.cpp
  )
  target_enable_feature(obs-studio "Widget Playground" ENABLE_WIDGET_PLAYGROUND)
else()
  target_disable_feature(obs-studio "Widget Playground")
endif()
