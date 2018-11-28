# serendip6
The serendip6 code in this repository is the CPU/GPU portion of a thresholding
spectrometer currently installed at the Arecibo, Green Bank, and FAST radio telescopes.

The remaining portion of the instrument code is typically an FPGA design.  The FPGA
is responsible for digitizing (and perhaps channelizing) raw voltages and transmitting
them to the CPU/GPU over a fast Ethernet.

Packetized voltages are received on a NIC and copied to the GPU where the
following processing takes place:
- performing an FFT
- power spectra formation
- power normalization
- thresholding

Frequency bins exceeding the threshold are recorded, along with observatory
meta-data, in FITS files.

This code is implemented as a plug-in to the Hashpipe data transport framework.
