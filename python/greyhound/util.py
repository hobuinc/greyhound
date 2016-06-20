import urllib2
import numpy as np
import json
import struct
import laspy

def writeLASfile(data, filename):

    minx = min(data['X'])
    miny = min(data['Y'])
    minz = min(data['Z'])

    header = laspy.header.Header()
    scale = 0.01
    header.x_scale = scale; header.y_scale = scale; header.z_scale = scale
    header.x_offset = minx ;header.y_offset = miny; header.z_offset = minz
    header.offset = [minx, miny, minz]

    X = (data['X'] - header.x_offset)/header.x_scale
    Y = (data['Y'] - header.y_offset)/header.y_scale
    Z = (data['Z'] - header.z_offset)/header.z_scale
    output = laspy.file.File(filename, mode='w', header = header)
    output.X = X
    output.set_scan_dir_flag(data['ScanDirectionFlag'])
    output.set_intensity(data['Intensity'])
    output.set_scan_angle_rank(data['ScanAngleRank'])
    output.set_pt_src_id(data['PointSourceId'])
    output.set_edge_flight_line(data['EdgeOfFlightLine'])
    output.set_return_num(data['ReturnNumber'])
    output.set_num_returns(data['NumberOfReturns'])
    output.Y = Y
    output.Z = Z
    output.Raw_Classification = data['Classification']
    output.close()
