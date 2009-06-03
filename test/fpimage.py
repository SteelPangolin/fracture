#!/usr/bin/env python

from math import *
import struct
import sys
import os

import numpy
import Image

def readSt(f, fmt):
    buf = f.read(struct.calcsize(fmt))
    st = struct.unpack(fmt, buf)
    return st

def readSt1(f, fmt):
    return readSt(f, fmt)[0]

def writeSt(f, fmt, *fields):
    buf = struct.pack(fmt, *fields)
    f.write(buf)

def readFL32Image(filename):
    f = open(filename, 'rb')
    
    headerSigFmt = '!4s'
    sig = readSt1(f, headerSigFmt)
    if   sig == "23lf":
        headerAfterSigFmt = '<III'
        sampleFormat = '<f'
    elif sig == "fl32":
        headerAfterSigFmt = '>III'
        sampleFormat = '>f'
    else:
        raise SyntaxError("not an fl32 file")
    
    numChannels, width, height = readSt(f, headerAfterSigFmt)
    
    dataLen = width * height * numChannels
    dataType = numpy.dtype(sampleFormat)
    data = numpy.fromfile(f, dataType, dataLen)
    data = numpy.reshape(data, (height, width, numChannels)) # [y, x, channel]
    
    f.close()
    
    return data

def writeFL32Image(filename, data):
    if len(data.shape) == 2:
        height, width = data.shape
        numChannels = 1
    else:
        height, width, numChannels = data.shape
    data = numpy.asarray(data, numpy.dtype('<f'))
    buf = numpy.getbuffer(data)
    
    sig = "23lf"
    headerFmt = "<4sIII"
    
    f = open(filename, 'wb')
    writeSt(f, headerFmt, sig, numChannels, width, height)
    f.write(buf)
    f.close()
    
    print "wrote array as float dump: %s (%d x %d, %d channels)" \
        % (filename, width, height, numChannels)

def readPILImage(filename):
    img = Image.open(filename)
    width, height = img.size
    numChannels = len(img.getbands())
    
    dataLen = width * height * numChannels
    dataType = numpy.dtype('B')
    data = numpy.frombuffer(img.tostring(), dataType, dataLen)
    data = numpy.reshape(data, (height, width, numChannels)) # [y, x, channel]
    data = data / 255.0 # intentional, frombuffer returns read-only version
    
    return data

def writePILImage(filename, data):
    data = data * 255.0 # intentional, copy for output
    if len(data.shape) == 2:
        height, width = data.shape
        numChannels = 1
    else:
        height, width, numChannels = data.shape
    data = numpy.asarray(data, numpy.dtype('B'))
    buf = numpy.getbuffer(data)
    
    if numChannels == 1:
        mode = 'L'
    elif numChannels == 4:
        mode = 'RGBA'
    else:
        raise Exception()
    img = Image.frombuffer(mode, (width, height), buf, 'raw', mode, 0, 1)
    img.save(filename)
    
    fmt = os.path.splitext(filename)[1][1:].upper()
    print "wrote array as %s: %s (%d x %d, %d channels)" \
        % (fmt, filename, width, height, numChannels)

def log2int(x):
    return int(ceil(log(x, 2)))

def sumReduce(A, times):
    R = numpy.empty((A.shape[0] >> times, A.shape[1] >> times), numpy.dtype('f'))
    for j in xrange(R.shape[0]):
        for i in xrange(R.shape[1]):
            R[j, i] = numpy.sum(A[
                j << times : (j + 1) << times,
                i << times : (i + 1) << times])
    
    return R

def avgReduce(A, times):
    R = numpy.empty((A.shape[0] >> times, A.shape[1] >> times), numpy.dtype('f'))
    for j in xrange(R.shape[0]):
        for i in xrange(R.shape[1]):
            R[j, i] = numpy.sum(A[
                j << times : (j + 1) << times,
                i << times : (i + 1) << times])
    
    b = 1 << times
    R /= (b * b)
    
    return R

def calcS(n, sum_D, sum_D2, sum_r, sum_rD):
    S_lo = n * sum_D2 + sum_D * sum_D
    # TODO: if abs(s_lo) is close to zero, special handling
    # see: Fisher, p. 21
    S_hi = n * sum_rD + sum_r * sum_D
    S = S_hi / S_lo
    # old 'cap' mode
    S = numpy.clip(S, -1 + 1E-4, 1 - 1E-4)
    return S

def calcO(n, sum_D, sum_r, S):
    return (sum_r - S * sum_D) / n

def calcMSE(n, sum_D, sum_D2, sum_r, sum_r2, sum_rD, S, O):
    SE = S * (S * sum_D2 + 2 * (O * sum_D - sum_rD)) \
        + O * (n * O - 2 * sum_r) \
        + sum_r2
    return SE / n

def calcMSEAlt(n, sum_D, sum_D2, sum_r, sum_r2, sum_rD, S, O):
    # rearrange for speed
    # see: Fisher, p. 21
    SE = sum_r2 \
        + S * S * sum_D2 \
        + 2 * O * S * sum_D \
        - 2 * S * sum_rD \
        - 2 * O * sum_r \
        + n * O * O
    return SE / n

