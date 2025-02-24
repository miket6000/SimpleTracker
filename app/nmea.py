import datetime

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
    return time.strftime("%H:%M:%S")

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

def get_fix(nmea_sentence):
    content = get_content(nmea_sentence)
    fields = content.split(',')
    return int(fields[6])

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

