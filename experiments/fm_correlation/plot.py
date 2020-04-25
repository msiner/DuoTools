
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
from matplotlib import pyplot


def main():
    results = []
    with open('results.csv', 'r') as csv_file:
        reader = csv.reader(csv_file)
        results = [[float(y) for y in x] for x in reader]
    results = numpy.array(results)
    pyplot.scatter(results[:,0], results[:,1])
    pyplot.show()
    
if __name__ == '__main__':
    main()