
import time
import os
import os.path
import shutil
import random
import csv
import json
from subprocess import check_call, Popen

import numpy
from numpy import fft


def main():
    out_path = 'duo.wav'
    res_path = 'results.csv'
    
    res_file = open(res_path, 'a')

    settings = None
    with open('settings.json', 'r') as json_file:
        settings = json.load(json_file)
        
    if not os.path.exists(settings['duowav_path']):
        raise RuntimeError(
            'DuoWAV executable not found at %s' % settings['duowav_path'])

    sample_rate = 2e6 / settings['decimation']
    delta_t = 1 / sample_rate
    base_cmd = [
        settings['duowav_path'], '-f', '-o',
        '-w', str(settings['warmup']),
        '-d', str(settings['decimation']),
        '-l', str(settings['lna_state'])]

    stations = []
    with open('stations.csv', 'r') as csv_file:
        reader = csv.reader(csv_file)
        stations = [x for x in reader]
        
    random.shuffle(stations)
 
    for freq, callsign in stations:
        print('Testing %s MHz' % freq)
        freq_val = int(float(freq) * 1e6)
        if os.path.exists(out_path):
            os.remove(out_path)
        cmd = base_cmd + [str(freq_val), str(settings['file_size']), out_path]
        check_call(cmd)
        data = numpy.fromfile(out_path, dtype=numpy.complex64)
        data = data.reshape([-1,2])
        chan_a = data[:,0].ravel()
        chan_b = data[:,1].ravel()
        fft_a = fft.fft(chan_a).conjugate()
        fft_b = fft.fft(chan_b)
        corr = fft.ifft(fft_a.conjugate() * fft_b)
        corr = fft.fftshift(corr)
        peak_i = numpy.argmax(numpy.abs(corr))
        corr_angle = numpy.degrees(numpy.angle(corr[peak_i]))
        
        # Check results
        #phase_shift = chan_b * numpy.exp(1j * numpy.radians(-corr_angle))
        #phase_shift_fft = fft.fft(phase_shift)
        #check_corr = fft.ifft(fft_a.conjugate() * phase_shift_fft)
        #check_corr = fft.fftshift(check_corr)
        #check_peak_i = numpy.argmax(numpy.abs(check_corr))
        #check_angle = numpy.degrees(numpy.angle(check_corr[check_peak_i]))
        #assert(check_angle < 0.0001)
        
        print(freq_val, corr_angle)
        res_file.write('%s,%0.04f\n' % (freq, corr_angle))
        res_file.flush()
        
    
if __name__ == '__main__':
    main()