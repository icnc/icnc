import sys
import re
import difflib

def CompareBinFile(f1, f2):
    file1 = open( f1 )
    lines1 = []
    for line in file1.readlines():
        lines1.append( line )
    file1.close()
    file2 = open( f2 )
    lines2 = []
    for line in file2.readlines():
        lines2.append(line)
    file2.close()
    sm = difflib.SequenceMatcher(None,lines1,lines2)
    if sm.ratio() < 1:
        print "Error: binary files do not match."
        return True
    return False

def CompareFile(fp, fc):
    fileCompare = open( fc )
    compLines = []
    for line in fileCompare.readlines():
        compLines.append(line.rstrip('\r\n'))
    fileCompare.close()
    compLines.sort()

#    print '\n'.join(compLines)

    patterns = []
    filePattern = open( fp )
    for line in filePattern.readlines():
        line = line.rstrip('\r\n')
        pat = re.compile(line)
        patterns.append((pat,line))
    filePattern.close()

    # This is a simple greedy algorithm.  We should backtrack and look for other matches
    # if it fails.
    #

    while patterns:
        (pat, x) = patterns[0]
        while compLines and not pat.match(compLines[0]):
            compLines.pop(0)
        if not compLines:
            PrintNoMatch(patterns)
            return True
        compLines.pop(0)
        patterns.pop(0)
    return False


def PrintNoMatch(patList):
    print "Error: Remaining patterns do not match:"
    for (x, patStr) in patList:
        print ">> " + patStr


if __name__ == "__main__":
    if sys.argv[3] == "txt":
        sys.exit(CompareFile(sys.argv[1], sys.argv[2]))
    else:
        sys.exit(CompareBinFile(sys.argv[1], sys.argv[2]))
