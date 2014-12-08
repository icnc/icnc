import sys, os, imp, platform, shutil, subprocess, argparse

argParser = argparse.ArgumentParser(prog="make_kit.py", description="Build CnC runtime and API kit")
argParser.add_argument('-a', '--arch',      default='intel64',                       help="Processor architecture(s) to build for")
argParser.add_argument('-r', '--release',   default="current", help="release number")
argParser.add_argument('-t', '--travis',    default=False, action='store_true',      help="Run in Travis mode (implies --nodebug --itac='' --mpi=/usr)")
argParser.add_argument('-p', '--product',   default=False, action='store_true',      help="Build a release/product package (implies -i --mic and not -d -t --nodebug)")
argParser.add_argument('-d', '--devbuild',  default=False, action='store_true',      help="Build from an unclean development branch (implies -r=current)")
argParser.add_argument('-k', '--keep',      default=False, action='store_true',      help="Keep existing (partial) builds")
argParser.add_argument('-v', '--verbose',   default=False, action='store_true',      help="Verbose builds")
argParser.add_argument('-i', '--installer', default=False, action='store_true',      help="Build an installer")
argParser.add_argument('--phi',             default=False, action='store_true',      help="Build libs for Xeon(R) Phi")
argParser.add_argument('--nodebug',         default=False, action='store_true',      help="Don't build debug libs")
argParser.add_argument('--tbb',             default=os.getenv('TBBROOT', '/usr'),    help="TBB root directory")
argParser.add_argument('--mpi',             default=os.getenv('I_MPI_ROOT', '/usr'), help="MPI root directory")
argParser.add_argument('--itac',            default=os.getenv('VT_ROOT', 'NONE'),    help="ITAC root directory")
argParser.add_argument('--msvs',            default='12',                            help="Version(s) of MS Visual Studio to use/build for (MS Windows only)")
argParser.add_argument('--zip',             default=False, action='store_true',      help="also create password protected zip archive (requires -i)")
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
phi = args.phi
PARCHS = args.arch.split()
    
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
    phi = True
    PARCHS = ['intel64']
    release = '1.0.100' # hm, need to update this automatically?
    if pf == 'Windows':
        tbbroot = "C:\\tbb42_20140122oss" #C:\\tbb41_20121003oss"
        vs = '12 11'
    else:
        tbbroot = "/nfs/hd/disks/tpi0/vssad3/proj/CnC/intel/tbb42_20140122oss"
        PARCHS += ['mic']
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
      print("error code: " + str(retv))
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
  for arch in PARCHS:
    if arch == 'mic':
	cmake_args = ['-DCMAKE_CXX_COMPILER=icpc', '-DCMAKE_C_COMPILER=icc']
    else:
	cmake_args = []
    for rel in BUILDS:

      print('Building ' + vs + ' ' + arch + ' ' + rel)
      
      builddir = 'kit.' + rel + '.' + arch
      if not vs == '':
          builddir += '.' + vs
      if keepbuild == False:
          shutil.rmtree(builddir, True )
      if os.path.isdir(builddir) == False:
          os.mkdir(builddir)

      cmake_args += ['-DCMAKE_BUILD_TYPE=' + rel, '-DPARCH=' + arch] + cmake_args_core
      
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
    tbbver = os.path.basename( tbbroot )
    pwd = os.getcwd()
    docdir = os.path.join(reldir, 'doc')
    pagesdir = 'icnc.github.io'
    os.chdir('..')
    # if os.path.isdir(pagesdir) == False:
    #     exe_cmd(("git clone --depth=1 https://github.com/icnc/"+pagesdir).split())
    # else:
    #     os.chdir(pagesdir)
    #     exe_cmd(['git', 'pull'])
    #     os.chdir('..')
    os.chdir(pwd)
    orgdir = os.path.join('..', pagesdir)
    shutil.copy(os.path.join(orgdir, 'LICENSE'), reldir)
    shutil.copy(os.path.join(orgdir, 'README.md'), os.path.join(reldir, 'README')) 
    for doc in ['FAQ.html', 'Release_Notes.html', 'Getting_Started.html', 'CnC_eight_patterns.pdf']:
        shutil.copy(os.path.join(orgdir, doc), docdir)

    if pf == 'Windows':
        for withTBB in [True, False]:
            aip = 'cnc_installer_' + arch + '.aip'
            shutil.copy(os.path.join('pkg', aip), kitdir)
            shutil.copy(os.path.join('pkg', 'LICENSE.rtf'), kitdir)
            pkgstub = 'cnc_b_' + release + '_' + arch + ('_'+tbbver if withTBB == True else '')
            tbbenv = ''
            if withTBB == True:
                tbbenv = ( 'AddFolder ' + os.path.join('APPDIR', tbbver) + ' ' + os.path.join(tbbroot, 'include') + '\n'
                           'AddFolder ' + os.path.join('APPDIR', tbbver, 'lib') + ' ' + os.path.join(tbbroot, 'lib', arch ) + '\n'
                           'AddFolder ' + os.path.join('APPDIR', tbbver, 'bin') + ' ' + os.path.join(tbbroot, 'bin', arch ) + '\n'
                           'AddFile ' + os.path.join('APPDIR', tbbver, 'bin') + ' ' + os.path.join(tbbroot, 'bin', 'tbbvars.bat') + '\n'
                           'AddFile ' + os.path.join('APPDIR', tbbver) + ' ' + os.path.join(tbbroot, 'README') + '\n'
                           'AddFile ' + os.path.join('APPDIR', tbbver) + ' ' + os.path.join(tbbroot, 'COPYING') + '\n'
                           'AddFile ' + os.path.join('APPDIR', tbbver) + ' ' + os.path.join(tbbroot, 'CHANGES') + '\n'
                           'NewEnvironment -name TBBROOT -value [APPDIR]\\' + tbbver + (' -install_operation CreateUpdate -behavior Replace\n' if withTBB == True else '\n') )
            aic = os.path.join(kitdir, pkgstub + '.aic')
            inf = open( os.path.join('pkg', 'edit_aip.aic.tmpl'))
            l = inf.read();
            inf.close()
            outf = open( aic, 'w' )
            outf.write( l.format( KITDIR=kitdir, CNCVER=release, ARCH=arch, PKGNAME=os.path.abspath(os.path.join(kitdir, 'w_'+pkgstub+'.msi')), TBBENV=tbbenv ) )
            outf.close()
            exe_cmd( os.path.normpath( r'"C:/Program Files (x86)/Caphyon/Advanced Installer 11.2/bin/x86/AdvancedInstaller.com"')
                     + ' /execute ' + os.path.join(kitdir, aip) + ' ' + aic )
    else:
        bindir = os.path.join(reldir, 'bin')
        if os.path.isdir(bindir) == False:
            os.mkdir(bindir)
        shutil.copy(os.path.join('pkg', 'cncvars.csh'), bindir)
        shutil.copy(os.path.join('pkg', 'cncvars.sh'), bindir)
        exe_cmd('chmod 644 `find ' + reldir + ' -type f`')
        exe_cmd('chmod 755 `find ' + reldir + ' -name \*sh`')
        exe_cmd('dos2unix -q `find ' + reldir + ' -name \*.h`')
        exe_cmd('dos2unix -q `find ' + reldir + ' -name \*sh` `find ' + reldir + ' -name \*txt`')
        exe_cmd('dos2unix -q `find ' + reldir + ' -name \*cpp`')
        os.chdir(reldir)
        exe_cmd('tar cfj - * | gpg --batch -c --passphrase "I accept the EULA" > ../cnc_b_' + release + '_install.files')
        os.chdir(pwd)
        cncdir = os.path.join(kitdir, 'cnc/')
        shutil.copy(os.path.join('pkg', 'install.sh'), cncdir)
        exe_cmd(['sed', '-i', 's/$TBBVER/' + tbbver + '/g ; s/$CNCVER/' + release + '/g', os.path.join(cncdir, 'install.sh')])
        shutil.copy('LICENSE', cncdir)
        shutil.copy(os.path.join('pkg', 'INSTALL.txt'), cncdir)
        os.chdir(cncdir)
        if keepbuild == False:
          shutil.rmtree(tbbver, True )
        if os.path.isdir(tbbver) == False:
          os.mkdir(tbbver)
        if os.path.isdir(os.path.join(tbbver, 'bin')) == False:
          os.mkdir(os.path.join(tbbver, 'bin'))
        if os.path.isdir(os.path.join(tbbver, 'lib')) == False:
          os.mkdir(os.path.join(tbbver, 'lib'))
        exe_cmd(['cp', '-r', os.path.join(tbbroot, 'include'), tbbver + '/'])
        exe_cmd(['cp', '-r', os.path.join(tbbroot, 'lib/intel64'), os.path.join(tbbver, 'lib/')])
        if phi:
          exe_cmd(['cp', '-r', os.path.join(tbbroot, 'lib/mic'), os.path.join(tbbver, 'lib/')])
        exe_cmd('cp ' + os.path.join(tbbroot, 'bin/tbbvars.*h') + ' ' + os.path.join(tbbver, 'bin/'))
        exe_cmd(['cp', os.path.join(tbbroot, 'README'), os.path.join(tbbroot, 'CHANGES'), os.path.join(tbbroot, 'COPYING'), tbbver + '/'])
        exe_cmd(['tar', 'cfj', tbbver + '_cnc_files.tbz', tbbver + '/'])
        os.chdir('..')
        exe_cmd(['tar', 'cfvz', 'l_cnc_b_' + release + '.tgz',
                 os.path.join('cnc', 'install.sh'),
                 os.path.join('cnc', 'INSTALL.txt'),
                 os.path.join('cnc', 'LICENSE'),
                 os.path.join('cnc', 'cnc_b_' + release + '_install.files'),
                 os.path.join('cnc', tbbver + '_cnc_files.tbz')])
        if args.zip:
            exe_cmd(['zip', '-rP', 'cnc', 'l_cnc_b_' + release + '.zip',
                     os.path.join('cnc', 'install.sh'),
                     os.path.join('cnc', 'INSTALL.txt'),
                     os.path.join('cnc', 'LICENSE'),
                     os.path.join('cnc', 'cnc_b_' + release + '_install.files'),
                     os.path.join('cnc', tbbver + '_cnc_files.tbz')])
