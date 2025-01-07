#! /bin/env python3

import os, sys
from serial import Serial
import datetime
#import geopands as gpd

#rom shapley.geometry import Point, Polygon

dt = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
raw1_filename = f"nmea_1_{dt}.txt"
raw2_filename = f"nmea_2_{dt}.txt"
log_filename = f"log_{dt}.csv"
live_filename = "live.csv"

def conv_dec(deg_min):
    if (len(deg_min) == 0):
        return 0
    brk = deg_min.find('.') - 2
    degrees = deg_min[0:brk]
    minutes = deg_min[brk:]
    return float(degrees) + float(minutes) / 60;

def get_content(nmea_sentence):
    delim_index = 1;
    if (nmea_sentence[0] != '$'):
        return ''
    for i in range(len(nmea_sentence)):
        if (nmea_sentence[i] == '*'):
            delim_index = i
    return nmea_sentence[1:delim_index]

def get_time(nmea_sentence):
    content = get_content(nmea_sentence)
    fields = content.split(',')
    time = datetime.time(int(fields[1][:-8]), int(fields[1][-8:-6]), int(fields[1][-6:-4]))
    return time
 
def get_latitude(nmea_sentence):
    content = get_content(nmea_sentence)
    fields = content.split(',')
    lat = conv_dec(fields[2])
    if (fields[3] == 'N'):
        return lat
    else:
        return -lat

def get_longitude(nmea_sentence):
    content = get_content(nmea_sentence)
    fields = content.split(',')
    long = conv_dec(fields[4])
    if (fields[5] == 'E'):
        return long
    else:
        return -long

def get_altitude(nmea_sentence):
    content = get_content(nmea_sentence)
    fields = content.split(',')
    return fields[9] 

def get_checksum(nmea_sentence):
    if (nmea_sentence[0] != '$'):
        return ''
    for i in range(len(nmea_sentence)):
        if (nmea_sentence[i] == '*'):
            delim_index = i
    return nmea_sentence[delim_index+1:delim_index+3]
 
def calc_checksum(nmea_sentence):
    cksum = 0
    content = get_content(nmea_sentence)
    for char in content:
        cksum ^= ord(char)
    return f"{cksum:02X}"

def nmea_checksum_ok(nmea_sentence):
    ccksum = calc_checksum(nmea_sentence)
    rcksum = get_checksum(nmea_sentence)
    return (ccksum == rcksum)

port1 = True
port2 = True

try:
    gps1 = Serial("/dev/ttyACM0", timeout=0.5)
    raw1 = open(raw1_filename, "w")
    print("Ground station found on /dev/ttyACM0")
except Exception:
    port1 = False

try:
    gps2 = Serial("/dev/ttyACM1", timeout=0.5)
    raw2 = open(raw2_filename, "w")
    print("Ground station found on /dev/ttyACM1")
except Exception:
    port2 = False

log = open(log_filename, "w")
live = open(live_filename, "w")

header = "utc_time,latitude,longitude,altitude,port1_rssi,port2_rssi"
log.write(header)
live.write(header)
print(header)
columns = {
    "utc_time"      :"", 
    "latitude"      :"", 
    "longitude"     :"", 
    "altitude"      :0.0, 
    "port1_rssi"    :0, 
    "port2_rssi"    :0
}

new_data = False

while (1):
    try:
        if (port1):
            raw_str = gps1.readline().decode()
            raw1.write(raw_str)
            raw1.flush()
            data = raw_str.split(" ")
            if (data[0] == "RSSI:"):
                columns["port1_rssi"] = int(data[1].strip())
            elif (data[0] == "->"):
                if (nmea_checksum_ok(data[2])):
                    new_data = True
                    columns["utc_time"] = get_time(data[2])
                    columns["latitude"] = f"{get_latitude(data[2]):.6f}"
                    columns["longitude"] = f"{get_longitude(data[2]):.6f}"
                    columns["altitude"] = get_altitude(data[2])
                    #print(f"p1 columns:{columns}")
    except Exception as e:
        #print (f"Error on line {sys.exc_info()[2].tb_lineno}: {e}")
        pass
    
    try:
        if (port2):
            raw_str = gps2.readline().decode()
            raw2.write(raw_str)
            raw2.flush()
            data = raw_str.split(" ")
            if (data[0] == "RSSI:"):
                columns["port2_rssi"] = int(data[1].strip())
            elif (data[0] == "->"):
                if (nmea_checksum_ok(data[2])):
                    new_data = True
                    columns["utc_time"] = get_time(data[2]).strftime("%H:%M:%S")
                    columns["latitude"] = f"{get_latitude(data[2]):.6f}"
                    columns["longitude"] = f"{get_longitude(data[2]):.6f}"
                    columns["altitude"] = get_altitude(data[2])
                    #print(f"p2 columns:{columns}")
    except Exception as e:
        #print (f"Error on line {sys.exc_info()[2].tb_lineno}: {e}")
        pass

    if (new_data):
        #print(columns)
        line = ",".join([f"{columns[h]}" for h in header.split(',')])
        log.write(f"{line}\n")
        live.write(f"{line}\n")
        live.flush()
        new_data = False
        print(line)
            

