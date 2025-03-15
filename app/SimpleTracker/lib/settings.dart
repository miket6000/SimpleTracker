import 'dart:typed_data';

const double tickDuration = 0.01;

class Setting {
  final String title;
  bool configurable;
  Map? optionList;
  int value;
  final String hint;
  Setting({required this.title, required this.value, this.optionList, this.configurable = true, this.hint = ""});
}
class Unit {
  final String title;
  final double slope;
  final double offset;
  const Unit({required this.title, required this.slope, required this.offset});
}

final Map<String, Setting> settings = {
  "f": Setting(title:"Frequency", value: 434, hint:"The center frequency in MHz, this must match on both the transmitter and receiver."),
  "s": Setting(title:"Spread Factor", value: 9, optionList: spreadingFactorOptions, hint:"Controls the length of the chirp. Larger values take longer to transmit, but are will increase range. Must match on transmitter and receiver."),
  "b": Setting(title:"Bandwidth", value: 6, optionList: bandwidthOptions, hint:"A lower bandwith will take longer to transmit but will increase range. Low bandwidths might not be legal, use with caution. Must match on transmitter and receiver."),
  "c": Setting(title:"CRC Rate", value: 1, optionList: codingRateOptions, hint:"Controls the number of error detection bits in the CRC. Must match on tramsmitter and receiver."),
  "d": Setting(title:"Transmit Power", value: 0xFF, optionList: powerOptions, hint:"Controls the transmit power in dBm. Higher values will go further, but use more power and might cause brownout in the power supply."), 
  "o": Setting(title:"Overcurrent Limit", value: 150, configurable: false, hint:"Sets a hard current limit on the power amplifier current drawer to prevent brownout."),
  "p": Setting(title:"Preamble Length", value: 8, hint:"Longer preamble is important for receivers that are only powered up occasionally. Must match on the transmitter and receiver."),
};

const Map<String, int> spreadingFactorOptions = {
  "7"   : 7, 
  "8"   : 8, 
  "9"   : 9, 
  "10"  : 10, 
  "11"  : 11, 
  "12"  : 12
};

const Map<String, int> bandwidthOptions = {
  "7.8kHz"    : 0, 
  "10.4kHz"   : 1, 
  "15.6kHz"   : 2, 
  "20.8kHz"   : 3, 
  "31.25kHz"  : 4,
  "41.7kHz"   : 5,
  "62.5kHz"   : 6,
  "125kHz"    : 7,
  "250kHz"    : 8,
  "500kHz"    : 9,
};

const Map<String, int> codingRateOptions = {
  "4_5" : 1,
  "4_6" : 2,
  "4_7" : 3,
  "4_8" : 4,
};

const Map<String, int> powerOptions = {
  "11dB"  : 0xF6,
  "14dB"  : 0xF9,
  "17dB"  : 0xFC,
  "20dB"  : 0xFF,
};

const Map<String, int> modeOptions = {
  "Receiver"    : 1,
  "Transmitter" : 2,
};

const Map<String, Unit> units = {
  "Hz":   Unit(title:"Frequency",     slope:0.000001,     offset:0),
  "kHz":  Unit(title:"Frequency",     slope:0.001,        offset:0),
  "MHz":  Unit(title:"Frequency",     slope:1,            offset:0),
  "SF":   Unit(title:"Spreading Factor", slope:1,         offset:0),
  "dBm":  Unit(title:"Power",         slope:1,            offset:0),
  "mA":   Unit(title:"Current",       slope:1,            offset:0),
  "A":    Unit(title:"Current",       slope:0.001,        offset:0),
  "":     Unit(title:"Count",         slope:1,            offset:0),
  "V":    Unit(title:"Voltage",       slope:1000,         offset:0),
  "m":    Unit(title:"Altitude",      slope:100,          offset:0),
  "ft":   Unit(title:"Altitude",      slope:30.48,        offset:0),
  "hPa":  Unit(title:"Pressure",      slope:100,          offset:0),
  "psi":  Unit(title:"Pressure",      slope:6894.75729,   offset:0),
  "°C":   Unit(title:"Temperature",   slope:100,          offset:0),
  "°F":   Unit(title:"Temperature",   slope:180,          offset:32),
  "-":    Unit(title:"Status",        slope:1,            offset:0),
};

int swapBytes(Uint8List bytes) {
  var value = 0;
  if (bytes.length == 1) {
    value = bytes[0].toInt();
  } else if (bytes.length == 2) {
    value = (bytes[1].toInt() << 8) + bytes[0].toInt();
  } else  if (bytes.length == 4) {
    value = (bytes[3].toInt() << 24) + 
            (bytes[2].toInt() << 16) + 
            (bytes[1].toInt() << 8) + 
            bytes[0].toInt();
  } else {
    throw "Invalid number of bytes"; 
  }

  return value;
}