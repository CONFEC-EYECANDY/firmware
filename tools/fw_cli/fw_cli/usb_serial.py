"""Raw USB serial transport for esptool and monitoring; no framed protocol.

espbridge 1.2 supplies Android fd wrapping and descriptor discovery. Line setup
is done here without automatic reset pulses; esptool owns the reset sequence.
"""
import struct
import time

import serial


class USBSerial:
    SUPPORTED = {(0x303A, 0x1001), (0x303A, 0x0002), (0x10C4, 0xEA60),
                 (0x1A86, 0x7523), (0x1A86, 0x5523), (0x1A86, 0x55D4),
                 (0x0403, 0x6001)}

    def __init__(self, fd, port, baudrate=115200, timeout=0.2, monitor=False):
        import espbridge as backend
        import usb.core
        import usb.util
        self.backend, self.usb, self.util = backend, usb.core, usb.util
        self.fd, self.port, self.name = fd, port, port
        self.timeout, self.write_timeout = timeout, 10
        self._baudrate = baudrate
        self._dtr, self._rts = False, False
        self._buffer = bytearray()
        self._claimed = []
        self.is_open = False
        self.device = None
        self._monitor = monitor
        self.open()

    def open(self):
        if self.is_open:
            return
        try:
            self.device = self.backend.wrap_fd(self.fd)
            self.vid, self.pid = self.device.idVendor, self.device.idProduct
            self.identity = self.vid, self.pid
            if self.identity not in self.SUPPORTED:
                raise serial.SerialException(f"Unsupported USB serial device {self.vid:04x}:{self.pid:04x}")
            self.ep_in, self.ep_out, self.interface = self.backend.get_cdc_endpoints(self.device)
            self.native = self.vid == 0x303A
            self.cdc = self.native or self.identity == (0x1A86, 0x55D4)
            self.control = self.backend.find_cdc_control_interface(self.device, self.interface) if self.cdc else self.interface
            # Do not call set_configuration: Android already configured this fd.
            for number in sorted({self.interface, self.control}):
                self.util.claim_interface(self.device, number)
                self._claimed.append(number)
            cfg = self.device.get_active_configuration()
            ep = next(e for i in cfg for e in i if e.bEndpointAddress == self.ep_in)
            self.packet_size = ep.wMaxPacketSize
            self.is_open = True
            self._initialize()
            self.baudrate = self._baudrate
            self._dtr, self._rts = bool(self._monitor), False
            self._set_lines()
        except Exception as exc:
            self.close()
            if isinstance(exc, serial.SerialException):
                raise
            raise serial.SerialException(f"Cannot open Android USB: {exc}. Check libusb/OTG and USB permission.") from exc

    def _control(self, request_type, request, value=0, index=None, data=None):
        try:
            return self.device.ctrl_transfer(request_type, request, value,
                                             self.control if index is None else index,
                                             data, timeout=2000)
        except self.usb.USBError as exc:
            raise serial.SerialException(f"USB control transfer failed: {exc}") from exc

    def _initialize(self):
        if self.identity == (0x10C4, 0xEA60):
            self._control(0x41, 0x00, 1)        # UART enable
            self._control(0x41, 0x03, 0x0800)   # 8N1
            self._control(0x41, 0x13, data=struct.pack("<IIII", 1, 0x40, 0, 0))  # no flow control
        elif self.identity in {(0x1A86, 0x7523), (0x1A86, 0x5523)}:
            self._control(0x40, 0xA1, index=0)
            self._control(0x40, 0x9A, 0x2518, 0xC3)  # enable RX/TX, 8N1
        elif self.identity == (0x0403, 0x6001):
            self._control(0x40, 0x00, 0, 0)     # serial engine reset, not DTR/RTS
            self._control(0x40, 0x02, 0, 0)     # no flow control
            self._control(0x40, 0x04, 8, 0)     # 8N1

    @property
    def baudrate(self):
        return self._baudrate

    @baudrate.setter
    def baudrate(self, baud):
        if baud <= 0:
            raise ValueError("Baud rate must be positive")
        if self.identity == (0x10C4, 0xEA60):
            self._control(0x41, 0x1E, data=struct.pack("<I", baud))
        elif self.cdc:
            self._control(0x21, 0x20, data=struct.pack("<IBBB", baud, 0, 0, 8))
        elif self.identity in {(0x1A86, 0x7523), (0x1A86, 0x5523)}:
            if not 1200 <= baud <= 2000000:
                raise ValueError("CH340 baud rate must be between 1200 and 2000000")
            self.backend.set_uart_bridge_baud(self.device, baud)
        else:
            if baud != 115200:
                raise ValueError("The Android FT232 transport currently supports only 115200 baud")
            self._control(0x40, 0x03, 0x001A, 0)
        self._baudrate = baud

    def _set_lines(self):
        if self.cdc:
            self._control(0x21, 0x22, int(self._dtr) | (int(self._rts) << 1))
        else:
            try:
                self.backend.set_dtr_rts(self.device, self._dtr, self._rts)
            except self.usb.USBError as exc:
                raise serial.SerialException(f"USB reset-line control failed: {exc}") from exc

    @property
    def dtr(self):
        return self._dtr

    @dtr.setter
    def dtr(self, value):
        self._dtr = bool(value)
        self._set_lines()

    @property
    def rts(self):
        return self._rts

    @rts.setter
    def rts(self, value):
        self._rts = bool(value)
        self._set_lines()

    def setDTR(self, value):
        self.dtr = value

    def setRTS(self, value):
        self.rts = value

    def _receive(self, timeout_ms):
        try:
            raw = bytes(self.device.read(self.ep_in, self.packet_size * 16, timeout=max(1, timeout_ms)))
        except self.usb.USBTimeoutError:
            return
        except self.usb.USBError as exc:
            raise serial.SerialException(f"USB disconnected/read failed: {exc}") from exc
        if self.identity == (0x0403, 0x6001):
            # FTDI prepends two modem/status bytes to EVERY USB packet.
            raw = b"".join(raw[i + 2:i + self.packet_size] for i in range(0, len(raw), self.packet_size))
        self._buffer.extend(raw)

    @property
    def in_waiting(self):
        self._receive(1)
        return len(self._buffer)

    def inWaiting(self):
        return self.in_waiting

    def read(self, size=1):
        if not self.is_open:
            raise serial.SerialException("USB port is closed")
        deadline = None if self.timeout is None else time.monotonic() + self.timeout
        while len(self._buffer) < size:
            remaining = 0.2 if deadline is None else max(0, deadline - time.monotonic())
            self._receive(max(1, int(min(remaining, 0.2) * 1000)))
            if deadline is not None and time.monotonic() >= deadline:
                break
        result = bytes(self._buffer[:size])
        del self._buffer[:size]
        return result

    def write(self, data):
        if not self.is_open:
            raise serial.SerialException("USB port is closed")
        data = bytes(data)
        offset = 0
        deadline = None if self.write_timeout is None else time.monotonic() + self.write_timeout
        while offset < len(data):
            remaining = 10 if deadline is None else deadline - time.monotonic()
            if remaining <= 0:
                raise serial.SerialTimeoutException("USB write timed out")
            try:
                count = self.device.write(self.ep_out, data[offset:], timeout=max(1, int(remaining * 1000)))
            except self.usb.USBTimeoutError as exc:
                raise serial.SerialTimeoutException("USB write timed out") from exc
            except self.usb.USBError as exc:
                raise serial.SerialException(f"USB write failed: {exc}") from exc
            if count <= 0:
                raise serial.SerialException("USB write made no progress")
            offset += count
        return offset

    def reset_input_buffer(self):
        self._buffer.clear()
        deadline = time.monotonic() + 0.2
        while time.monotonic() < deadline:
            self._receive(1)
            if not self._buffer:
                break
            self._buffer.clear()

    def reset_output_buffer(self):
        pass  # USB writes are synchronous; no queued output

    flushInput = reset_input_buffer
    flushOutput = reset_output_buffer
    flush = reset_output_buffer

    def isOpen(self):
        return self.is_open

    def close(self):
        if self.device is not None:
            for number in reversed(self._claimed):
                try:
                    self.util.release_interface(self.device, number)
                except self.usb.USBError:
                    pass
            self._claimed.clear()
            self.util.dispose_resources(self.device)
            self.device = None
        self.is_open = False
        self._buffer.clear()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
