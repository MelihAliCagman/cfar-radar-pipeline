# Her test ayrı bir çalıştırılabilir; CTest hepsini çalıştırıp çıkış koduna bakar.
function(add_radar_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE radar)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_radar_test(test_signal_gen)
add_radar_test(test_fft)
