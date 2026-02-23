"""
LGS Smart Gateway (High Efficiency Version)
===========================================
Architecture: AsyncIO TCP Server -> Priority Queue -> Single Serial Worker
Features:
  - Non-blocking TCP handling
  - Serialized RTU access (No Race Conditions)
  - Intelligent Write Deduplication (Coil vs Register awareness)
  - Auto-reconnect & Sparse Memory usage

Author: Gemini (Refactored for LGS Project)
"""

import asyncio
import logging
import time
import os
from concurrent.futures import ThreadPoolExecutor
from typing import Tuple, Optional, Any, Union

from pymodbus.server import StartAsyncTcpServer
from pymodbus.datastore import (
    ModbusDeviceContext,
    ModbusServerContext,
    ModbusSparseDataBlock,
)
from pymodbus.client import ModbusSerialClient
from pymodbus.exceptions import ModbusException, ModbusIOException
from pymodbus.constants import ExcCodes

# ================= Configuration =================
TCP_HOST = "0.0.0.0"
TCP_PORT = 502  # Use 502 if running as root/admin

SERIAL_CONFIG = {
    "port": "/dev/ttyUSB0",  # Change to your port (e.g., /dev/ttyUSB0)
    "baudrate": 9600,
    "bytesize": 8,
    "parity": "N",
    "stopbits": 1,
    "timeout": 0.5,  # RTU Response timeout
}

# Caching Strategy
CACHE_TTL = 0.2  # Seconds (prevent spamming identical commands)
# Default timeout for waiting RTU responses from serial_manager
SERVER_TIMEOUT = 2.0  # seconds

# ================= Logger Setup =================
logging.basicConfig(
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    level=logging.INFO,
    datefmt="%H:%M:%S"
)
log = logging.getLogger("LGS-Gateway")
log.setLevel(logging.DEBUG)

# Ensure pymodbus logs at INFO so connection events are visible
logging.getLogger("pymodbus").setLevel(logging.INFO)

# Component loggers for clearer event types
LOG_ROOT = "LGS-Gateway"
serial_log = logging.getLogger(f"{LOG_ROOT}.serial")
worker_log = logging.getLogger(f"{LOG_ROOT}.worker")
context_log = logging.getLogger(f"{LOG_ROOT}.context")
dedupe_log = logging.getLogger(f"{LOG_ROOT}.dedupe")
tcp_log = logging.getLogger(f"{LOG_ROOT}.tcp")


# ================= Core Logic: The Serial Worker =================

class RtuRequest:
    """Structure to hold a request in the Queue"""
    def __init__(self, unit_id, func_code, address, value=None, count=1):
        self.unit_id = unit_id
        self.func_code = func_code
        self.address = address
        self.value = value
        self.count = count
        # Future will be created on the SerialManager loop when submitting
        self.future: Optional[asyncio.Future] = None
        # Timestamps for lifecycle tracing (epoch seconds)
        self.queue_ts: Optional[float] = None
        self.dequeued_ts: Optional[float] = None
        self.forward_ts: Optional[float] = None
        self.resp_ts: Optional[float] = None
        self.complete_ts: Optional[float] = None

