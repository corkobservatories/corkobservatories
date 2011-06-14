"""
Basic functions needed to calibrate ML-CORK / BPR data.

More text to describe what's going on
"""

import numpy as np
import datetime

import sys, os
ConfigPath = os.path.abspath(os.path.dirname(__file__))
if not os.path.isdir(ConfigPath):
    ConfigPath = os.getcwd()

def readParoCoeffs():
    """ 
    Read coeff from parosci.txt file.
    
    These coeffs are used later on
    """
    Coeffs={}
    f=open(os.path.join(ConfigPath,'parosci.txt'))
    N=0
    while True:
        try:
            N += 1
            # print '======= ', N
            line=f.readline()
            ParoID=int(line.split()[0])
            
        except:
            # print "Done!!!!!!!!!!!"
            break
            
        IDCoeffs={}
        for i in range(1,12):
            line=f.readline().split()
            IDCoeffs[line[0]]=float(line[1])
        Coeffs[ParoID]=IDCoeffs
        # print ParoID
        # print IDCoeffs
    f.close()
    
    return Coeffs

def readPlatinumCoeffs():
    Coeffs={}
    f=open(os.path.join(ConfigPath,'platinum.txt'))
    N=0
    while True:
        try:
            N += 1
            # print '======= ', N
            line=f.readline()
            PlatID=int(line.split()[0],16) # Hex id
            line1=f.readline().split()
            line2=f.readline().split()
            
        except:
            # print "Done!!!!!!!!!!!"
            break
        
        Coeffs[PlatID]={'a': float(line1[1]), 'b': float(line2[1])}
        # print '%X' % PlatID, Coeffs[PlatID]
    f.close()
    
    return Coeffs


PlatinumCoeffs=readPlatinumCoeffs()    
def getPlatinumCoeffs(ID=None):
    try:
        return PlatinumCoeffs[ID]
            
    except KeyError:
        warn('Using default temperature calibration!!!!')
        return {'a': -2.95083e-006, 'b' : 40.0678 }

ParoCoeffs=readParoCoeffs()        
def getParoCoeffs(ID=None):
    try:
        # print readParoCoeffs()[ID]
        return ParoCoeffs[ID]
    except KeyError:
        raise Exception, 'You have to supply a valid probe serial # !!!!'

    
def calibrateParoT(xFT,Coeffs=None):
    """Calibrate temperatures from Paroscientifc Type-II gauges.
    
    
    """
    C=Coeffs
    U=((xFT+4294967296)*4.656612873e-9/4)-C['U0']
    return C['Y1']*U+C['Y2']*np.power(U,2)
        

def calibratePlatinum(xT,Coeffs=None,ID=None):
    """Calibrate temperatures from platinum chip sensors.
    
    The conversion from A/D counts to temperatures in degC is a simple linear relation ship.
    
    .. math::
       
       T = a \cdot x + b
    """
    C=Coeffs
    return C['a']*xT+C['b']
    
def calibrateParoP(xFP,Coeffs=None,xFT=None, Temp=None):
    C=Coeffs
    isTypeI= C['U0']==0 # U0 is not zero for Type II probes
    
    if xFT==None and Temp==None:
        # Make sure some kind of compensation is done
        warn('Using constant temperature 0 degC !!!!')
        Temp=0
        
    if not isTypeI:
        # Compute Type-II compensation frequency U
        if xFT==None:
            # Compute compensation frequency if temperature is given for Type-II probes
            U= -(C['Y1']+np.sqrt(np.power(C['Y1'],2)+4*C['Y2']*Temp))/(2*C['Y2'])
        else:
            # Raw temperature freq count to compensatin freq U
            U=(((xFT+4294967296)*4.656612873e-9)/4)-C['U0'];
    else:
        # For Type-I compensate (U) with temperature
        if Temp==None:
            # Make sure some kind of compensation is done
            warn('Using constant temperature 0 degC !!!!')
            Temp=0
        U=Temp
        
    # Do the proper compensation with U
    T= (xFP+4294967296)*4.656612873e-9;
    CU= C['C1']+ C['C2']*U +C['C3'] * np.power(U,2);
    D= C['D1'];
    T0= C['T1'] + C['T2']*U + C['T3'] * np.power(U,2) + C['T4'] * np.power(U,3);
    P= CU * (1- (np.power(T0,2) / np.power(T,2))) * (1-D*(1- (np.power(T0,2) / np.power(T,2))));
    return P*6.894757/10 # Pressure in decibar

