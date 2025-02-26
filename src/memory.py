import os
import psutil
import struct

from ctypes import (
    byref,
    c_char_p,
    c_ubyte,
    get_last_error,
    POINTER,
    sizeof,
    string_at,
    WinDLL,
    WinError,
)
from ctypes import c_size_t as SIZE_T
from ctypes import c_void_p as VOID
from ctypes import Structure as STRUCT
from ctypes.wintypes import BOOL, DWORD
from time import sleep
from typing import List, Optional
from src.logger import LOGGER


ptrn_lgls = "72 1F E8 ? ? ? ? 8B 0D ? ? ? 01 FF C1 48"
ptrn_glt = "8B 05 ? ? ? ? 89 ? 48 8D 4D C8"
ptrn_gs = "83 3D ? ? ? ? ? 75 17 8B 43 20 25"
ptrn_gvov = "8B C3 33 D2 C6 44 24 20"


# https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-enumprocessmodulesex
LIST_MODULES_ALL = 0x03
# https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
MEM_COMMIT = 0x00001000
# https://learn.microsoft.com/en-us/windows/win32/memory/memory-protection-constants
PAGE_READWRITE = 0x04
PAGE_NOACCESS = 0x01
PAGE_GUARD = 0x100
# https://learn.microsoft.com/en-us/windows/win32/procthread/process-security-and-access-rights
PROCESS_VM_READ = 0x0010
PROCESS_VM_OPERATION = 0x0008
PROCESS_QUERY_INFORMATION = 0x0400


kernel32 = WinDLL("kernel32", use_last_error=True)
psapi = WinDLL("Psapi.dll", use_last_error=True)


class MODULEINFO(STRUCT):
    _fields_ = [
        ("lpBaseOfDll", VOID),
        ("SizeOfImage", DWORD),
        ("EntryPoint", VOID),
    ]


class MEMORY_BASIC_INFORMATION(STRUCT):
    _fields_ = [
        ("BaseAddress", VOID),
        ("AllocationBase", VOID),
        ("AllocationProtect", DWORD),
        ("RegionSize", SIZE_T),
        ("State", DWORD),
        ("Protect", DWORD),
        ("Type", DWORD),
    ]


VirtualQueryEx = kernel32.VirtualQueryEx
VirtualQueryEx.argtypes = [
    VOID,
    VOID,
    POINTER(MEMORY_BASIC_INFORMATION),
    SIZE_T,
]
VirtualQueryEx.restype = SIZE_T


LOG = LOGGER()


