"""
Temporary debug copy of gateway to run cleanly.
"""

from importlib.machinery import SourceFileLoader
import sys

module_path = r"c:\Users\mteer\OneDrive\VS Code Workspace\LGS-Standard-Module\tools\modbus_tcp_to_rtu_gateway.py"
mod = SourceFileLoader('gw_debug', module_path).load_module()
print('Loaded module:', mod.__name__)
# Do not start the server automatically when loading; print available symbols
print('SerialManager:', hasattr(mod, 'SerialManager'))
print('GatewayContext:', hasattr(mod, 'GatewayContext'))
print('Done. To run the server, run the original script directly.')
