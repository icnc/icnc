import sys, os, imp, platform, shutil, subprocess, getopt

release = "1.1.000"  # official next/current release version
ARCHS = ['intel64']  # dropped support for ia32
devbuild = False     # by default, we don't accept changes in source tree
keepbuild = False    # by default, we clean up build dirs before install/compile
verbose = False      # by default builds are not verbose
installer = False    # by default we don't create an installer
nodbg = False        # by default we build release and debug libs
tbbroot = os.getenv('TBBROOT', '/usr') # by default we read TBBROOT for TBB installation path
mpiroot = os.getenv('I_MPI_ROOT', '/usr') # by default we read I_MPI_ROOT for MPI installation path
itacroot = os.getenv('VT_ROOT', 'NONE') # by default we read VT_ROOT for ITAC installation path or disable ITAC
vs = '12'            # by default we use VS 12 (windows), to build more than one do something like '12 11'

travis = False       # travis mode: only release libs and no packaging
product = False      # product mode: right TBB, all libs, installer

try:
  opts, args = getopt.getopt(sys.argv[1:],"r:tdkhvip",["release=", "travis", "devbuild", "keep", "help", "verbose", "installer", "product", "mpi=", "itac=", "nodebug", "msvs=" ])
except getopt.GetoptError:
  print 'make_kit.py [-r=<rel>] [-t] [-d] [-k] [-h] [-v] [-i] [-p] [--nodebug] [--product] [--tbb=<tbbroot>] [--mpi=<mpiroot>] [--itac=<itacroot>] [--msvs=<vs versions>]'
  sys.exit(2)
for opt, arg in opts:
  if opt in ("-h", "--help"):
    print 'make_kit.py [-r=<rel>] [-t] [-d] [-k] [-h] [-v] [-i] [-p] [--nodebug] [--product] [--tbb=<tbbroot>] [--mpi=<mpi>] [--itac=<itacroot>] [--msvs=<vs versions>]'
    sys.exit(3)
  elif opt in ("-d", "--devbuild"):
    devbuild = True
  elif opt in ("-k", "--keep"):
    keepbuild = True
  elif opt in ("-t", "--travis"):
    travis = True
  elif opt in ("--tbb"):
    tbbroot = arg
  elif opt in ("--mpi"):
    mpiroot = arg
  elif opt in ("--itac"):
    itacroot = arg
  elif opt in ("--msvs"):
    vs = arg
  elif opt in ("--nodebug"):
    nodbg = True
  elif opt in ("-r", "--release"):
    release = arg
  elif opt in ("-v", "--verbose"):
    verbose = True
  elif opt in ("-p", "--product"):
    product = True
  elif opt in ("-i", "--installer"):
    installer = True

if devbuild == True:
    release = "current"
    
if travis == True:
    release = "current"
    nodbg = True
    mpiroot = '/usr'
    itacroot = 'NONE'
    
pf = platform.system()

if product == True:
    nodbg = False
    installer = True
    devbuild = False
    travis = False
    if pf == 'Windows':
        tbbroot = "C:\\tbb42_20140122oss" #C:\\tbb41_20121003oss"
        vs = '12 11'
    else:
        tbbroot = "/nfs/hd/disks/tpi0/vssad3/proj/CnC/intel/tbb42_20140122oss"
        #  ARCHS += ['mic']
    if itacroot == 'NONE' or mpiroot == 'NONE':
        print('Need itacroot and mpiroot for product build')
        sys.exit(44)
      
# dir where the kit gets installed into
kitdir = 'kit.pkg'
# the specific release gets installed into here
reldir = os.path.join(kitdir, 'cnc', release)

##############################################################
##############################################################
# End of config section
##############################################################
##############################################################

if pf == 'Windows':
    VSS = vs.split()
else:
    VSS = ['']

BUILDS = ['Release']
if nodbg == False:
    BUILDS += ['Debug']

def exe_cmd( cmd ):
  print cmd
  if isinstance(cmd, str):
      retv = os.system(cmd)
  else:
      retv = subprocess.call( cmd )
  if not retv == 0:
      print("error code: " + retv)
      exit(444)

##############################################################
# check if our source tree is clean
output = subprocess.check_output(["git", "status", "-uall", "--porcelain", "cnc", "samples"])
output += subprocess.check_output(["git", "status", "-uno", "--porcelain", "src"])
if output:
  print('\ngit status not clean')
  if devbuild == False:
    print(output)
    sys.exit(43)
    
