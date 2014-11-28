import sys, os, imp, platform, shutil, subprocess, argparse

argParser = argparse.ArgumentParser(prog="make_kit.py", description="Build CnC runtime and API kit")
argParser.add_argument('-r', '--release', default="current", help="release number")
argParser.add_argument('-t', '--travis', action='store_true', default=False, help="Run in Travis mode (implies --nodebug --itac='' --mpi=/usr)")
argParser.add_argument('-p', '--product', action='store_true', default=False, help="Build a release/product package (implies -i and not -d -t --nodebug)")
argParser.add_argument('-d', '--devbuild', action='store_true', default=False, help="Build from an unclean development branch (implies -r=current)")
argParser.add_argument('-k', '--keep', action='store_true', default=False, help="Keep existing (partial) builds")
argParser.add_argument('-v', '--verbose', action='store_true', default=False, help="Verbose builds")
argParser.add_argument('-i', '--installer', action='store_true', default=False, help="Build an installer")
argParser.add_argument('--nodebug', action='store_true', default=False, help="Don't build debug libs")
argParser.add_argument('--tbb', default=os.getenv('TBBROOT', '/usr'), help="TBB root directory")
argParser.add_argument('--mpi', default=os.getenv('I_MPI_ROOT', '/usr'), help="MPI root directory")
argParser.add_argument('--itac', default=os.getenv('VT_ROOT', 'NONE'), help="ITAC root directory")
argParser.add_argument('--msvs', default='12', help="Version(s) of MS Visual Studio to use/build for (MS Windows only)")
args = argParser.parse_args()

release = args.release  
travis = args.travis
product = args.product
devbuild = args.devbuild
keepbuild = args.keep   
verbose = args.verbose  
installer = args.installer
nodbg = args.nodebug      
tbbroot = args.tbb
mpiroot = args.mpi
itacroot = args.itac
vs = args.msvs

ARCHS = ['intel64']  # dropped support for ia32

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
