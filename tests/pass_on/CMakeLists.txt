# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(pass_on_all   pass_on_all.cpp   " " pass_on_all.ref   "dist")
add_cnc_test(pass_on_bcast pass_on_bcast.cpp " " pass_on_bcast.ref "dist")