class Scanner:
    def __init__(self, process_name=None):
        if process_name:
            self.process_name = process_name
            self.pid = self.get_process_id()
            self.process_handle = self.get_process_handle()

    class Pointer:
        def __init__(self, scanner, address: int):
            self.scanner: Scanner = scanner
            self.address: int = address

        def add(self, offset: int):
            return self.scanner.Pointer(self.scanner, self.address + offset)

        def sub(self, offset: int):
            return self.scanner.Pointer(self.scanner, self.address - offset)

        def rip(self):
            try:
                data = self.scanner.read_process_memory(
                    self.scanner.process_handle, self.address, 4
                )
                rel_offset = struct.unpack("<i", data)[0]
                rip_addr = self.address + rel_offset + 4
                return self.scanner.Pointer(self.scanner, rip_addr)
            except Exception as e:
                LOG.error(f"[SCANNER] Failed to RIP address at 0x{self.address:X}: {e}")
                return self.scanner.Pointer(self.scanner, 0x0)

        def get_byte(self) -> int:
            return self._get_(1, "b")

        def get_word(self) -> int:
            return self._get_(2, "h")

        def get_dword(self) -> int:
            return self._get_(4, "i")

        def get_qword(self) -> int:
            return self._get_(8, "q")

        def get_float(self) -> float:
            return self._get_(4, "f")

        def get_string(self) -> str:
            try:
                data = self.scanner.read_process_memory(
                    self.scanner.process_handle, self.address, 8
                )
                null_pos = data.find(b"\x00")
                if null_pos != -1:
                    return data[:null_pos].decode("utf-8", errors="ignore")
                else:
                    return data.decode("utf-8", errors="ignore")
            except Exception as e:
                print(f"Failed to read string at 0x{self.address:X}: {e}")
                return None

        def get_c_char_p(self) -> str:
            try:
                data = self.scanner.read_process_memory(
                    self.scanner.process_handle, self.address, 8
                )
                c_char_ptr = c_char_p(data)
                return string_at(c_char_ptr).decode()
            except Exception as e:
                print(f"Failed to read string at 0x{self.address:X}: {e}")
                return None

        def _get_(self, size: int, format: str):
            try:
                data = self.scanner.read_process_memory(
                    self.scanner.process_handle, self.address, size
                )
                return struct.unpack(format, data)[0]
            except Exception as e:
                LOG.error(f"[SCANNER] Failed to read memory at 0x{self.address:X}: {e}")
                return None

        def __repr__(self):
            return f"Pointer<0x{self.address:X}>"

    def find_process(self, process_name, timeout=0.001):
        self.process_name = process_name
        self.pid = self.get_process_id(timeout)
        self.process_handle = self.get_process_handle()

    def is_process_running(self):
        return self.pid not in (None, 0)

    def get_process_id(self, timeout=0.001):
        for p in psutil.process_iter(["name", "exe", "cmdline"]):
            sleep(timeout)
            if (
                self.process_name == p.info["name"]
                or (
                    p.info["exe"]
                    and os.path.basename(p.info["exe"]) == self.process_name
                )
                or (p.info["cmdline"] and p.info["cmdline"][0] == self.process_name)
            ):
                return p.pid
        return None

    def get_process_modules(self):
        try:
            loaded_modules = {}
            process = psutil.Process(self.pid)
            for module in process.memory_maps():
                if module.path and module.path.endswith(".dll"):
                    loaded_modules[os.path.basename(module.path).lower()] = module.path
            return loaded_modules
        except (psutil.NoSuchProcess, psutil.AccessDenied) as e:
            LOG.error(f"[SCANNER] Failed to get process modules: {e}")
            return {}

    def is_module_loaded(self, module_name: str):
        if self.pid and module_name.endswith(".dll"):
            return module_name.lower() in self.get_process_modules()
        return False

    def get_base_address(self) -> int:
        lphModule = (VOID * 1024)()
        lpcbNeeded = DWORD()

        if psapi.EnumProcessModulesEx(
            self.process_handle,  # hProcess
            byref(lphModule),
            sizeof(lphModule),
            byref(lpcbNeeded),
            LIST_MODULES_ALL,  # dwFilterFlag
        ):
            return lphModule[0]
        return 0

    def get_module_info(self, module_handle: int) -> MODULEINFO:
        module_info = MODULEINFO()
        GetModuleInformation = psapi.GetModuleInformation
        GetModuleInformation.argtypes = [
            VOID,
            VOID,
            POINTER(MODULEINFO),
            DWORD,
        ]
        GetModuleInformation.restype = BOOL

        if not GetModuleInformation(
            self.process_handle, module_handle, byref(module_info), sizeof(module_info)
        ):
            error = WinError(get_last_error())
            LOG.critical(f"[SCANNER] Failed to retrieve module info: {error}")
            raise error
        return module_info

    def get_module_size(self) -> int:
        module_handle = self.get_base_address()
        if not module_handle:
            LOG.critical("[SCANNER] Failed to retrieve module handle")
            raise Exception("Failed to retrieve module handle")
        module_info = self.get_module_info(module_handle)
        return module_info.SizeOfImage

    def get_process_handle(self):
        if self.pid:
            hProcess = kernel32.OpenProcess(
                PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION,
                False,
                self.pid,
            )
            kernel32.CloseHandle()
            return hProcess

    def is_address_valid(self, address):
        return 0 < address <= 0x7FFFFFFFFFFF

    def is_memory_readable(self, address):
        if self.process_handle and self.is_address_valid(address):
            mem_info = MEMORY_BASIC_INFORMATION()
            result = VirtualQueryEx(
                self.process_handle,
                VOID(address),
                byref(mem_info),
                sizeof(mem_info),
            )
            if result == 0:
                return False
            return mem_info.State == MEM_COMMIT and mem_info.Protect not in (
                PAGE_NOACCESS,
                PAGE_GUARD,
            )
        return False

    def parse_pattern(self, sig: str) -> List[Optional[int]]:
        pattern = []
        n = len(sig)
        i = 0
        while i < n:
            if sig[i] == " ":
                i += 1
                continue

            if sig[i] != "?":
                if i + 1 >= n:
                    LOG.critical("Incomplete byte in pattern!")
                    raise ValueError("Incomplete byte in pattern!")
                try:
                    c1 = int(sig[i], 16)
                    c2 = int(sig[i + 1], 16)
                    pattern.append((c1 << 4) + c2)
                except ValueError:
                    LOG.critical(f"Invalid hex digit near '{sig[i:i+2]}'")
                    raise ValueError(f"Invalid hex digit near '{sig[i:i+2]}'")
                i += 2
            else:
                pattern.append(None)
                i += 1
        return pattern

    def scan_pattern(
        self, pattern: List[Optional[int]], memory: bytes
    ) -> Optional[int]:
        """
        https://en.wikipedia.org/wiki/Boyer%E2%80%93Moore%E2%80%93Horspool_algorithm
        """
        length = len(pattern)
        module_size = len(memory)
        if length == 0 or module_size < length:
            return None

        max_idx = length - 1
        max_shift = length
        wildcard_idx: Optional[int] = None

        for i in range(max_idx - 1, -1, -1):
            if pattern[i] is None:
                max_shift = max_idx - i
                wildcard_idx = i
                break

        if wildcard_idx is None:
            wildcard_idx = -1

        shift_table = [max_shift] * 256
        for i in range(wildcard_idx + 1, max_idx):
            if pattern[i] is not None:
                shift_table[pattern[i]] = max_idx - i

        scan_end = module_size - length
        current_idx = 0

        while current_idx <= scan_end:
            for sig_idx in range(max_idx, -1, -1):
                ptrn_byte = pattern[sig_idx]
                mem_byte = memory[current_idx + sig_idx]
                if ptrn_byte is not None and mem_byte != ptrn_byte:
                    current_idx += shift_table[memory[current_idx + max_idx]]
                    break
                elif sig_idx == 0:
                    return current_idx
            else:
                return current_idx
        return None

    def read_process_memory(
        self, process_handle: int, address: int, size: int
    ) -> bytes:
        buffer = (c_ubyte * size)()
        bytesRead = SIZE_T(0)
        if not kernel32.ReadProcessMemory(
            process_handle, VOID(address), buffer, size, byref(bytesRead)
        ):
            raise WinError(get_last_error())
        return bytes(buffer)

    def find_pattern(self, sig: str, chunk_size: int = 4096) -> Pointer:
        pattern = self.parse_pattern(sig)
        base_address = self.get_base_address()
        module_size = self.get_module_size()
        current_addr = base_address
        end_addr = base_address + module_size

        LOG.debug(f"[SCANNER] Scanning memory pattern: '{sig}'")
        while current_addr < end_addr:
            read_size = min(chunk_size, end_addr - current_addr)
            try:
                mem_chunk = self.read_process_memory(
                    self.process_handle, current_addr, read_size
                )
            except Exception as e:
                LOG.error(f"[SCANNER] Failed to read memory at 0x{current_addr:X}: {e}")
                current_addr += read_size
                continue

            offset = self.scan_pattern(pattern, mem_chunk)
            if offset is not None:
                LOG.debug(
                    f"[SCANNER] Found pattern at address: 0x{(current_addr + offset):X}"
                )
                return self.Pointer(self, current_addr + offset)

            current_addr += read_size

        return None


def get_game_version():
    scanner = Scanner("GTA5.exe")
    if not scanner.is_process_running():
        LOG.debug("Process not found.")
        return

    gv_addr = scanner.find_pattern(ptrn_gvov)
    if gv_addr:
        try:
            gv = gv_addr.add(0x24).rip().get_string()
            ov = gv_addr.add(0x24).rip().add(0x20).get_string()
            LOG.debug(f"GTA V b{gv} online {ov}")
            # print(gv_addr.add(0x24).rip().get_c_char_p())
            return gv, ov
        except Exception as e:
            LOG.error(e)
    return "Nullbyte", "Nullbyte"
