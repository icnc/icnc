import sys, os, imp, platform, shutil, subprocess, getopt

release = "1.1.000"
release = "current"
ARCHS = ['intel64']
devbuild = False
keepbuild = False

try:
  opts, args = getopt.getopt(sys.argv[1:],"dkh",["devbuild","keep","help"])
except getopt.GetoptError:
  print 'make_kit.py [-d] [-k] [-h]'
  sys.exit(2)
for opt, arg in opts:
  if opt == '-g':
    print 'make_kit.py [-d] [-k] [-h]'
    sys.exit()
  elif opt in ("-d", "--devbuild"):
    devbuild = True
  elif opt in ("-k", "--keep"):
    keepbuild = True


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

        for rel in ['Release','Debug']:
            builddir = 'kit.' + rel
            if keepbuild == False:
              shutil.rmtree(builddir, True )
              os.mkdir(builddir)
            exe_cmd('cd ' + builddir + '; cmake -DCMAKE_BUILD_TYPE=' + rel
                    + ' -DBUILD_LIBS_FOR_MPI=TRUE -DBUILD_LIBS_FOR_ITAC=TRUE -DCNC_PRODUCT_BUILD=TRUE -DTBBROOT=' + tbbroot
                    + ' -DCMAKE_INSTALL_PREFIX=' + os.path.join('..', reldir) + ' .. && make -j 16 install')
        
        exe_cmd( 'chmod 644 `find ' + reldir + ' -type f` && chmod 755 `find ' + reldir + ' -name \*sh`' )
        exe_cmd( 'dos2unix -q `find ' + reldir +' -name \*.h` `find ' + reldir +' -name \*sh`') # && dos2unix -q `find ' + reldir +' -name \*txt` && dos2unix -q `find ' + reldir +' -name \*cpp`)
        exe_cmd('cd ' + kitdir + ' && tar cfvj cnc_' + release + '_pkg.tbz cnc')

        # exe_cmd( 'cd ' + reldir +'/samples; make -j clean CNCROOT=' + reldir +'> /dev/null' )
        # exe_cmd( 'cd distro/cnc/' + release + ' && sed -e \'s/\$CNCVER/' + release + '/g\' -e \'s/\$TBBVER/' + tbbver + '/g\' install.sh > install.sh.ver && mv install.sh.ver install.sh && chmod +x install.sh' )
        # exe_cmd( 'cd distro/cnc/' + release + ' && mv install.sh INSTALL.txt EULA.txt ../ && tar cfj - * | gpg --batch -c --passphrase "I accept the EULA" > ../cnc_b_' + release + '_install.files' )
        # exe_cmd( 'cd distro/cnc/' + release + ''
        #          ' && mkdir -p ' + tbbver + '/bin'
        #          ' && mkdir -p ' + tbbver + '/lib'
        #          ' && cp -r ' + tbbroot + '/include ' + tbbver + ''
        #          ' && cp -r ' + tbbroot + '/lib/intel64 ' + tbbver + '/lib/'
        #          #             ' && cp -r ' + tbbroot + '/lib/mic ' + tbbver + '/lib/'
        #          ' && cp ' + tbbroot + '/bin/tbbvars.*h ' + tbbver + '/bin/'
        #          ' && cp ' + tbbroot + '/README ' + tbbroot + '/CHANGES ' + tbbroot + '/COPYING ' + tbbver + ''
        #          ' && tar cfj ../' + tbbver + '_cnc_files.tbz ' + tbbver + '/' )
        # exe_cmd( 'cd distro && chmod +x cnc/install.sh && tar cfvz l_cnc_b_' + release + '.tgz cnc/install.sh cnc/EULA.txt cnc/cnc_b_' + release + '_install.files cnc/' + tbbver + '_cnc_files.tbz' )
        # exe_cmd( 'cd distro && zip -rP cnc l_cnc_b_' + release + '.zip cnc/install.sh cnc/EULA.txt cnc/cnc_b_' + release + '_install.files cnc/' + tbbver + '_cnc_files.tbz' )



# else:
#     #if False:
#   for arch in ARCHS:
#     for withTBB in [True, False]:
#       aip = 'cnc_installer_' + arch + '.aip'
#       shutil.copy( 'pkg/' + aip, 'distro' )
#       pkgstub = 'cnc_b_' + release + '_' + arch + ('_'+tbbver if withTBB == True else '')
#       tbbenv = ''
#       if withTBB == True:
#         tbbenv = ( 'AddFolder APPDIR\\'+tbbver+' '+tbbroot+'\\include\n'
#                    'AddFolder APPDIR\\'+tbbver+'\\lib '+tbbroot+'\\lib\\'+arch+'\n'
#                    'AddFolder APPDIR\\'+tbbver+'\\bin '+tbbroot+'\\bin\\'+arch+'\n'
#                    'AddFile APPDIR\\'+tbbver+'\\bin '+tbbroot+'\\bin\\tbbvars.bat\n'
#                    'AddFile APPDIR\\'+tbbver+' '+tbbroot+'\\README\n'
#                    'AddFile APPDIR\\'+tbbver+' '+tbbroot+'\\COPYING\n'
#                    'AddFile APPDIR\\'+tbbver+' '+tbbroot+'\\CHANGES\n'
#                    'NewEnvironment -name TBBROOT -value [APPDIR]\\' + tbbver + (' -install_operation CreateUpdate -behavior Replace\n' if withTBB == True else '\n') )
#       aic = 'distro\\' + pkgstub + '.aic'
#       inf = open( 'pkg/edit_aip.aic.tmpl' )
#       l = inf.read();
#       inf.close()
#       outf = open( aic, 'w' )
#       outf.write( l.format( CNCVER=release, ARCH=arch, PKGNAME=os.path.abspath('distro/w_'+pkgstub+'.msi'), TBBENV=tbbenv ) )
#       outf.close()
#       exe_cmd( os.path.normpath( r'"C:/Program Files (x86)/Caphyon/Advanced Installer 11.2/bin/x86/AdvancedInstaller.com"') + ' /execute distro\\' + aip + ' ' + aic )

# print "\nBuilding kit completed successfully\n"
