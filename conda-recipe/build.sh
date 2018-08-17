env TBBROOT=${PREFIX} python make_kit.py -r ${PKG_VERSION} --mpi=${PREFIX} --sdl -v -d --nodebug
cp -rp kit.pkg/cnc/${PKG_VERSION}/include kit.pkg/cnc/${PKG_VERSION}/share ${PREFIX}/
cp -rp kit.pkg/cnc/${PKG_VERSION}/lib/intel64/* ${PREFIX}/lib/
