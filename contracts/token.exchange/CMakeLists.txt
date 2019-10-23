add_contract(token.exchange token.exchange ${CMAKE_CURRENT_SOURCE_DIR}/src/token.exchange.cpp)

target_include_directories(token.exchange
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../eosio.token/include)

set_target_properties(token.exchange
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
