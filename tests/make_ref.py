import sys
import re


def make_ref(argv):
    filePattern = open(sys.argv[1], 'r')
    compLines = []
    for line in filePattern.readlines():
        compLines.append(line.rstrip('\r\n'))
    filePattern.close()
    compLines.sort()

    for line in compLines:
        print re.sub( r'([\(\)\[\]\.\*\?\+])', r'\\\1', line )

if __name__ == "__main__": sys.exit(make_ref(sys.argv[1:]))
