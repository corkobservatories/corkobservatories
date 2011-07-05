#!/usr/bin/python
from optparse import OptionParser
import datetime
import numpy as np


def parseCMDOpts():
    # http://docs.python.org/library/optparse.html
    usage="usage: %prog [options] bin_files"
    description="""This script processes ML-CORK bin files"""
    epilog="""Examples:"""
    
    help_n="""Put no spaces between fields in dump. This is the default so that the output can
              be processed by calibrateLogfile.py."""
    help_I="""Force RTC ID #. Supply id as hex integer (e.g. 0x8C). Default: 5th byte in file"""
    help_p="""Plots the data. May not work if you do not have the right libaries installed"""
    
    parser=OptionParser(usage=usage, description=description, epilog=epilog)
    parser.add_option("-I","--RTC_ID",type="int",default=None,help=help_I)
    parser.add_option("-n","--no_spaces",action="store_false",dest='spaces',help=help_n)
    parser.add_option("-s","--spaces",action="store_true",dest='spaces',default=False)
    parser.add_option("-a","--print_all",action="store_true",dest='printAll',default=False)
    parser.add_option("-p","--plot_data",action="store_true",dest='doPlots',default=False,\
                                                                                help=help_p)
    
    return parser.parse_args()
    

def readBinFile(binFileName='1027C_Weekend.bin'):
    # from struct import unpack
    #binFile= open(binFileName,'rb')
    #DataStr=binFile.read()
    #binFile.close()
    #Data=array('B')
    #Data.fromstring(DataStr)
    Data= np.fromfile(file=binFileName, dtype=np.uint8)
    return Data
    
def calCoeffs(loggerID):
    pass

def recordLength(Data, loggerID=None):
    if not loggerID:
        loggerID=Data[4]
    
    IdIdx=np.where(Data == loggerID)[0]
    recLen=int(np.round(np.median(np.diff(IdIdx))))    
    return recLen
        
def getStatistics(Data, loggerID=None,do_plots=False):
    if not loggerID:
        loggerID=Data[4]
    
    print 'RTC ID: %02X' % loggerID
    print 'Data bytes: %d' % len(Data)
    recLen=recordLength(Data,loggerID)
    print 'Bytes per record: %d' % recLen
    NRecs=len(Data)/float(recLen)
    print 'Possible records: %.2f' % NRecs
    NRecs=np.floor(NRecs)
    NParo=np.round(((recLen-9)/4))
    print 'Parosci channels: %d' % NParo
    print 'Aligned IDs: %d' % len(np.where(Data[4::recLen]==loggerID)[0])
    print 'Aligned trailing 00s: %d' % len(np.where(Data[recLen-1::recLen]==0x00)[0])
    #view=Data.view(dtype=[('date', np.int32),('id',np.int8),('Ti_1',np.int8),('Ti_2',np.int16),('Paros',np.int32,NParo),('Zero',np.int8)])
    #vData=Data.view(dtype=[('date', 'i1', (1,28))])
    #np.reshape(Data,(NRecs,-1))
    #print NRecs*recLen
    
    #print Data.shape
    #print Data.dtype
    
    vData=Data[0:recLen*NRecs].view()
    vData.dtype=[('Time','>u4'),('IDTemp','>u4'),('Data','>u4',(1,NParo)),('Zeros','>u1')]
    #vData.dtype=[('rec',('date', np.int32,(1,7)),('Zeros',np.int8))]
    #vData.dtype=[('date', '>u1',(1,16)),('date2', '>u1',(1,1))]
    print vData.shape
    
    #print NParo*'%X ' % tuple(vData['Data'][1][0].tolist())
    #print '%d' % len(vData)
    
    NZeros=len(np.where(vData['Data']==0)[0])
    print "%d (%f %%) Paro readings are zero" % (NZeros, 100*NZeros/(NRecs*NParo))
    
    Dt=np.diff(vData['Time'])
    try:
        Dts, idxs =np.unique(Dt, return_inverse=True)
        print "Time differences\n Dt ---      #"
        for i in range(0,len(Dts)):
            print '%3d --- %7d' % (Dts[i], len(np.where(idxs==i)[0]))
    except:
        print 'Old vergsion of numpy, cannot do gap analysis...!'
    
    # --- remove ID from internal temperatures ----     
    Ti=-2.95083e-006 * np.bitwise_and(vData['IDTemp'],0x00FFFFFF) + 40.0678
    Freqs=vData['Data'].reshape((NRecs,-1))
    NFreqs=Freqs.copy()
    print NFreqs[0:5]
    print NFreqs.shape
    for i in range(0,NParo):
        NFreqs[...,:,i]=Freqs[...,:,i]-np.median(Freqs[...,:,i])
        print np.median(Freqs[...,:,i])
    
    if do_plots:
        try:
            import matplotlib.pyplot as plt
            #print '%X' % Ti[0]
            ax1=plt.subplot(211)
            t=[datetime.datetime(1988,1,1)+datetime.timedelta(seconds=int(Secs)) for Secs in vData['Time']]
            #quit()
            plt.plot(vData['Time']-vData['Time'][0],Ti)
            #plt.plot(t,Ti,label='Ti')
            plt.ylabel('T int')
            plt.subplot(212, sharex=ax1)
            plt.plot(vData['Time']-vData['Time'][0],NFreqs-2206988218)
            #plt.plot(t,NFreqs-2206988218,label='Frequs')
            plt.legend()
            plt.show()
        except:
            print 'Could not do plot, you probably have to install matplotlib!!!'