class SerialManager:
    """
    Manages the Serial Connection and processes requests strictly one by one.
    This prevents race conditions and RS485 collisions.
    """
    def __init__(self):
        # The async queue must be created on the event loop that will run
        # the serial worker. Create it later inside `serial_runner` after
        # the serial loop is set. For now keep a placeholder.
        self.queue = None
        self.client = ModbusSerialClient(**SERIAL_CONFIG)
        # Dedicated thread pool for blocking serial I/O
        self.executor = ThreadPoolExecutor(max_workers=8)
        # connection tracking
        self._connected = False
        self.connect_retries = 3
        self.reconnect_delay = 0.5
        self._write_history = {} # Key: (unit, addr, type) -> (value, timestamp)
        self._read_cache = {}    # Key: (unit, addr, fc, count) -> (value, timestamp)
        # Internal cleanup thresholds
        self._history_ttl = max(10 * CACHE_TTL, 1.0)

    def _clean_write_history(self):
        """Remove stale entries from write history."""
        now = time.time()
        to_del = [k for k, (_, ts) in self._write_history.items() if (now - ts) > self._history_ttl]
        for k in to_del:
            try:
                del self._write_history[k]
            except KeyError:
                pass

    def start(self):
        """Start the background worker task"""
        # Use the current event loop (may not be running yet in this thread)
        loop = asyncio.get_event_loop()
        serial_log.debug("SerialManager.start() scheduling worker on loop %s", loop)
        loop.create_task(self._worker_loop())

    async def submit_request(self, req: RtuRequest):
        """Interface for TCP Server to submit a job"""
        
        # 1. Smart Write Deduplication (Check before queuing)
        # Type 'c' for Coils, 'r' for Registers
        req_type = 'c' if req.func_code in (5, 15) else 'r'
        
        # Simple cleanup for write history to prevent unbounded growth
        self._clean_write_history()

        if req.func_code in (5, 6, 15, 16): # Write commands
            cache_key = (req.unit_id, req.address, req_type)
            last = self._write_history.get(cache_key)
            
            # Extract single value for comparison if single write
            val_to_check = req.value
            if req.func_code in (5, 6) and isinstance(req.value, (list, tuple)):
                val_to_check = req.value[0]

            # Normalize coil values for comparison (so dedupe compares 0/1)
            if req_type == 'c':
                if isinstance(val_to_check, (list, tuple)):
                    val_to_check = [1 if v else 0 for v in val_to_check]
                else:
                    val_to_check = 1 if val_to_check else 0
            
            # If value matches last write and is within TTL, skip hardware send
            if last:
                last_val, last_ts = last
                if last_val == val_to_check and (time.time() - last_ts) < CACHE_TTL:
                    dedupe_log.debug("SKIP Redundant Write: %s = %s (ttl=%.3f)", cache_key, val_to_check, CACHE_TTL)
                    # Fake success response
                    # Normalize coil values to integers (0/1) for returned packet
                    if req_type == 'c':
                        if isinstance(req.value, (list, tuple)):
                            return [1 if v else 0 for v in req.value]
                        return [1 if req.value else 0]
                    else:
                        return req.value if isinstance(req.value, list) else [req.value]

        # record queue timestamp and log
        req.queue_ts = time.time()
        serial_log.debug("Queueing request: unit=%s fc=%s addr=%s count=%s ts=%.6f", req.unit_id, req.func_code, req.address, req.count, req.queue_ts)

        # 2. Add to Queue
        # Create a Future bound to this running loop (serial manager loop)
        loop = asyncio.get_running_loop()
        req.future = loop.create_future()
        if self.queue is None:
            raise RuntimeError("SerialManager queue not initialized on serial loop")
        await self.queue.put(req)

        # 3. Wait for result (Non-blocking wait)
        serial_log.debug("Waiting for RTU result for request: unit=%s fc=%s addr=%s queued_ts=%.6f", req.unit_id, req.func_code, req.address, req.queue_ts)
        try:
            result = await req.future
        except Exception as exc:
            # Propagate serial errors to caller
            raise
        # result may have timestamps filled by worker
        now = time.time()
        resp_ts = getattr(req, 'resp_ts', now)
        total_ms = (resp_ts - req.queue_ts) * 1000 if req.queue_ts else 0
        serial_log.debug("Received RTU result for unit=%s addr=%s -> %s (resp_ts=%.6f total_ms=%.1f)", req.unit_id, req.address, repr(result), resp_ts, total_ms)
        
        # 4. Update History on success
        if req.func_code in (5, 6, 15, 16) and not isinstance(result, Exception):
            cache_key = (req.unit_id, req.address, req_type)
            val_to_store = req.value
            # For single writes, extract scalar
            if req.func_code in (5, 6) and isinstance(req.value, (list, tuple)):
                val_to_store = req.value[0]
            # Normalize coil storage to int 0/1
            if req_type == 'c':
                if isinstance(val_to_store, (list, tuple)):
                    val_normalized = [1 if v else 0 for v in val_to_store]
                else:
                    val_normalized = 1 if val_to_store else 0
                self._write_history[cache_key] = (val_normalized, time.time())
            else:
                self._write_history[cache_key] = (val_to_store, time.time())

        return result

    async def _worker_loop(self):
        """Background loop that processes the queue"""
        serial_log.info("Serial Worker Started on %s", SERIAL_CONFIG['port'])
        while True:
            loop = asyncio.get_running_loop()
            # Ensure serial connection (connect only when needed)
            if not self._connected:
                connected = False
                for attempt in range(self.connect_retries):
                    try:
                        connected = await loop.run_in_executor(self.executor, self.client.connect)
                    except Exception as e:
                        serial_log.debug("connect attempt %d failed: %s", attempt+1, e)
                        connected = False
                    if connected:
                        self._connected = True
                        serial_log.info("Serial connected on %s", SERIAL_CONFIG['port'])
                        break
                    # short backoff between quick retries
                    await asyncio.sleep(0.1)
                if not connected:
                    serial_log.warning("Serial not connected after %d attempts. Sleeping %.2fs", self.connect_retries, self.reconnect_delay)
                    await asyncio.sleep(self.reconnect_delay)
                    continue

            # Get next job
            serial_log.debug("Waiting for next request in queue...")
            req: RtuRequest = await self.queue.get()
            req.dequeued_ts = time.time()
            queue_wait_ms = ((req.dequeued_ts - req.queue_ts) * 1000) if req.queue_ts else None
            worker_log.debug("Dequeued request: unit=%s fc=%s addr=%s queued_ms=%s ts=%.6f", req.unit_id, req.func_code, req.address, f"{queue_wait_ms:.1f}" if queue_wait_ms is not None else "-", req.dequeued_ts)

            try:
                addr = req.address
                req.forward_ts = time.time()
                worker_log.debug("Forwarding to RTU: unit=%s fc=%s addr=%s forward_ts=%.6f", req.unit_id, req.func_code, addr, req.forward_ts)
                data = None

                if req.func_code == 1:
                    resp = await loop.run_in_executor(self.executor, lambda: self.client.read_coils(addr, count=req.count, device_id=req.unit_id))
                    data = getattr(resp, 'bits', None)
                    if data is not None:
                        data = data[:req.count]
                elif req.func_code == 2:
                    resp = await loop.run_in_executor(self.executor, lambda: self.client.read_discrete_inputs(addr, count=req.count, device_id=req.unit_id))
                    data = getattr(resp, 'bits', None)
                    if data is not None:
                        data = data[:req.count]
                elif req.func_code == 3:
                    resp = await loop.run_in_executor(self.executor, lambda: self.client.read_holding_registers(addr, count=req.count, device_id=req.unit_id))
                    data = getattr(resp, 'registers', None)
                    if data is not None:
                        data = data[:req.count]
                elif req.func_code == 4:
                    resp = await loop.run_in_executor(self.executor, lambda: self.client.read_input_registers(addr, count=req.count, device_id=req.unit_id))
                    data = getattr(resp, 'registers', None)
                    if data is not None:
                        data = data[:req.count]
                elif req.func_code == 5:
                    # write single coil
                    try:
                        await loop.run_in_executor(self.executor, lambda: self.client.write_coil(addr, req.value, device_id=req.unit_id))
                        # Normalize coil response to 0/1
                        if isinstance(req.value, (list, tuple)):
                            data = [1 if v else 0 for v in req.value]
                        else:
                            data = [1 if req.value else 0]
                    except Exception as e:
                        # Broadcast (unit 0) devices do not reply — treat as success
                        if isinstance(e, ModbusIOException) and req.unit_id == 0:
                            if isinstance(req.value, (list, tuple)):
                                data = [1 if v else 0 for v in req.value]
                            else:
                                data = [1 if req.value else 0]
                        else:
                            raise
                elif req.func_code == 6:
                    # write single register
                    try:
                        await loop.run_in_executor(self.executor, lambda: self.client.write_register(addr, req.value, device_id=req.unit_id))
                        data = [req.value] if not isinstance(req.value, list) else req.value
                    except Exception as e:
                        # Broadcasts may not return a response; treat as success
                        if isinstance(e, ModbusIOException) and req.unit_id == 0:
                            data = [req.value] if not isinstance(req.value, list) else req.value
                        else:
                            raise
                elif req.func_code == 15:
                    # write multiple coils
                    try:
                        resp = await loop.run_in_executor(self.executor, lambda: self.client.write_coils(addr, req.value, device_id=req.unit_id))
                        data = getattr(resp, 'bits', None)
                        if data is not None:
                            # Ensure ints 0/1
                            data = [1 if b else 0 for b in data[:req.count]]
                        else:
                            # Fallback to echoing requested values (normalized to 0/1)
                            if isinstance(req.value, (list, tuple)):
                                data = [1 if v else 0 for v in req.value][:req.count]
                            else:
                                data = [1 if req.value else 0]
                    except Exception as e:
                        # No response expected for Modbus broadcast — treat as success
                        if isinstance(e, ModbusIOException) and req.unit_id == 0:
                            if isinstance(req.value, (list, tuple)):
                                data = [1 if v else 0 for v in req.value][:req.count]
                            else:
                                data = [1 if req.value else 0]
                        else:
                            raise
                elif req.func_code == 16:
                    # write multiple registers
                    try:
                        resp = await loop.run_in_executor(self.executor, lambda: self.client.write_registers(addr, req.value, device_id=req.unit_id))
                        data = getattr(resp, 'registers', None)
                        if data is not None:
                            data = data[:req.count]
                        else:
                            # Fallback to echoing requested values
                            data = req.value if isinstance(req.value, (list, tuple)) else [req.value]
                    except Exception as e:
                        # Broadcasts may not return a response; treat as success
                        if isinstance(e, ModbusIOException) and req.unit_id == 0:
                            data = req.value if isinstance(req.value, (list, tuple)) else [req.value]
                        else:
                            raise
                else:
                    log.warning("Unsupported function code: %s", req.func_code)
                    data = []

                # If reads returned None or no data, try to return an empty list
                if data is None:
                    data = []

                # record response time and compute RTU latency
                req.resp_ts = time.time()
                rtu_ms = (req.resp_ts - req.forward_ts) * 1000 if req.forward_ts else 0
                total_ms = (req.resp_ts - req.queue_ts) * 1000 if req.queue_ts else 0

                # Satisfy the waiting future
                if req.future and not req.future.done():
                    req.future.set_result(data)

                worker_log.debug("Completed request unit=%s addr=%s data=%s rtu_ms=%.1f total_ms=%.1f resp_ts=%.6f", req.unit_id, req.address, data, rtu_ms, total_ms, req.resp_ts)

            except Exception as e:
                worker_log.exception("Worker Exception")
                # mark connection down on exception during I/O
                try:
                    self._connected = False
                except Exception:
                    pass
                if req.future and not req.future.done():
                        req.future.set_exception(e)
            finally:
                # Force small delay to give RS485 bus a breather (Optional)
                await asyncio.sleep(0.01)
                try:
                    self.queue.task_done()
                except Exception:
                    pass

