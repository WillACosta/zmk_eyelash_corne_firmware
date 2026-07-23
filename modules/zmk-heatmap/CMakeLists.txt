if(CONFIG_ZMK_HEATMAP)
  target_sources(app PRIVATE src/zmk_heatmap.c)
  target_include_directories(app PRIVATE include)
endif()
