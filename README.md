# DuoTools
DuoTools is a collection of dual-channel utilities for the SDRplay RSPDuo dual-tuner software-defined radio.
The utilities are designed as easy and generic ways to interface with the dual-tuner functionality provided by the RSPDuo.
Specifically, they provide interleaved and framed sample data from both tuners simultaneously.
At this time, the utilities don't provide control of each tuner.
That is, both tuners are tuned to the same frequency and gain and AGC settings for each analog frontend are also matched.

## DuoEngine
DuoEngine is a wrapper around the SDRPlay API specifically designed for the use case described above.
It simplifies and handles configuration.
It also handles buffering and framing sample data.

### Framing
The SDRPlay API uses separate callbacks for each tuner.
Each callback also has separate buffers for I (in-phase) and Q (quadrature) samples.
DuoEngine, and the utilities that use it, reformat this data to facilitate time-synchronous analysis.
The format of the data, for tuners ```1``` and ```2```, is as follows:
```
I1, Q1, I2, Q2
```

Described as a C struct, the default frame might be specified as follows.
```
struct Frame {
  int16_t i1;
  int16_t q1;
  int16_t i2;
  int16_t q2;
}
```

DuoEngine, and the utilities that use it, also provide the capability to convert the sample scalars from the original 16-bit signed integer format to 32-bit floating point for easier downstream processing at the cost of network bandwidth or storage space.
Note that this representation may not be portable between different processor architectures.
In that case, the C struct for the frame might be specified as follows.
```
struct Frame {
  float i1;
  float q1;
  float i2;
  float q2;
}
```

## DuoWAV
DuoWAV is a utility to capture samples from the RSPDuo directly to a file.
The supported file format is [WAV](https://en.wikipedia.org/wiki/WAV).
The WAV format supports multi-channel framing, as described above, as well as 16-bit linear PCM and IEEE floating point sample scalar formats.
While WAV input is supported by many applications some may not support more than 2 channels (i.e. stereo) or the IEEE floating point sample format.
Notably, the GNURadio [Wav File Source](https://wiki.gnuradio.org/index.php/Wav_File_Source) only supports linear PCM format WAV files, but it does automatically convert the samples to floating point.

Below is the usage description for the DuoWAV utility.

```
Usage: DuoWAV.exe [-h] [-m max] [-a agchz] [-t agcdb] [-l lna] [-d decim]
                  [-n notch] [-w warmup] [-o] [-f] [-k] [-x] freq bytes [path]

Options:
  -h: print this help message
  -m max: maximum transfer size in bytes (default=10240)
  -a 0|5|50|100: AGC frequency in Hz (default=0)
  -t [-72-0]: AGC set point in dBFS (default=-30)
  -l 0-9: LNA state where 0 provides the least RF gain reduction.
      Default value is 4 (20-37 dB reduction depending on frequency).
  -d 1|2|4|8|16|32: Decimation factor (default=1)
      For factors 4, 8, 16, and 32, the analog bandwidth will
      be reduced to 600, 300, 200, and 200 kHz respectively unless
      the -x option is also specified. In which case the analog
      bandwidth remains 1.536 MHz.
  -n mwfm|dab: Enable MW/FM or DAB notch filter
      Both filters can be enabled by providing the -n option twice
      (once for each filter). By default, both filters are disabled.
  -w seconds: Run the radio for the specified number of seconds to
      warm up and stabilize performance before capture (default=0).
      During the warmup period, samples are discarded.
  -f: Convert samples to floating point
  -o: Omit the WAV header. Samples will start at beginning of file.
  -k: Use USB bulk transfer mode instead of isochronous
  -x: Use the maximum 8 MHz master sample rate.
      This will deliver 12 bit ADC resolution, but with slightly
      better anti-aliaising performance at the widest bandwidth.
      This mode is only available at 1.536 MHz analog bandwidth.
      The default mode is to use a 6 MHz master sample clock.
      That mode delivers 14 bit ADC resolution, but with slightly
      inferior anti-aliaising performance at the widest bandwidth.
      The default mode is also compatible with analog bandwidths of
      1.536 MHz, 600 kHz, 300 kHz, and 200 kHz. 6 MHz operation
      should result in a slightly lower CPU load.

Arguments:
  freq: Tuner RF frequency in Hz is a mandatory argument.
      Can be specified with k, K, m, M, g, or G suffix to indicate
      the value is in kHz, MHz, or GHz respectively (e.g. 1.42G)
  bytes: Maximum output file size in bytes.
      Can be specified with k, K, m, M, g, or G suffix to indicate
      the value is in KiB, MiB, or GiB respectively (e.g. 10M)
      NOTE: WAV files cannot exceed 4 GiB.
  [path]: The destination file path (default=duo.wav)
```

## DuoUDP

```
Usage: DuoUDP.exe [-h] [-m mtu] [-a agchz] [-t agcdb] [-l lna] [-d decim]
                  [-n notch] [-k] [-x] freq [[ipaddr][:port]]

Options:
  -h: print this help message
  -m mtu: packet MTU (default=1500)
  -a 0|5|50|100: AGC frequency in Hz (default=0)
  -t [-72-0]: AGC set point in dBFS (default=-30)
  -l 0-9: LNA state where 0 provides the least RF gain reduction.
      Default value is 4 (20-37 dB reduction depending on frequency).
  -d 1|2|4|8|16|32: Decimation factor (default=1)
      For factors 4, 8, 16, and 32, the analog bandwidth will
      be reduced to 600, 300, 200, and 200 kHz respectively unless
      the -x option is also specified. In which case the analog
      bandwidth remains 1.536 MHz.
  -n mwfm|dab: Enable MW/FM or DAB notch filter
      Both filters can be enabled by providing the -n option twice
      (once for each filter). By default, both filters are disabled.
  -f: Convert samples to floating-point
  -k: Use USB bulk transfer mode instead of isochronous
  -x: Use the maximum 8 MHz master sample rate.
      This will deliver 12 bit ADC resolution, but with slightly
      better anti-aliaising performance at the widest bandwidth.
      This mode is only available at 1.536 MHz analog bandwidth.
      The default mode is to use a 6 MHz master sample clock.
      That mode delivers 14 bit ADC resolution, but with slightly
      inferior anti-aliaising performance at the widest bandwidth.
      The default mode is also compatible with analog bandwidths of
      1.536 MHz, 600 kHz, 300 kHz, and 200 kHz. 6 MHz operation
      should result in a slightly lower CPU load.

Arguments:
  freq: Tuner RF frequency in Hz is a mandatory argument.
      Can be specified with k, K, m, M, g, or G suffix to indicate
      the value is in kHz, MHz, or GHz respectively (e.g. 1.42G)
  [ipaddr][:port]: The destination IPv4 address and UDP port can optionally
      be specified (default=127.0.0.1:1234). One or both can be specified and
      the default of the unspecified value will be used.
```
