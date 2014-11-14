import sys, os, imp, platform, shutil, subprocess, getopt

release = "1.1.000"
release = "current"
ARCHS = ['intel64']
devbuild = False
keepbuild = False
minbuild = False
mpiroot = os.getenv('I_MPI_ROOT', '/usr')
verbose = False

try:
  opts, args = getopt.getopt(sys.argv[1:],"tdkhv",["travis", "devbuild", "keep", "help", "verbose", "mpi=" ])
except getopt.GetoptError:
  print 'make_kit.py [-t] [-d] [-k] [-h] [-v] [--mpi=<mpi>]'
  sys.exit(2)
for opt, arg in opts:
  if opt == '-g':
    print 'make_kit.py [-t] [-d] [-k] [-h] [-v] [--mpi=<mpi>]'
    sys.exit()
  elif opt in ("-d", "--devbuild"):
    devbuild = True
  elif opt in ("-k", "--keep"):
    keepbuild = True
  elif opt in ("-t", "--travis"):
    minbuild = True
  elif opt in ("--mpi"):
    mpiroot = arg
  elif opt in ("-v", "--verbose"):
    verbose = True


tscons = os.getcwd( )+'/tscons/tscons'
pf = platform.system()


if pf == 'Windows':
  tbbroot = "C:\\tbb42_20140122oss" #C:\\tbb41_20121003oss"
  ARCHS += ['ia32']
  VSS = ['12', '11', '10'] #['9', '11', '10']
else:
  #  tbbroot = "/nfs/hd/disks/tpi0/vssad3/proj/CnC/intel/tbb41_20121003oss"
  tbbroot = "/nfs/hd/disks/tpi0/vssad3/proj/CnC/intel/tbb42_20140122oss"
  VSS = ['']
#  ARCHS += ['mic']

BUILDS = ['Release']
if minbuild == False:
  BUILDS += ['Debug']

tbbver = os.path.basename( tbbroot )

buildenv = { 'Windows': 'TBB=' + tbbroot + ' toolchain=msvc ',
             'Linux': 'TBB=' + tbbroot,
             }

def exe_cmd( cmd ):
  print cmd
  if os.system( cmd ):
    print('failed executing command')
    sys.exit(4)
  
kitdir = 'kit.pkg'
reldir = os.path.join(kitdir, 'cnc', release)

output = subprocess.check_output(["git", "status", "-uall", "--porcelain", "cnc", "samples"])
output += subprocess.check_output(["git", "status", "-uno", "--porcelain", "src"])
if output:
  print('\ngit status not clean')
  if devbuild == False:
    print(output)
    sys.exit(43)

if keepbuild == False:
  shutil.rmtree(kitdir, True)

for vs in VSS:
  for arch in ARCHS:
    if pf == 'Windows':
        v = 'msvcver=' + vs
    else:
        v = ''
#        os.makedirs(reldir+'/lib/'+arch)

        for rel in BUILDS:
            builddir = 'kit.' + rel
            if keepbuild == False:
                shutil.rmtree(builddir, True )
            if os.path.isdir(builddir) == False:
                os.mkdir(builddir)

            cmdl = 'cd ' + builddir + '; cmake -DCMAKE_BUILD_TYPE=' + rel + ' -DTBBROOT=' + tbbroot + ' -DCMAKE_INSTALL_PREFIX=' + os.path.join('..', reldir)
            if minbuild == False:
                cmdl += ' -DBUILD_LIBS_FOR_ITAC=TRUE -DCNC_PRODUCT_BUILD=TRUE'
            else:
                cmdl += ' -DCMAKE_CXX_FLAGS="-DCNC_REQUIRED_TBB_VERSION=6101"'
            if verbose == True:
                cmdl += ' -DCMAKE_VERBOSE_MAKEFILE=TRUE'
            cmdl += ' -DBUILD_LIBS_FOR_MPI=TRUE -DMPIROOT=' + mpiroot + ' .. && make -j 16 install'
            exe_cmd(cmdl)
        
        if minbuild == False:
            exe_cmd( 'chmod 644 `find ' + reldir + ' -type f` && chmod 755 `find ' + reldir + ' -name \*sh`' )
            exe_cmd( 'dos2unix -q `find ' + reldir +' -name \*.h` `find ' + reldir +' -name \*sh`') # && dos2unix -q `find ' + reldir +' -name \*txt` && dos2unix -q `find ' + reldir +' -name \*cpp`)
            exe_cmd('cd ' + kitdir + ' && tar cfvj cnc_' + release + '_pkg.tbz cnc')
