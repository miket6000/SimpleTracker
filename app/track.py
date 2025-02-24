#! /bin/env python3

import os, sys
from nmea import *
from serial import Serial
import datetime
import time
import serial.tools.list_ports
from PyQt5 import QtCore, QtSerialPort
from PyQt5.QtWidgets import QApplication, QWidget, QLabel, QMainWindow, QPushButton, QVBoxLayout, QGridLayout, QTextEdit

verbose = True

dt = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
log_filename = f"log_{dt}.csv"
live_filename = "live.csv"


class ground_station:
    def __init__(self, port_name, text_edit):
        self.ser = QtSerialPort.QSerialPort(
            port_name,
            baudRate=QtSerialPort.QSerialPort.Baud9600,
            readyRead=self.readline
        )        
        #self.ser = Serial(port_name, timeout = 0.5)
        #self.ser.write(b"UID\n")
        #time.sleep(1)
        self.uid = "auniquestr" #f"{int(self.ser.readline().decode()):08x}"
        print(f"Opened Ground Station {self.uid}\n")
        self.raw_filename = f"{self.uid}_{dt}.raw"
        self.log = open(self.raw_filename, "w")

    @QtCore.pyqtSlot() 
    def readline(self):
        while self.ser.canReadLine:
            line = self.ser.readline().decode()
            self.log.write(line)
            self.log.flush()
            text_edit.append(line)

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("SimpleTrackerGUI")
        self.lbl_latitude = QLabel("Latitude: ")
        self.lbl_longitude = QLabel("Longitude: ")
        self.lbl_altitude = QLabel("Altitude: ")
        self.btn_reset_ground = QPushButton("Reset Ground Level")
        self.te_rx_msg = QTextEdit()
#        self.btn = QPushButton("Press Me!")

        lo_main = QVBoxLayout()
        lo_grid = QGridLayout()
        lo_grid.addWidget(self.lbl_latitude, 0, 0)
        lo_grid.addWidget(self.lbl_longitude, 1, 0)
        lo_grid.addWidget(self.lbl_altitude, 2, 0)
        lo_grid.addWidget(self.btn_reset_ground, 2, 2)

        lo_main.addLayout(lo_grid)
        lo_main.addWidget(self.te_rx_msg)

        container = QWidget()
        container.setLayout(lo_main)

        self.setCentralWidget(container)

        #self.gs_list = []
        
        ports = serial.tools.list_ports.comports()
        for p in ports:
            if p.product == "SimpleTracker":
                #self.gs_list.append(ground_station(p.device, self.te_rx_msg))
                self.serial = QtSerialPort.QSerialPort(
                    p.device,
                    baudRate=QtSerialPort.QSerialPort.Baud9600,
                    readyRead=self.receive
                )
                print(f"Connected to {p.device}")

        
        self.log = open(log_filename, "w")
        self.live = open(live_filename, "w")
        
        self.header = "utc_time,uid,latitude,longitude,altitude,rssi"
        self.log.write(f"{self.header}\n")
        self.live.write(f"{self.header}\n")
        print(self.header)
        self.columns = {
            "utc_time"      :"", 
            "uid"           :"",
            "latitude"      :"", 
            "longitude"     :"", 
            "altitude"      :0.0, 
            "rssi"          :0, 
        }
        self.serial.open(QtCore.QIODevice.ReadWrite) 
        self.new_data = False
    
    @QtCore.pyqtSlot()
    def receive(self):
        while self.serial.canReadLine:
            line = self.serial.readLine().decode().strip('\r\n')
            print(line)
            #self.log.write(line)
            #self.log.flush()
            self.te_rx_msg.append(line)


app = QApplication(sys.argv)
window = MainWindow()
window.show()
app.exec()

while (1):

    for gs in gs_list:
        try:
            raw = gs.readline()
            if raw != "":
                data = raw.split(" ")
                if (data[0] == "RSSI:"):
                    columns["rssi"] = int(data[1].strip())
                elif (data[0] == "->") and \
                    nmea_checksum_ok(data[2]) and \
                    (get_fix(data[2]) == 1) and \
                    (get_time(data[2]) != columns["utc_time"]):
                        new_data = True
                        columns["utc_time"] = get_time(data[2])                    
                        columns["uid"] = data[1].strip()
                        columns["latitude"] = f"{get_latitude(data[2]):.6f}"
                        columns["longitude"] = f"{get_longitude(data[2]):.6f}"
                        columns["altitude"] = get_altitude(data[2])
        except Exception as e:
            print (f"Error on line {sys.exc_info()[2].tb_lineno}: {e}")
            print (raw)
            pass

    if (new_data):
        new_data = False
        #print(columns)
        line = ",".join([f"{columns[h]}" for h in header.split(',')])
        log.write(f"{line}\n")
        live.write(f"{line}\n")
        live.flush()
        print(line)
            