##############################################################
# clean existing builds if requested
if keepbuild == False:
  shutil.rmtree(kitdir, True)

##############################################################
# cmake command line: the args which are shared on all platforms
cmake_args_core = ['-DTBBROOT=' + tbbroot, '-DCMAKE_INSTALL_PREFIX=' + os.path.join('..', reldir)]
if not mpiroot == 'NONE':
  cmake_args_core += ['-DBUILD_LIBS_FOR_MPI=TRUE', '-DMPIROOT='+mpiroot]
else:
  cmake_args_core += ['-DBUILD_LIBS_FOR_MPI=FALSE']
  
if not itacroot == 'NONE':
  cmake_args_core += ['-DBUILD_LIBS_FOR_ITAC=TRUE', '-DITACROOT='+itacroot]
else:
  cmake_args_core += ['-DBUILD_LIBS_FOR_ITAC=FALSE']
  
if travis == True or devbuild == True:
  cmake_args_core += ['-DCMAKE_CXX_FLAGS=-DCNC_REQUIRED_TBB_VERSION=6101']

if product == True:
  cmake_args_core += ['-DCNC_PRODUCT_BUILD=TRUE']
else:
  cmake_args_core += ['-DCNC_PRODUCT_BUILD=FALSE']
  
if verbose == True:
  cmake_args_core += ['-DCMAKE_VERBOSE_MAKEFILE=TRUE']
else:
  cmake_args_core += ['-DCMAKE_VERBOSE_MAKEFILE=FALSE']
  
cmake_args_core += ['..']

##############################################################
# build all libs and install headers and examples etc into reldir
for vs in VSS:
  for arch in ARCHS:
    for rel in BUILDS:

      print('Building ' + vs + ' ' + arch + ' ' + rel)
      
      builddir = 'kit.' + rel
      if not vs == '':
          builddir += '.' + vs
      if keepbuild == False:
          shutil.rmtree(builddir, True )
      if os.path.isdir(builddir) == False:
          os.mkdir(builddir)

      cmake_args = ['-DCMAKE_BUILD_TYPE=' + rel] + cmake_args_core
      
      os.chdir(builddir)
      if pf == 'Windows':
          exe_cmd( ['c:/Program Files (x86)/Microsoft Visual Studio ' + vs + '.0/VC/vcvarsall.bat', 'x64',
                  '&&', 'cmake', '-G', 'NMake Makefiles'] + cmake_args + ['&&', 'nmake', 'install'] )
      else:
          exe_cmd(['cmake'] + cmake_args)
          exe_cmd(['make', '-j', '16', 'install'])
      os.chdir('..')

##############################################################
# now sanitize files and create installer
if installer == True:
    pwd = os.getcwd()
    docdir = os.path.join(reldir, 'doc')
    pagesdir = 'icnc.github.io'
    os.chdir('..')
    if os.path.isdir(pagesdir) == False:
        exe_cmd(("git clone --depth=1 https://github.com/icnc/"+pagesdir).split())
    else:
        os.chdir(pagesdir)
        exe_cmd(['git', 'pull'])
        os.chdir('..')
    os.chdir(pwd)
    orgdir = os.path.join('..', pagesdir)
    shutil.copy(os.path.join(orgdir, 'LICENSE'), reldir)
    shutil.copy(os.path.join(orgdir, 'README.md'), os.path.join(reldir, 'README')) 
    for doc in ['FAQ.html', 'Release_Notes.html', 'Getting_Started.html', 'CnC_eight_patterns.pdf']:
        shutil.copy(os.path.join(orgdir, doc), docdir)

    if pf == 'Windows':
        print('no installer built')
    else:
        exe_cmd('chmod 644 `find ' + reldir + ' -type f`')
        exe_cmd('chmod 755 `find ' + reldir + ' -name \*sh`')
        exe_cmd('dos2unix -q `find ' + reldir + ' -name \*.h`')
        exe_cmd('dos2unix -q `find ' + reldir + ' -name \*sh` `find ' + reldir + ' -name \*txt`')
        exe_cmd('dos2unix -q `find ' + reldir + ' -name \*cpp`')
        os.chdir(kitdir)
        exe_cmd(['tar', 'cfvj', 'cnc_' + release + '_pkg.tbz', os.path.join('cnc', release)])
        exe_cmd(['zip', '-rP', 'cnc', 'cnc_' + release + '_pkg.zip', os.path.join('cnc', release)])
        os.chdir('..')
