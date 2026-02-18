"""
Modbus TCP to Modbus RTU Gateway
================================
This script acts as a Modbus TCP Server (listening on 0.0.0.0:5020)
and forwards incoming TCP requests to a Modbus RTU Serial interface
(USB-to-RS485 dongle).

Requirements:
    pip install pymodbus pyserial

Configuration:
    - TCP_HOST / TCP_PORT : TCP server bind address and port
    - SERIAL_PORT         : COM port (e.g. "COM3" on Windows, "/dev/ttyUSB0" on Linux)
    - SERIAL_BAUDRATE     : Baud rate for RTU serial (default 9600)
    - SERIAL_BYTESIZE     : Data bits (default 8)
    - SERIAL_PARITY       : Parity ("N"=None, "E"=Even, "O"=Odd)
    - SERIAL_STOPBITS     : Stop bits (default 1)

Author : LGS Engineering
Date   : 2026-02-18
"""

import logging
import sys
import signal
import time
import serial
import serial.tools.list_ports
import asyncio

from pymodbus.server import StartAsyncTcpServer
from pymodbus.datastore import (
    ModbusDeviceContext,
    ModbusServerContext,
    ModbusSequentialDataBlock,
)
from pymodbus.client import ModbusSerialClient
from pymodbus.exceptions import ModbusException

# =============================================================================
#  CONFIGURATION — Adjust these values to match your setup
# =============================================================================

# --- TCP Server Settings ---
TCP_HOST = "0.0.0.0"       # Listen on all interfaces
TCP_PORT = 502             # Modbus TCP port (use 502 if you have admin/root privileges)

# --- RTU Serial Settings ---
SERIAL_PORT     = "COM23"    # Windows: "COM3", "COM4", etc.  |  Linux: "/dev/ttyUSB0"
SERIAL_BAUDRATE = 9600      # Baud rate
SERIAL_BYTESIZE = 8        # Data bits
SERIAL_PARITY   = "N"      # "N" = None, "E" = Even, "O" = Odd
SERIAL_STOPBITS  = 1       # Stop bits
SERIAL_TIMEOUT   = 2       # Timeout in seconds for RTU responses
SERIAL_INTER_CHAR_TIMEOUT = 0.04  # Inter-character timeout for RTU

# =============================================================================
#  LOGGING SETUP
# =============================================================================

# Setup console handler (stdout only - avoid duplicate output to stderr)
console_handler = logging.StreamHandler(sys.stdout)
console_handler.setLevel(logging.DEBUG)

# Setup file handler
file_handler = logging.FileHandler("modbus_gateway.log", mode="a", encoding="utf-8")
file_handler.setLevel(logging.DEBUG)

