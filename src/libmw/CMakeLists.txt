add_library(libmw SHARED "libmw.cpp" "BlockStoreWrapper.h" "Transformers.h" "State.h")

add_dependencies(libmw litecoin MW::Common MW::Crypto MW::Database MW::File MW::MMR MW::Traits MW::Node)
target_link_libraries(libmw PRIVATE litecoin MW::Common MW::Crypto MW::Database MW::File MW::MMR MW::Traits MW::Node)
set_target_properties(libmw PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN 1)