#!/usr/bin/python
from optparse import OptionParser
import datetime
import numpy as np
import sys

from calibrateData import calibratePPCTime


def parseCMDOpts():
    # http://docs.python.org/library/optparse.html
    usage="usage: %prog [options] bin_files"
    description="""This script processes ML-CORK bin files"""
    epilog="""Examples:"""
    
    help_n="""Skip stats--they can take some time and generate some clutter"""
    help_I="""Force RTC ID #. Supply id as hex integer (e.g. 0x8C). Default: 5th byte in file"""
    help_d="""Remove detected spikes by inserting linear interpolation."""
    help_p="""Plots the data. May not work if you do not have the right libaries installed"""
    help_i="""Just output statistics (and plot, optionally) and not the actual data"""
    help_t="""Print calibrated timestamps on every line emulating NC logfiles, at the same time."""
    help_f="""Modify timestamp format defaults is '%Y%m%d %H:%M:%S'.
              Use -f '%Y%m%dT%H%M%S.000Z' to emulate NEPTUNE Canada log files. """
    help_b="""Safe a binary file skipping problematic records"""
    
    parser=OptionParser(usage=usage, description=description, epilog=epilog)
    parser.add_option("-I","--RTC_ID",type="int",default=None,help=help_I)
    parser.add_option("-n","--no_stats",action="store_false",dest='statistics',default=True,help=help_n)
    parser.add_option("-s","--spaces",action="store_true",dest='spaces',default=False)
    parser.add_option("-a","--print_all",action="store_true",dest='printAll',default=False)
    parser.add_option("-p","--plot_data",action="store_true",dest='doPlots',default=False,\
                                                                                help=help_p)
    parser.add_option("-i","--info_only",action="store_true",dest='info',default=False,help=help_i)
    parser.add_option("-t","--timestamps",action="store_true",dest='writeTimestamp',default=False,\
                                                                                            help=help_t)
    parser.add_option("-f","--timestampFMT",type="string",dest='timestampFMT',\
                                    default='%Y-%m-%d %H:%M:%S',help=help_f)
    parser.add_option("-d","--despike",action="store_true",dest='interpSpikes', default=False,help=help_d)
    parser.add_option("-b","--binary_file",type="string",default=None,dest='binaryFile',help=help_b)
    
    # '%Y%m%d %H:%M:%S'
    # '%Y%m%dT%H%M%S.000Z' NC format
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
        
def getStatistics(Data, loggerID=None,do_plots=False,interp_spikes=False):
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
    #print vData.shape
    
    print 'Time of first alinged reading: %s' % calibratePPCTime(vData['Time'][0]).strftime(options.timestampFMT) 
    print 'Time of last  alinged reading: %s' % calibratePPCTime(vData['Time'][-1]).strftime(options.timestampFMT) 
    
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
    
    # Create sample spikes
    #Freqs[1][0]=0
    #Freqs[2][0]=0
    #Freqs[3][0]=0
    #Freqs[1][4]=0
    
    print Freqs.shape
    print Freqs
    for SensNo in range(Freqs.shape[1]):
        print '=== Sensor %d ===' % SensNo
        i=1
        spikeDetect=np.where(np.abs(np.diff(Freqs[0:,SensNo]/1e9,axis=0))>0.01)
        # print spikeDetect
        while i< len(spikeDetect[0]):
            if (spikeDetect[0][i]-spikeDetect[0][i-1])<=3: 
                # Spike can be three samples long, at most...
                if Freqs[spikeDetect[0][i]][SensNo] == 0:
                    print 'Zero spike!'
                else:
                    print 'Spike!!!'
                    # Has to be loop!!! spikeDetect[0][i-1]+1:spikeDetect[0][i]
                    # Freqs[spikeDetect[0][i-1]+1][SensNo]=0
                spikeStart=spikeDetect[0][i-1]+1
                spikeEnd=spikeDetect[0][i]
                spikeRange=range(spikeStart,spikeEnd+1)
                spikeFill=np.interp(spikeRange,[spikeStart-1, spikeEnd+1],[Freqs[spikeStart-1,SensNo],Freqs[spikeEnd+1,SensNo]])
                print spikeRange, spikeFill
                if interp_spikes:
                    Freqs[spikeRange,SensNo]=spikeFill
                i+=2
            else:
                # No spike
                print 'No Spike...'
                i+=1
    #print np.float(Freqs)    
    spikes=np.where(np.abs(np.diff(Freqs/1e9,axis=0))>0.01)
    
    NFreqs=Freqs.copy()
    print NFreqs[0:5]
    print NFreqs.shape
    for i in range(0,NParo):
        NFreqs[...,:,i]=Freqs[...,:,i]-np.median(Freqs[...,:,i])
        print np.median(Freqs[...,:,i])
    
    if do_plots:
        try:
            import matplotlib.pyplot as plt
            from matplotlib.dates import AutoDateLocator, AutoDateFormatter
            #print '%X' % Ti[0]
            ax1=plt.subplot(311)
            t=[calibratePPCTime(Secs) for Secs in vData['Time']]
            td=(vData['Time']-vData['Time'][0])/86400.0
            #quit()
            #plt.plot_date(t, Ti, fmt='bo',xdate=True,linestyle='-',marker='None')
            #dateLoc=AutoDateLocator()
            #ax1.xaxis.set_major_locator(dateLoc)
            #ax1.xaxis.set_major_formatter(AutoDateFormatter(dateLoc))
            plt.plot(td,Ti)
            #plt.plot(t,Ti,label='Ti')
            plt.ylabel('T int')
            
            plt.subplot(312, sharex=ax1)
            plt.plot(td,Freqs,label=' ')
            plt.legend()
            #plt.plot(td,np.abs(np.diff(Freqs/1e9,axis=0)),label='Fre')
            #(Freqs+4294967296)*4.656612873e-9
            #-2206988218
            #plt.plot(t,NFreqs-2206988218,label='Frequs')
            
            plt.subplot(313, sharex=ax1) #
            #plt.plot(td[1:],np.abs(np.diff(Freqs/1e9,axis=0)))
            plt.plot(td[spikes[0]],spikes[1]+1,'*',label='spike')
            plt.plot(td[spikes[0]],spikes[1]+1,'+',label='zero')
            plt.grid('on')
            plt.ylabel('Freq channel')
            plt.xlabel('Days since %s' % t[0].strftime(options.timestampFMT))
            plt.legend()
            plt.show()
        except:
            print 'Could not do plot, you probably have to install matplotlib!!!'
            print 'Error: ',  sys.exc_info()


