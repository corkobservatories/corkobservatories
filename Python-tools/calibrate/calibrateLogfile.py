#!/usr/bin/python
from optparse import OptionParser
from datetime import datetime
import re
import sys
#import datetime
#import numpy as np
#import matplotlib.pyplot as plt

def parseCMDOpts():
    # http://docs.python.org/library/optparse.html
    usage="usage: %prog [options] logfile.log"
    description="""This script find raw data in mlterm logfiles and calibrates it"""
    parser=OptionParser(usage=usage, description=description)
    parser.add_option("-c","--calibration",type="string",default=None)
    # parser.add_option("-l","--logfile",type="string",default='test.log')
    return parser.parse_args()
        
if __name__=='__main__':
    from calibrateData import calibratePPCTime
    from sys import exc_info
    import calibrateData as CD
    
    (options, args) = parseCMDOpts()
    if len(args) == 0:
        fileName='STDIN'
        f=sys.stdin
    else:
        fileName=args[0]
        f=open(args[0])
        
    
    print '# ' + datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    print '# Input: ' + fileName
    
    try:
        calibrateData = getattr(CD, 'calibrate_'+options.calibration)
        print '# Calibration: calibration_' + options.calibration + '() in calibrateData.py'
    except:
        raise 'Could not locate calibration in calibrateData.py'
    
    for line in f:
        try:
            # Minimum sample with time, id, Tint, 1 Paros, and 00 has 26 characters
            hexBlock=re.search(r'[\dA-Fa-f]{26,}',line)
            #print hexBlock.group(0)
            x=re.findall(r'[\dA-Fa-f]{8}',hexBlock.group(0))
            x[1]=x[1][2:]
            t=calibratePPCTime(int(x[0],16)).strftime('%Y-%m-%d %H:%M:%S')
            xData=[int(xi,16) for xi in x[1:]]
            Data=calibrateData(xData)
            if len(Data)==len(xData):
                print t,(len(Data)*'%.4f ') % tuple(Data)
            #print t,(len(Data)*'%.4f ') % tuple(Data)
        except:
            #print exc_info()[0]
            pass
    f.close()