# Instantiate the Global Manager
serial_manager = SerialManager()

# ================= Data Store: The Proxy =================

class GatewayBlock(ModbusSparseDataBlock):
    """
    A Custom DataBlock that intercepts getValues/setValues 
    and redirects them to the SerialManager.
    """
    
    # We need to pass the Unit ID to the block, but DataBlock is generic.
    # So we use a Context wrapper to inject it, or hack it here.
    # Better approach: The Context calls the block. Pymodbus structure makes 
    # passing UnitID down to Block hard. 
    # WORKAROUND: We put the logic in the Context, not the Block.
    pass

class GatewayContext(ModbusDeviceContext):
    """
    Custom Context to intercept requests per Unit ID
    """
    def __init__(self, unit_id):
        self.unit_id = unit_id
        # Use sparse blocks to save memory
        super().__init__(
            GatewayBlock({0:0}), GatewayBlock({0:0}), 
            GatewayBlock({0:0}), GatewayBlock({0:0})
        )

    def getValues(self, fc, address, count=1):
        """Intercept READ requests"""
        context_log.debug("TCP READ request fc=%s id=%s addr=%s count=%s", fc, self.unit_id, address, count)
        tcp_log.debug("TCP activity: READ fc=%s id=%s addr=%s count=%s", fc, self.unit_id, address, count)
        
        # Allow unit 0 (broadcast) through; broadcast read-after-write is
        # handled by write-path (fire-and-forget). Do not early-return here.

        # Map write FCs (called by pymodbus after a write) to read-coils
        # so that getValues used for read-after-write does not perform another write.
        read_fc = fc
        if fc in (5, 15):
            read_fc = 1

        # Create Request Wrapper (use mapped read_fc)
        req = RtuRequest(self.unit_id, read_fc, address, count=count)
        
        # Submit to Manager and WAIT for result (Threadsafe)
        # Note: getValues is sync, but we need to wait for async result.
        # Ideally we use `asyncio.run_coroutine_threadsafe`, but we are inside the loop.
        # This is the tricky part of Pymodbus v3 Async Server + Custom Logic.
        
        # SOLUTION: We use a Future and wait on it.
        # However, we cannot block the Event Loop.
        # Since `StartAsyncTcpServer` calls this, if we block here, we block the server.
        # BUT, standard Pymodbus doesn't support async getValues yet.
        # We will use `asyncio.create_task` to submit, but we must return something immediately?
        # No, Pymodbus expects the data NOW.
        
        # COMPROMISE for Pymodbus Architecture: 
        # We rely on the fact that we can call async code here if we handle it right.
        # Actually, let's use the Future's result.
        
        try:
            # Fallback synchronous bridge for environments that call this sync.
            fut = asyncio.run_coroutine_threadsafe(
                serial_manager.submit_request(req),
                serial_manager_loop,
            )
            server_wait_start = time.time()
            res = fut.result(timeout=SERVER_TIMEOUT)
            server_wait_end = time.time()
            server_wait_ms = (server_wait_end - server_wait_start) * 1000
            context_log.debug("getValues result fc=%s id=%s addr=%s -> %s (server_wait_ms=%.1f)", fc, self.unit_id, address, repr(res), server_wait_ms)
            tcp_log.debug("TCP READ result fc=%s id=%s addr=%s -> %s (server_wait_ms=%.1f)", fc, self.unit_id, address, repr(res), server_wait_ms)
            return res
        except RuntimeError:
            # We're likely already running in the event loop; fall through to async path
            pass
        except Exception as e:
            context_log.exception("Synchronous getValues bridge failed: %s", e)
            return []

        # Direct Await Hack:
        # Since we are in the loop, we can't easily wait for another task without 'await'.
        # But getValues is not async def.
        # This is why standard Gateway implementations often use Sync Server with Threads.
        
        # However, for this specific request, let's use the 'Context' approach 
        # that allows passing the work to the worker.
        
        # If synchronous bridge failed, return gateway no response
        return ExcCodes.GATEWAY_NO_RESPONSE

    def setValues(self, fc, address, values):
        """Intercept WRITE requests"""
        context_log.debug("TCP WRITE request fc=%s id=%s addr=%s values=%s", fc, self.unit_id, address, values)
        tcp_log.debug("TCP activity: WRITE fc=%s id=%s addr=%s values=%s", fc, self.unit_id, address, values)
        
        # For writes, we handle single values logic
        val = values[0] if len(values) == 1 else values

        # Ensure request `count` reflects number of items for multi-write
        req_count = len(values) if isinstance(values, (list, tuple)) else 1
        req = RtuRequest(self.unit_id, fc, address, value=val, count=req_count)
        
        # Same sync/async bridge issue.
        # We will solve this by running the Serial Manager in a SEPARATE THREAD with its own Loop.
        # Broadcast writes (unit id 0) should be forwarded but do not wait
        # for a response and should not cause read-after-write behavior.
        if self.unit_id == 0:
            try:
                # fire-and-forget the write to the serial loop
                asyncio.run_coroutine_threadsafe(serial_manager.submit_request(req), serial_manager_loop)
                # Return normalized success for FC5/6/15/16 as pymodbus expects
                if fc in (5,):
                    if isinstance(val, (list, tuple)):
                        return [1 if v else 0 for v in val]
                    return [1 if val else 0]
                if fc in (15,):
                    if isinstance(val, (list, tuple)):
                        return [1 if v else 0 for v in val]
                    return [1 if val else 0]
                if fc in (6,16):
                    return val if isinstance(val, list) else [val]
                return []
            except Exception as e:
                context_log.exception("Broadcast write scheduling failed: %s", e)
                raise

        # Bridge to serial_manager on its loop (normal non-broadcast)
        try:
            cf = asyncio.run_coroutine_threadsafe(serial_manager.submit_request(req), serial_manager_loop)
            res = cf.result(timeout=SERVER_TIMEOUT)
            context_log.debug("setValues completed fc=%s id=%s addr=%s -> %s (server_wait_ms=%.1f)", fc, self.unit_id, address, repr(res), (time.time()-req.queue_ts)*1000 if req.queue_ts else 0)
            tcp_log.debug("TCP WRITE result fc=%s id=%s addr=%s -> %s", fc, self.unit_id, address, repr(res))
            return res
        except Exception as e:
            context_log.exception("Write Failed: %s", e)
            raise

    async def async_getValues(self, fc, address, count=1):
        """Async datastore read used by pymodbus async server."""
        # Allow unit 0 (broadcast) through; read-after-write is suppressed
        # by mapping write FCs to read and by the write-path handling.

        # Map write FCs (called by pymodbus after a write) to read-coils
        read_fc = fc
        if fc in (5, 15):
            read_fc = 1
        req = RtuRequest(self.unit_id, read_fc, address, count=count)
        try:
            cf = asyncio.run_coroutine_threadsafe(serial_manager.submit_request(req), serial_manager_loop)
            wrapped = asyncio.wrap_future(cf)
            res = await asyncio.wait_for(wrapped, timeout=SERVER_TIMEOUT)
            return res
        except Exception as e:
            context_log.exception("async_getValues error: %s", e)
            return ExcCodes.GATEWAY_NO_RESPONSE

    async def async_setValues(self, fc, address, values):
        """Async datastore write used by pymodbus async server."""
        val = values[0] if len(values) == 1 else values
        req_count = len(values) if isinstance(values, (list, tuple)) else 1
        req = RtuRequest(self.unit_id, fc, address, value=val, count=req_count)
        try:
            # For broadcast, schedule and return immediately (no response from RTU)
            if self.unit_id == 0:
                asyncio.run_coroutine_threadsafe(serial_manager.submit_request(req), serial_manager_loop)
                return None

            cf = asyncio.run_coroutine_threadsafe(serial_manager.submit_request(req), serial_manager_loop)
            wrapped = asyncio.wrap_future(cf)
            res = await asyncio.wait_for(wrapped, timeout=SERVER_TIMEOUT)
            # On success return 0/None, on failure return an ExcCodes
            return None
        except Exception as e:
            context_log.exception("async_setValues error: %s", e)
            return ExcCodes.GATEWAY_NO_RESPONSE
        