def stripTrash(Data):
    loggerID=0x5c
    IdIdx=np.where(Data == loggerID)[0]
    
    
if __name__=='__main__':
    (options, args) = parseCMDOpts()
    
    if args:
        Data=readBinFile(args[0])
    else:
        Data=readBinFile()
    
    getStatistics(Data,do_plots=options.doPlots)
    
    
    
    print len(Data)
    print type(Data)
    np.set_printoptions(threshold=10000)
    
    loggerID=options.RTC_ID    
    if not loggerID:
        loggerID=Data[4]
        
    recLen=recordLength(Data,loggerID=loggerID)
    
    print recLen
#    recType=np.dtype([('t',np.uint32),('ID_Ti',np.uint32),('Freqs',(np.uint32,(1,(recLen-9)/4))),('trailer',np.uint8)]);
#    print recType
#    a=np.arange(2,10)
#    print a.dtype
#    newRec=np.array(Data[0:29])
#    newRec.dtype=recType
    #np.append(newRec,Data[0:29])
    #,dtype=recType
#    print newRec
#    quit()

    IdIdx=np.where(Data == loggerID)[0]
    IdxGood=np.where(np.logical_and(((Data[IdIdx[0:-2]]-Data[IdIdx[0:-2]+recLen]) == 0), (Data[IdIdx[0:-2]+recLen-5]==0)))[0]
    IdIdx=IdIdx[IdxGood]-4
    lastIdx=0
    recordErrors=0
    LastTime=0
    for idx in IdIdx.flat:
        dIdx=(idx-lastIdx)
        if dIdx < recLen:
            continue
        CurrTime=int(4*'%02x' % tuple(Data[idx:idx+4].tolist()),16)
        if CurrTime < LastTime:
            print "-> Time Problem"
        LastTime=CurrTime
        
        if not (dIdx == recLen):
            recordErrors += 1
            if options.printAll:
                print '=================== %d =======================' % dIdx
                print dIdx*'%02x' % tuple(Data[lastIdx:idx].tolist())
                print '--------------------------------------------'
        if not options.spaces:
            print (((recLen)/4*'%02X%02X%02X%02X')+'%02X') \
                       % tuple(Data[idx:idx+recLen].tolist())
        else:
            print ( '%02X%02X%02X%02X %02X %02X%02X%02X '+  \
                   ((recLen-8)/4*'%02X%02X%02X%02X ')+'%02X') \
                     % tuple(Data[idx:idx+recLen].tolist())
        lastIdx=idx
    print "Record errors: %d" % recordErrors
    
# Change type of an array        
#    b.dtype=np.dtype([('a',np.int16),('b',np.int16),('c',np.int32)])