def calibratePPCTime(xt=0x2A4E2328):
    # convert and calibrate (TODO) PPC time counts to python datetimes
    if not getattr(xt,'__iter__',False):
        t=datetime.datetime(1988,1,1)+datetime.timedelta(seconds=int(xt))
    else:
        t=[datetime.datetime(1988,1,1)+datetime.timedelta(seconds=int(Secs)) for Secs in xt]
    return t 
        
def calibrate_1027C(Raw=[0x485484, 0x289F3D18, 0x7F7ABA66, 0x838C3082, 0x846E4B05, 0x7F501E6C]):
    RTC_ID=0x5C
    Out=[]
    CP_SF=getParoCoeffs(93976)
    CP_S1=getParoCoeffs(94000)
    CP_S2=getParoCoeffs(94223)
    CP_S3=getParoCoeffs(106853)
    CT_Ti=getPlatinumCoeffs(0x96)
    Out.append(calibratePlatinum(Raw[0],Coeffs=CT_Ti))
    Out.append(calibrateParoT(Raw[1],Coeffs=CP_SF))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_S1,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[3],Coeffs=CP_S2,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[4],Coeffs=CP_S3,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[5],Coeffs=CP_SF,xFT=Raw[1]))
    return Out
    
def calibrate_889E(Raw=[0x554E8A,0x2ABDD2BB,0x82907AA7,0x8125BB8A,0x7ECD98A3,0x838E5360,0x7ED87781]):
    """Calibration for IODP 889E (has new name now) data.
    
    Here are the results of an example calibration:
    
    >>> calibrate_889E(Raw=[5590666, 717083323, 2190506663, 2166733706, 2127403171, 2207142752, 2128115585])
    [23.626078129440003, 22.276150361949441, 11.103497907947677, 10.510306334848947, 522.67979987438071, -517.29555399981541, 11.071455420816344]
    """
    RTC_ID=0x08
    Out=[]
    CP_SF=getParoCoeffs(119147)
    CP_S1=getParoCoeffs(119141)
    CP_S2=getParoCoeffs(119144)
    CP_S3=getParoCoeffs(119145)
    CP_S4=getParoCoeffs(119146)
    CT_Ti=getPlatinumCoeffs(0x92)
    Out.append(calibratePlatinum(Raw[0],Coeffs=CT_Ti))
    Out.append(calibrateParoT(Raw[1],Coeffs=CP_SF))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_S1,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[3],Coeffs=CP_S2,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[4],Coeffs=CP_S3,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[5],Coeffs=CP_S4,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[6],Coeffs=CP_SF,xFT=Raw[1]))
    return Out

def calibrate_SR2A(Raw=[0x5A28E6, 0x80B536A8, 0x85201A2B, 0x7DF02102]):
    RTC_ID=0x76
    Out=[]
    CP_SF=getParoCoeffs(89098)
    CP_S1=getParoCoeffs(75738)
    CP_S2=getParoCoeffs(75739)
    CT_Ti=getPlatinumCoeffs(0x97)
    Out.append(calibratePlatinum(Raw[0],Coeffs=CT_Ti))
    Out.append(calibrateParoP(Raw[1],Coeffs=CP_S1,Temp=Out[0]))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_S2,Temp=Out[0]))
    Out.append(calibrateParoP(Raw[3],Coeffs=CP_SF,Temp=Out[0]))
    return Out

def calibrate_SR2B(Raw=[0x5A6B14, 0x8318A3C2, 0x80EDC755]):
    RTC_ID=0x89
    Out=[]
    CP_SF=getParoCoeffs(106096)
    CP_S1=getParoCoeffs(106095)
    CT_Ti=getPlatinumCoeffs(0x98)
    Out.append(calibratePlatinum(Raw[0],Coeffs=CT_Ti))
    Out.append(calibrateParoP(Raw[1],Coeffs=CP_S1,Temp=Out[0]))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_SF,Temp=Out[0]))
    return Out

def calibrate_Endeavour_BPR83(Raw=[0x62C101, 0x2A3FFD76, 0x80DC9B7D]):
    RTC_ID=0x83
    Out=[]
    CP_SF=getParoCoeffs(119139)
    CT_Ti=getPlatinumCoeffs(0x87)
    Out.append(calibratePlatinum(Raw[0],Coeffs=CT_Ti))
    Out.append(calibrateParoT(Raw[1],Coeffs=CP_SF))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_SF,xFT=Raw[1]))
    return Out