# ================= Setup & Main =================

# We need a separate event loop for the Serial Manager to allow Thread-Safe blocking calls
serial_manager_loop = asyncio.new_event_loop()

def serial_runner():
    """Entry point for the background thread"""
    asyncio.set_event_loop(serial_manager_loop)
    # Create the async queue on the serial loop so all queue operations
    # are bound to this event loop (avoids cross-loop errors).
    serial_manager.queue = asyncio.Queue()
    # Schedule the serial worker directly on this loop and run
    serial_manager_loop.create_task(serial_manager._worker_loop())
    serial_manager_loop.run_forever()

from threading import Thread
t = Thread(target=serial_runner, daemon=True)
t.start()

async def main():
    print("--- LGS Smart Gateway Starting ---")
    
    # 1. Setup Contexts (Map Unit IDs 0-247) - include broadcast (0)
    slaves = {
        i: GatewayContext(unit_id=i) for i in range(0, 248)
    }
    context = ModbusServerContext(devices=slaves, single=False)
    
    # 2. Start TCP Server
    # We use StartAsyncTcpServer here, but because our GatewayContext
    # offloads work to 'serial_manager_loop' (in another thread),
    # we can use 'future.result()' in getValues without blocking the TCP loop!
    
    # If the process is not root on Unix, binding to ports < 1024 will fail.
    bind_port = TCP_PORT
    try:
        if TCP_PORT < 1024 and os.geteuid() != 0:
            tcp_log.warning("Insufficient privileges for port %d; falling back to 1502", TCP_PORT)
            print(f"Insufficient privileges for port {TCP_PORT}; falling back to 1502")
            bind_port = 1502
    except AttributeError:
        # os.geteuid() may not exist on some platforms (Windows), ignore
        pass

    print(f"Listening on {TCP_HOST}:{bind_port}...")
    tcp_log.debug("Starting TCP server on %s:%s", TCP_HOST, bind_port)
    await StartAsyncTcpServer(
        context=context,
        address=(TCP_HOST, bind_port)
    )
    tcp_log.debug("StartAsyncTcpServer returned (server stopped)")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("Stopping...")