# Format for all handlers
formatter = logging.Formatter(
    "%(asctime)s [%(levelname)-8s] %(name)-25s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)
console_handler.setFormatter(formatter)
file_handler.setFormatter(formatter)

# Configure root logger
root_logger = logging.getLogger()
root_logger.setLevel(logging.DEBUG)
root_logger.addHandler(console_handler)
root_logger.addHandler(file_handler)
root_logger.propagate = True

log = logging.getLogger("GW-Main")
log_tcp = logging.getLogger("GW-TCP")
log_rtu = logging.getLogger("GW-RTU")


# =============================================================================
#  HELPER — List available serial ports
# =============================================================================

def list_serial_ports():
    """Print all available serial ports on this machine."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        log.warning("No serial ports detected on this machine.")
    else:
        log.info("Available serial ports:")
        for p in ports:
            log.info("  %-10s  %s  [%s]", p.device, p.description, p.hwid)
    return ports


# =============================================================================
#  CUSTOM REQUEST HANDLER — Forwards TCP requests to RTU
# =============================================================================

class ModbusTcpToRtuGateway:
    """
    Gateway that receives Modbus TCP requests and forwards them
    to an RTU serial slave, then returns the response back to the
    TCP client. Uses synchronous Modbus client for proper RTU communication.
    """

    def __init__(self):
        self.rtu_client: ModbusSerialClient | None = None
        self._connecting = False
        self._last_forward_key = None  # Track duplicate forwards to RTU

    def _connect_rtu(self):
        """Establish (or re-establish) the RTU serial connection."""
        if self._connecting:
            return False
        
        self._connecting = True
        try:
            if self.rtu_client and self.rtu_client.is_socket_open():
                self.rtu_client.close()

            self.rtu_client = ModbusSerialClient(
                port=SERIAL_PORT,
                baudrate=SERIAL_BAUDRATE,
                bytesize=SERIAL_BYTESIZE,
                parity=SERIAL_PARITY,
                stopbits=SERIAL_STOPBITS,
                timeout=SERIAL_TIMEOUT,
            )

            result = self.rtu_client.connect()
            if result:
                log_rtu.info(
                    "Connected to RTU serial port %s @ %d baud",
                    SERIAL_PORT, SERIAL_BAUDRATE,
                )
                self._connecting = False
                return True
            else:
                log_rtu.error("Failed to open serial port %s", SERIAL_PORT)
                self._connecting = False
                return False

        except serial.SerialException as exc:
            log_rtu.error("Serial error while connecting: %s", exc)
            self._connecting = False
            return False
        except Exception as exc:
            log_rtu.error("Unexpected error while connecting RTU: %s", exc)
            self._connecting = False
            return False

    def _ensure_rtu_connection(self) -> bool:
        """Make sure the RTU client is connected; reconnect if needed."""
        if self.rtu_client and self.rtu_client.is_socket_open():
            return True
        log_rtu.warning("RTU connection lost — attempting reconnect …")
        return self._connect_rtu()

    def forward_request(self, function_code: int, address: int,
                        count_or_value, unit: int, **kwargs):
        """
        Generic dispatcher that forwards a Modbus TCP request to the RTU bus.
        Returns the RTU response or None on failure.
        """
        if not self._ensure_rtu_connection():
            log_rtu.error("Cannot forward — RTU not connected.")
            return None

        # Detect duplicate forwards in rapid succession
        forward_key = (function_code, address, count_or_value, unit)
        if forward_key == self._last_forward_key:
            log_rtu.warning("⚠ DUPLICATE FORWARD DETECTED: fc=%d  addr=%d  unit=%d",
                           function_code, address, unit)
        else:
            self._last_forward_key = forward_key

        try:
            response = None

            # --- READ functions ---
            if function_code == 1:  # Read Coils
                log_tcp.debug(">> FC01 Read Coils  addr=%d count=%d device_id=%d",
                              address, count_or_value, unit)
                response = self.rtu_client.read_coils(
                    address, count=count_or_value, device_id=unit)

            elif function_code == 2:  # Read Discrete Inputs
                log_tcp.debug(">> FC02 Read Discrete Inputs  addr=%d count=%d device_id=%d",
                              address, count_or_value, unit)
                response = self.rtu_client.read_discrete_inputs(
                    address, count=count_or_value, device_id=unit)

            elif function_code == 3:  # Read Holding Registers
                log_tcp.debug(">> FC03 Read Holding Regs  addr=%d count=%d device_id=%d",
                              address, count_or_value, unit)
                response = self.rtu_client.read_holding_registers(
                    address, count=count_or_value, device_id=unit)

            elif function_code == 4:  # Read Input Registers
                log_tcp.debug(">> FC04 Read Input Regs  addr=%d count=%d device_id=%d",
                              address, count_or_value, unit)
                response = self.rtu_client.read_input_registers(
                    address, count=count_or_value, device_id=unit)

            # --- WRITE functions ---
            elif function_code == 5:  # Write Single Coil
                log_tcp.debug(">> FC05 Write Single Coil  addr=%d value=%s device_id=%d",
                              address, count_or_value, unit)
                response = self.rtu_client.write_coil(
                    address, count_or_value, device_id=unit)

            elif function_code == 6:  # Write Single Register
                log_tcp.debug(">> FC06 Write Single Reg  addr=%d value=%d device_id=%d",
                              address, count_or_value, unit)
                response = self.rtu_client.write_register(
                    address, count_or_value, device_id=unit)

            elif function_code == 15:  # Write Multiple Coils
                values = kwargs.get("values", [])
                log_tcp.debug(">> FC15 Write Multiple Coils  addr=%d values=%s device_id=%d",
                              address, values, unit)
                response = self.rtu_client.write_coils(
                    address, values, device_id=unit)

            elif function_code == 16:  # Write Multiple Registers
                values = kwargs.get("values", [])
                log_tcp.debug(">> FC16 Write Multiple Regs  addr=%d values=%s device_id=%d",
                              address, values, unit)
                response = self.rtu_client.write_registers(
                    address, values, device_id=unit)

            else:
                log_tcp.warning("Unsupported function code: %d", function_code)
                return None

            # --- Check response ---
            if response and not response.isError():
                if hasattr(response, "bits"):
                    log_rtu.debug("<< RTU OK  fc=%d  bits=%s", function_code, response.bits)
                elif hasattr(response, "registers"):
                    log_rtu.debug("<< RTU OK  fc=%d  regs=%s", function_code, response.registers)
                return response
            else:
                log_rtu.warning("<< RTU Error response: %s", response)
                return response

        except ModbusException as exc:
            log_rtu.error("Modbus exception during forwarding: %s", exc)
            return None
        except serial.SerialException as exc:
            log_rtu.error("Serial exception during forwarding: %s", exc)
            self.rtu_client = None  # force reconnect next time
            return None
        except Exception as exc:
            log_rtu.error("Unexpected error during forwarding: %s", exc)
            return None

    def close(self):
        """Cleanly close the RTU serial connection."""
        if self.rtu_client:
            try:
                self.rtu_client.close()
                log_rtu.info("RTU serial port closed.")
            except Exception:
                pass


# =============================================================================
#  CUSTOM CALLBACK DATA STORE — intercepts every Modbus request
# =============================================================================

class GatewaySlaveContext(ModbusDeviceContext):
    """
    A custom device context that, instead of using a local datastore,
    forwards every read/write to the RTU bus via the gateway object.
    """

    def __init__(self, gateway: ModbusTcpToRtuGateway, unit_id: int = 1):
        # Initialise with dummy data blocks (they won't actually be used)
        super().__init__(
            ModbusSequentialDataBlock(0, [0] * 65536),  # di
            ModbusSequentialDataBlock(0, [0] * 65536),  # co
            ModbusSequentialDataBlock(0, [0] * 65536),  # hr
            ModbusSequentialDataBlock(0, [0] * 65536),  # ir
        )
        self.gateway = gateway
        self.unit_id = unit_id
        self._last_request_key = None  # Track duplicate requests

    def getValues(self, fc_as_hex, address, count=1):
        """Intercept read requests and forward to RTU."""
        fc_map = {1: 1, 2: 2, 3: 3, 4: 4}
        fc = fc_map.get(fc_as_hex, fc_as_hex)
        
        # Detect duplicate requests in rapid succession
        request_key = (fc, address, count)
        if request_key == self._last_request_key:
            log_tcp.warning("⚠ DUPLICATE REQUEST DETECTED: fc=%d  addr=%d  count=%d  unit=%d",
                           fc, address, count, self.unit_id)
        else:
            self._last_request_key = request_key
        
        log_tcp.info("← TCP READ  fc=%d  addr=%d  count=%d  unit=%d",
                      fc, address, count, self.unit_id)

        # Forward request to RTU (synchronous)
        try:
            response = self.gateway.forward_request(fc, address, count, self.unit_id)
            if response and not response.isError():
                # FC01/FC02 = Coils/Discrete Inputs (bit data)
                if fc in (1, 2):
                    if hasattr(response, "bits"):
                        data = response.bits[:count]
                        log_tcp.debug("← TCP RETN fc=%d  bits=%s", fc, data)
                        return data
                # FC03/FC04 = Holding/Input Registers (word data)
                elif fc in (3, 4):
                    if hasattr(response, "registers"):
                        data = response.registers[:count]
                        log_tcp.debug("← TCP RETN fc=%d  regs=%s", fc, data)
                        return data
            else:
                log_tcp.error("← TCP RETN Error response: %s", response)
        except Exception as exc:
            log_tcp.error("Error reading from RTU: %s", exc)

        # Return default values based on function code
        if fc in (1, 2):
            return [False] * count  # Bits default to False
        else:
            return [0] * count      # Registers default to 0

    def setValues(self, fc_as_hex, address, values):
        """Intercept write requests and forward to RTU."""
        fc_map = {5: 5, 6: 6, 15: 15, 16: 16}
        fc = fc_map.get(fc_as_hex, fc_as_hex)
        
        # Detect duplicate requests in rapid succession
        request_key = (fc, address, tuple(values) if isinstance(values, (list, tuple)) else values)
        if request_key == self._last_request_key:
            log_tcp.warning("⚠ DUPLICATE WRITE DETECTED: fc=%d  addr=%d  values=%s  unit=%d",
                           fc, address, values, self.unit_id)
        else:
            self._last_request_key = request_key
        
        log_tcp.info("← TCP WRITE fc=%d  addr=%d  values=%s  unit=%d",
                      fc, address, values, self.unit_id)

        # Forward request to RTU (synchronous)
        try:
            if fc in (5, 6):
                val = values[0] if isinstance(values, (list, tuple)) else values
                self.gateway.forward_request(fc, address, val, self.unit_id)
            else:
                self.gateway.forward_request(fc, address, len(values),
                                            self.unit_id, values=values)
        except Exception as exc:
            log_tcp.error("Error writing to RTU: %s", exc)

    def validate(self, fc_as_hex, address, count=1):
        """Always allow — validation happens on the real RTU slave."""
        return True


# =============================================================================
#  MAIN — Start the gateway
# =============================================================================

async def main():
    banner = r"""
    ╔══════════════════════════════════════════════════════════════╗
    ║          Modbus TCP  →  Modbus RTU  Gateway                 ║
    ║          pymodbus 3.x   |   LGS Engineering                ║
    ╚══════════════════════════════════════════════════════════════╝
    """
    print(banner)

    log.info("=" * 60)
    log.info("  Modbus TCP → RTU Gateway starting …")
    log.info("=" * 60)

    # Show available serial ports
    list_serial_ports()

    # Create the RTU gateway object
    gateway = ModbusTcpToRtuGateway()
    gateway._connect_rtu()  # Synchronous connection

    # Create device contexts for all unit IDs (1-247)
    # This allows the gateway to forward requests to the correct RTU device
    # based on the unit ID in the Modbus TCP request
    context_dict = {}
    for unit_id in range(1, 248):
        context_dict[unit_id] = GatewaySlaveContext(gateway, unit_id=unit_id)
    
    server_context = ModbusServerContext(devices=context_dict, single=False)

    # Graceful shutdown event
    shutdown_event = asyncio.Event()

    def signal_handler(signum, frame):
        log.info("Shutdown signal received (Ctrl+C) — cleaning up …")
        shutdown_event.set()

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # Start the Modbus TCP server
    log.info("TCP server listening on %s:%d", TCP_HOST, TCP_PORT)
    log.info("Forwarding to RTU serial %s @ %d baud", SERIAL_PORT, SERIAL_BAUDRATE)
    log.info("Press Ctrl+C to stop.\n")

    try:
        # Start server in background task
        server_task = asyncio.create_task(
            StartAsyncTcpServer(
                context=server_context,
                address=(TCP_HOST, TCP_PORT),
            )
        )
        
        # Wait for shutdown signal or server to finish
        shutdown_task = asyncio.create_task(shutdown_event.wait())
        
        done, pending = await asyncio.wait(
            [server_task, shutdown_task],
            return_when=asyncio.FIRST_COMPLETED
        )

        # Cancel all pending tasks
        for task in pending:
            task.cancel()
        
        log.info("Cleaning up and closing connections…")
        gateway.close()  # Synchronous close
        log.info("Gateway shutdown complete.")
        sys.exit(0)

    except OSError as exc:
        log.critical("Cannot bind TCP server: %s", exc)
        log.critical("Port %d may already be in use or requires elevated privileges.", TCP_PORT)
        gateway.close()
        sys.exit(1)
    except asyncio.CancelledError:
        log.info("Server task cancelled.")
        gateway.close()
        sys.exit(0)
    except Exception as exc:
        log.critical("Fatal error: %s", exc)
        gateway.close()
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())