def calibrate_Barkley_BPR78(Raw=[0xB16C74, 0x2C4BFE29, 0x7AD37C04]):
    RTC_ID=0x78
    Out=[]
    CP_SF=getParoCoeffs(93969)
    CT_Ti=getPlatinumCoeffs(0x8A)
    Out.append(calibratePlatinum(Raw[0],Coeffs=CT_Ti)-0.1867)
    Out.append(calibrateParoT(Raw[1],Coeffs=CP_SF))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_SF,xFT=Raw[1]))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_SF,Temp=Out[0]))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_SF,Temp=5.578))
    return Out

def calibrate_HeissCalib(Raw=[0x5DFB8B, 0x2946795B, 0x7F117EC8]):
    RTC_ID=0x58
    Out=[]
    CP_SF=getParoCoeffs(116527)
    CT_Ti=getPlatinumCoeffs(0x91)
    Out.append(calibratePlatinum(Raw[0],Coeffs=CT_Ti))
    Out.append(calibrateParoT(Raw[1],Coeffs=CP_SF))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_SF,xFT=Raw[1]))
    return Out

def calibrate_NT_C10_SmartPlug(Raw=[0x4DFECE,0x2A56471B,0x810EF6F3,0x7FA0622E]):
    RTC_ID=0x57
    Out=[]
    CP_SF=getParoCoeffs(106100)
    CP_S1=getParoCoeffs(106043)
    CT_Ti=getPlatinumCoeffs(0x94)
    Out.append(calibratePlatinum(Raw[0],Coeffs=CT_Ti))
    Out.append(calibrateParoT(Raw[1],Coeffs=CP_S1))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_S1,xFT=Raw[1]))
    Out.append(calibrateParoP(Raw[3],Coeffs=CP_SF,Temp=Out[1]))
    return Out

def calibrate_DoNet1(Raw=[0x5C9A97,0x2C2C0084,0x8184FF79,0x817E1C06,0x8055EE4D,0x7E72B834]):
    RTC_ID=0x01
    Out=[]
    CP_SF=getParoCoeffs(108829)
    CP_S1=getParoCoeffs(106097)
    CP_S2=getParoCoeffs(106098)
    # CP_S3=getParoCoeffs(106099) # Drops out at 1Hz
    CP_S3=getParoCoeffs(106101)
    CT_Ti=getPlatinumCoeffs(0x93)
    Out.append(calibratePlatinum(Raw[0],Coeffs=CT_Ti))
    Out.append(calibrateParoT(Raw[1],Coeffs=CP_SF))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_S1,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[3],Coeffs=CP_S2,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[4],Coeffs=CP_S3,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[5],Coeffs=CP_SF,xFT=Raw[1]))
    return Out

def calibrate_DoNet2_OtherGauges(Raw=[0x5BB02E,0x2C2EE772,0x818520A1,0x817DEA2D,0x7FDCAAB3,0x7E7277B5]):
    RTC_ID=0x05
    Out=[]
    CP_SF=getParoCoeffs(108829)
    CP_S1=getParoCoeffs(106097)
    CP_S2=getParoCoeffs(106098)
    CP_S3=getParoCoeffs(106099)
    CT_Ti=getPlatinumCoeffs(0x93)
    Out.append(calibratePlatinum(Raw[0],Coeffs=CT_Ti))
    Out.append(calibrateParoT(Raw[1],Coeffs=CP_SF))
    Out.append(calibrateParoP(Raw[2],Coeffs=CP_S1,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[3],Coeffs=CP_S2,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[4],Coeffs=CP_S3,Temp=Out[1]))
    Out.append(calibrateParoP(Raw[5],Coeffs=CP_SF,xFT=Raw[1]))
    return Out

    
if __name__=='__main__':
#    Coeffs=readParoCoeffs()
    print calibrate_1027C()
    print calibrate_SR2A()
    print calibrate_SR2B()
    print calibrate_Endeavour83()
    print calibrate_HeissCalib()
    print calibrate_NT_C10_SmartPlug()
    print calibrate_889E()
    print calibrate_DoNet2_OtherGauges()
    print calibrate_DoNet1()
    print calibratePPCTime().strftime('%Y-%m-%d %H:%M:%S')
    
    import doctest
    doctest.testmod()