def calcMSEAltSlow(r_size, n, r_tiled, D, S, O):
    S_stretch = S.repeat(r_size, 0).repeat(r_size, 1)
    O_stretch = O.repeat(r_size, 0).repeat(r_size, 1)
    err = r_tiled - (S_stretch * D + O_stretch)
    return avgReduce(err * err, log2int(r_size))

def searchReduceCore(P1, P2):
    R1 = numpy.empty((P1.shape[0] >> 1, P1.shape[1] >> 1, 3), numpy.dtype('f'))
    R2 = numpy.empty((P2.shape[0] >> 1, P2.shape[1] >> 1, 3), numpy.dtype('f'))
    
    for j in xrange(R1.shape[0]):
        for i in xrange(R1.shape[1]):
            err,  s,  o  = P1[(j << 1),     (i << 1)]
            _,    x,  y  = P2[(j << 1),     (i << 1)]
            
            err2, s2, o2 = P1[(j << 1) + 1, (i << 1)]
            _,    x2, y2 = P2[(j << 1) + 1, (i << 1)]
            if err2 < err:
                err, s, o = (err2, s2, o2)
                _,   x, y = (err2, x2, y2)
            
            err2, s2, o2 = P1[(j << 1),     (i << 1) + 1]
            _,    x2, y2 = P2[(j << 1),     (i << 1) + 1]
            if err2 < err:
                err, s, o = (err2, s2, o2)
                _,   x, y = (err2, x2, y2)
            
            err2, s2, o2 = P1[(j << 1) + 1, (i << 1) + 1]
            _,    x2, y2 = P2[(j << 1) + 1, (i << 1) + 1]
            if err2 < err:
                err, s, o = (err2, s2, o2)
                _,   x, y = (err2, x2, y2)
            
            R1[j, i] = (err2, s, o)
            R2[j, i] = (err2, x, y)
    
    return (R1, R2)

def searchReduce(P1, P2, times):
    for t in xrange(times):
        P1, P2 = searchReduceCore(P1, P2)
    return (P1, P2)

def window(A, c, d):
    a = A.min()
    b = A.max()
    return ((A - a) * (d - c) / (b - a) + c).clip(c, d)

def encode():
    d_size = 8 * 2
    r_size = 4 * 2
    
    srcImg = readPILImage("../data/lena.png")[:, :, 0]
    
    R = srcImg
    sum_R = sumReduce(R, log2int(r_size))
    R2 = R * R
    sum_R2 = sumReduce(R2, log2int(r_size))
    
    D = avgReduce(srcImg, log2int(d_size) - log2int(r_size))
    sum_D = sumReduce(D, log2int(r_size))
    D2 = D * D
    sum_D2 = sumReduce(D2, log2int(r_size))
    
    I = numpy.empty(sum_D.shape, numpy.dtype('f'))
    J = numpy.empty(sum_D.shape, numpy.dtype('f'))
    for j in xrange(sum_D.shape[0]):
        for i in xrange(sum_D.shape[1]):
            I[j, i] = i
            J[j, i] = j
    
    transforms = []
    n = r_size * r_size
    for j in xrange(sum_R.shape[0]):
        for i in xrange(sum_R.shape[1]):
            r = R[
                j * r_size : (j + 1) * r_size,
                i * r_size : (i + 1) * r_size]
            r_tiled = numpy.tile(r, (D.shape[0] / r_size, D.shape[1] / r_size))
            sum_rD = sumReduce(r_tiled * D, log2int(r_size))
            
            S = calcS(n, sum_D, sum_D2, sum_R[j, i], sum_rD)
            O = calcO(n, sum_D, sum_R[j, i], S)
            MSE = calcMSE(n, sum_D, sum_D2, sum_R[j, i], sum_R2[j, i], sum_rD, S, O)
            
            P1 = numpy.dstack((MSE, S, O))
            P2 = numpy.dstack((MSE, I, J))
            P1, P2 = searchReduce(P1, P2, log2int(sum_D.shape[0]))
            err, s, o = P1[0, 0]
            _,   x, y = P2[0, 0]
            transform = (i, j, s, o, int(x), int(y)) # range block, scale, offset, domain block
            transforms.append(transform)
    
    return transforms

def decode(transforms):
    d_size = 8 * 2
    r_size = 4 * 2
    
    dstShape = (256, 256)
    
    R = numpy.zeros(dstShape, numpy.dtype('f')) + 0.5 # makes it converge faster?
    
    for t in xrange(10):
        D = avgReduce(R, log2int(d_size) - log2int(r_size))
        R = numpy.empty(dstShape, numpy.dtype('f'))
        for i, j, s, o, x, y in transforms:
            p = D[
                y * r_size : (y + 1) * r_size,
                x * r_size : (x + 1) * r_size]
            R[
                j * r_size : (j + 1) * r_size,
                i * r_size : (i + 1) * r_size] = s * p + o
        
        print "iteration =", t
        print "min =", numpy.amin(R)
        print "max =", numpy.amax(R)
        print "avg =", numpy.mean(R)
        print
        writePILImage("rcxn_%02d.png" % t, R)
    
    writePILImage("rcxn_last.png", window(R, 0, 1))

def main():
    transforms = encode()
    decode(transforms)

main()

def f2png(filename):
    img = readFL32Image(filename)
    print "min =", numpy.amin(img)
    print "max =", numpy.amax(img)
    print "avg =", numpy.mean(img)
    print "stddev =", numpy.std(img)

#f2png(sys.argv[1])