def stripTrash(Data):
    loggerID=0x5c
    IdIdx=np.where(Data == loggerID)[0]
    
    
if __name__=='__main__':
    (options, args) = parseCMDOpts()
    
    if args:
        Data=readBinFile(args[0])
    else:
        Data=readBinFile()
    
    if options.statistics:
        getStatistics(Data,do_plots=options.doPlots,interp_spikes=options.interpSpikes)
    
    if options.info:
        # Don't return the actual data and quit right here
        quit()
    
    if options.binaryFile:
        binFile=open(options.binaryFile,'wb')
    
    NBytes=len(Data)
    print NBytes
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
    
    # Patch the data with a few records of logger IDs at the to simplify consistency checking
    Data=np.concatenate((Data,loggerID*np.ones(3*recLen, dtype=Data.dtype)),axis=1)
    #IdxGood=np.where(np.logical_and(((Data[IdIdx[0:-2]]-Data[IdIdx[0:-2]+recLen]) == 0), (Data[IdIdx[0:-2]+recLen-5]==0)))[0]
    IdxGood=np.where(np.logical_and(((Data[IdIdx]-Data[IdIdx+recLen]) == 0), (Data[IdIdx+recLen-5]==0)))[0]
    #IdxGood=np.where(Data[IdIdx[0:-2]+recLen-5]==0)[0]
    IdIdx=IdIdx[IdxGood]-4
    lastIdx=-recLen
    recordErrors=0
    goodRecords=0
    LastTime=0
    print IdIdx.flat[0]
    print len(IdIdx.flat)
    for idx in IdIdx.flat:
        dIdx=(idx-lastIdx)
        if dIdx < recLen:
            # assuming match by accident
            continue
            
        CurrTime=int(4*'%02x' % tuple(Data[idx:idx+4].tolist()),16)
        if CurrTime < LastTime:
            print "-> Time Problem"
        LastTime=CurrTime
        
        if options.writeTimestamp:
            sys.stdout.write('%s ' % calibratePPCTime(CurrTime).strftime(options.timestampFMT))
            # '%Y%m%d %H:%M:%S'
            # '%Y%m%dT%H%M%S.000Z' NC format
            
        if not (dIdx == recLen):
            recordErrors += 1
            if options.printAll:
                print '=================== %d =======================' % dIdx
                if not options.writeTimestamp:
                    print calibratePPCTime(CurrTime).strftime(options.timestampFMT)
          
                if lastIdx<0:
                    # Required if the bin file starts with garbage
                    lastIdx=0
                
                garbage=tuple(Data[lastIdx:idx].tolist())
                print len(garbage)*'%02x' % garbage
                print '--------------------------------------------'
                
        if not options.spaces:
            print (((recLen)/4*'%02X%02X%02X%02X')+'%02X') \
                       % tuple(Data[idx:idx+recLen].tolist())
        else:
            print ( '%02X%02X%02X%02X %02X %02X%02X%02X '+  \
                   ((recLen-8)/4*'%02X%02X%02X%02X ')+'%02X') \
                     % tuple(Data[idx:idx+recLen].tolist())
        if options.binaryFile:
            #Data[idx:idx+recLen].tofile(binFile)
            binFile.write(Data[idx:idx+recLen].tostring(order=None))
        goodRecords += 1
        if goodRecords == 1:
            firstTime=CurrTime
            
        lastIdx=idx
    
    print "Possible Records: %.1f" % (NBytes/float(recLen))
    print "Good Records:     %d   (%s -> %s)" % (goodRecords,
                        calibratePPCTime(firstTime).strftime(options.timestampFMT),
                        calibratePPCTime(CurrTime).strftime(options.timestampFMT))
    
    print "%d unused bytes (%.1f records) at the end" % (NBytes-(lastIdx+recLen), (NBytes-(lastIdx+recLen))/float(recLen))
    print "Record errors: %d" % recordErrors
    
    if options.binaryFile:
        binFile.close()
    
# Change type of an array        
#    b.dtype=np.dtype([('a',np.int16),('b',np.int16),('c',np.int32)])
