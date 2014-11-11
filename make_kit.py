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
