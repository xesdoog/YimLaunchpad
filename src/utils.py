import hashlib
import inspect
import json
import keyring
import logging
import logging.handlers
import os
import platform
import psutil
import requests
import shutil
import subprocess
import sys
import webbrowser
import winreg

from bs4 import BeautifulSoup
from ctypes import (
    Structure,
    windll,
    c_void_p,
    byref,
    create_string_buffer,
    sizeof,
    POINTER,
)
from ctypes import c_size_t as SIZE_T
from ctypes.wintypes import HANDLE, LPVOID, DWORD
from datetime import datetime, timezone
from github import Github, RateLimitExceededException
from github.Repository import Repository
from pathlib import Path
from pymem import Pymem
from pymem.memory import virtual_query
from requests_cache import DO_NOT_CACHE, install_cache
from time import sleep, time


LAUNCHPAD_PATH = os.path.join(os.getenv("APPDATA"), "YimLaunchpad")
if not os.path.exists(LAUNCHPAD_PATH):
    os.mkdir(LAUNCHPAD_PATH)

WORKDIR = os.path.abspath(os.getcwd())
PARENT_PATH = Path(__file__).parent
ASSETS_PATH = PARENT_PATH / Path(r"assets")
UPDATE_PATH = os.path.join(LAUNCHPAD_PATH, "update")
CONFIG_PATH = os.path.join(LAUNCHPAD_PATH, "settings.json")
YIM_MENU_PATH = os.path.join(os.getenv("APPDATA"), "YimMenu")
YIM_SCRIPTS_PATH = os.path.join(YIM_MENU_PATH, "scripts")
LOG_FILE = os.path.join(LAUNCHPAD_PATH, "yimlaunchpad.log")
LOG_BACKUP = os.path.join(LAUNCHPAD_PATH, "Log Backup")
LAUNCHPAD_URL = "https://github.com/xesdoog/YimLaunchpad"
CLIENT_ID = "Iv23lij2DMB5JgpS7rK7"
AUTH_TOKEN = None
USER_OS = platform.system()
USER_OS_ARCH = platform.architecture()[0][:2]
USER_OS_RELEASE = platform.release()
USER_OS_VERSION = platform.version()
# https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
MEM_COMMIT = 0x00001000
# https://learn.microsoft.com/en-us/windows/win32/memory/memory-protection-constants
PAGE_READWRITE = 0x04
PAGE_NOACCESS = 0x01
PAGE_GUARD = 0x100
# https://learn.microsoft.com/en-us/windows/win32/procthread/process-security-and-access-rights
PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400


def log_init_str(app_version: str):
    return f"""
--- YimLaunchpad ---

    - Version: v{app_version}
    - Operating System: {USER_OS} {USER_OS_RELEASE} x{USER_OS_ARCH} v{USER_OS_VERSION}
    - Working Directory: {LAUNCHPAD_PATH}
    - Executable Directory: {executable_dir()}


"""


kernel32 = windll.kernel32
kernel32.ReadProcessMemory.argtypes = [
    HANDLE,
    LPVOID,
    LPVOID,
    DWORD,
    POINTER(DWORD),
]


class MEMORY_BASIC_INFORMATION(Structure):
    _fields_ = [
        ("BaseAddress", LPVOID),
        ("AllocationBase", LPVOID),
        ("AllocationProtect", DWORD),
        ("RegionSize", SIZE_T),
        ("State", DWORD),
        ("Protect", DWORD),
        ("Type", DWORD),
    ]


mem_info = MEMORY_BASIC_INFORMATION()
VirtualQueryEx = kernel32.VirtualQueryEx
VirtualQueryEx.restype = SIZE_T
VirtualQueryEx.argtypes = [HANDLE, LPVOID, POINTER(MEMORY_BASIC_INFORMATION), SIZE_T]

install_cache(
    cache_name="yimlaunchpad_cache",
    cache_control=True,
    use_temp=True,
    urls_expire_after={
        "api.github.com/*": 360,
        "*": DO_NOT_CACHE,
        "github.com/xesdoog/*": DO_NOT_CACHE,
        "github.com/Mr-X-GTA/*": DO_NOT_CACHE,
    },
)


def res_path(path: str):
    return ASSETS_PATH / Path(path)


def executable_dir():
    return os.path.dirname(os.path.abspath(sys.argv[0]))


def read_cfg(file):
    if os.path.exists(file):
        with open(file, "r") as f:
            data = json.load(f)
            f.close()
            return data
    return None


def read_cfg_item(file, item_name):
    if os.path.exists(file):
        with open(file, "r") as f:
            data = json.load(f)
            if item_name in data:
                f.close()
                return data[item_name]
    return None


def save_cfg(file, list):
    open_mode = os.path.exists(file) and "w" or "x"
    with open(file, open_mode) as f:
        json.dump(list, f, indent=4)
        f.close()


def save_cfg_item(file, item_name, value):
    config = read_cfg(file)
    with open(file, "w") as f:
        config[item_name] = value
        json.dump(config, f, indent=4)
        f.close()


def delete_folder(folder_path, on_fail=None, *args):
    if not os.path.exists(folder_path):
        LOG.error(f"Folder path does not exist. {folder_path}")
        if on_fail:
            on_fail("Folder path does not exist.", [1.0, 0.0, 0.0])
        return

    try:
        shutil.rmtree(folder_path)
    except WindowsError:
        try:
            os.chmod(folder_path, 0o777)
            shutil.rmtree(folder_path)
        except Exception:
            LOG.error(f"Failed to delete {folder_path}")
            if on_fail:
                on_fail(*args)


def delete_file(file_path, on_fail=None, *args):
    if not os.path.exists(file_path) or not os.path.isfile(file_path):
        LOG.error(f"Path either does not exist or is not a file: {file_path}")
        if on_fail:
            on_fail("Path either does not exist or is not a file.", [1.0, 0.0, 0.0])
        return

    try:
        os.remove(file_path)
    except WindowsError:
        try:
            os.chmod(file_path, 0o777)
            os.remove(file_path)
        except Exception:
            LOG.error(f"Failed to delete {file_path}")
            if on_fail:
                on_fail(*args)


class CustomLogHandler(logging.FileHandler):
    def __init__(
        self,
        filename,
        archive_path=LOG_BACKUP,
        archive_name="backup_%Y%m%d_%H%M%S.log",
        max_bytes=524288,
        encoding="utf-8",
        **kwargs,
    ):
        self.archive_path = archive_path
        self.archive_name = archive_name
        self.max_bytes = max_bytes
        self.encoding = encoding

        self._archive_log(filename)
        super().__init__(filename, **kwargs)

    def _archive_log(self, filepath):
        if os.path.exists(filepath) and os.path.getsize(filepath) > self.max_bytes:
            os.makedirs(self.archive_path, exist_ok=True)
            timestamped_name = datetime.now().strftime(self.archive_name)
            archive_file = os.path.join(self.archive_path, timestamped_name)
            os.rename(filepath, archive_file)

    def close(self):
        super().close()
        self._archive_log(self.baseFilename)


class CustomLogFilter(logging.Filter):
    def filter(self, record):
        record.caller_name = inspect.stack()[6].function
        return True


class LOGGER:
    def __init__(self, app_version=""):
        self.app_version = app_version
        self.logger = logging.getLogger("YLP")
        self.logger.addFilter(CustomLogFilter())
        self.logger.setLevel(logging.INFO)
        self.formatter = logging.Formatter(
            fmt="[%(asctime)s] [%(levelname)s] (%(caller_name)s): %(message)s",
            datefmt="%H:%M:%S",
        )
        self.file_handler = CustomLogHandler(LOG_FILE)
        self.file_handler.setLevel(logging.INFO)
        self.file_handler.setFormatter(self.formatter)
        self.console_handler = None
        logging.basicConfig(
            encoding="utf-8",
            level=logging.INFO,
            format="[%(asctime)s] [%(levelname)s] (%(caller_name)s): %(message)s",
            datefmt="%H:%M:%S",
            handlers=[self.file_handler],
        )

    def show_console(self):
        if not kernel32.GetConsoleWindow():
            kernel32.AllocConsole()
            sys.stdout = open("CONOUT$", "w", encoding="utf-8")
            sys.stderr = open("CONOUT$", "w", encoding="utf-8")
            kernel32.SetConsoleTitleW("YimLaunchpad")
        if not self.console_handler:
            self.console_handler = logging.StreamHandler(sys.stdout)
            self.console_handler.setLevel(logging.INFO)
            self.console_handler.setFormatter(self.formatter)
            self.logger.addHandler(self.console_handler)
            print(log_init_str(self.app_version))

    def hide_console(self):
        if kernel32.GetConsoleWindow():
            kernel32.FreeConsole()
            sys.stdout = sys.__stdout__
            sys.stderr = sys.__stderr__

        if self.console_handler:
            self.logger.removeHandler(self.console_handler)
            self.console_handler = None

    def debug(self, msg: str):
        self.logger.debug(msg)

    def info(self, msg: str):
        self.logger.info(msg)

    def warning(self, msg: str):
        self.logger.warning(msg)

    def error(self, msg: str):
        self.logger.error(msg)

    def critical(self, msg: str):
        self.logger.critical(msg)

    def on_init(self):
        with open(LOG_FILE, "a") as f:
            f.write(log_init_str(self.app_version))
            f.flush()
            f.close()


LOG = LOGGER()


if read_cfg_item(CONFIG_PATH, "git_logged_in"):
    AUTH_TOKEN = keyring.get_password("YLPGIT_ACC", "access_token")


class GitHubOAuth:
    _status = ""
    _instance = None
    _abort = False

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = super(GitHubOAuth, cls).__new__(cls)
        return cls._instance

    @classmethod
    def update_status(cls, new_status: str):
        cls._status = new_status

    @classmethod
    def get_status(cls):
        return cls._status

    def reset_abort(func):
        def wrapper(self, *args, **kwargs):
            self._abort = False
            return func(self, *args, **kwargs)

        return wrapper

    def abort(self):
        self._abort = True
        self._status = ""

    @reset_abort
    def parse_response(self, response):
        LOG.info("Parsing response...")
        if response.status_code in {200, 201}:
            LOG.info(f"Received response: {response.status_code} OK.")
            return response.json()
        elif response.status_code == 401:
            LOG.error(
                "Received response 401 ACCESS DENIED. User is not logged in to GitHub."
            )
            self.update_status("You are not authorized. Please login to GitHub.")
            return None
        else:
            LOG.error(f"Task failed! Response code: {response.status_code}")
            self.update_status("Task failed!")
            return None

    def store_tokens(self, access_token, refresh_token, expires_in=28800):
        keyring.set_password("YLPGIT_ACC", "access_token", access_token)
        keyring.set_password("YLPGIT_RFR", "refresh_token", refresh_token)
        keyring.set_password("YLPGIT_EXP", "token_expiry", str(time() + expires_in))

    def get_stored_tokens(self):
        try:
            access_token = keyring.get_password("YLPGIT_ACC", "access_token")
            refresh_token = keyring.get_password("YLPGIT_RFR", "refresh_token")
            expiry = keyring.get_password("YLPGIT_EXP", "token_expiry")
            return access_token, refresh_token, float(expiry) if expiry else None
        except Exception as e:
            LOG.error(e)

    def delete_tokens(self):
        try:
            keyring.delete_password("YLPGIT_ACC", "access_token")
            keyring.delete_password("YLPGIT_RFR", "refresh_token")
            keyring.delete_password("YLPGIT_EXP", "token_expiry")
        except Exception as e:
            LOG.error(e)

    def save_avatar(self, username: str):
        avatar_path = os.path.join(LAUNCHPAD_PATH, "avatar.png")
        url = f"https://avatars.githubusercontent.com/{username}"
        response = requests.get(url, stream=True)
        if response.status_code == 200:
            with open(avatar_path, "wb") as f:
                for chunk in response.iter_content(1024):
                    f.write(chunk)
                f.close()

    def delete_avatar(self):
        delete_file(os.path.join(LAUNCHPAD_PATH, "avatar.png"))

    @reset_abort
    def request_device_code(self):
        LOG.info(
            f"Requesting device code... | App: YimLaunchpad | Client ID: {CLIENT_ID} |"
        )
        url = "https://github.com/login/device/code"
        headers = {"Accept": "application/json"}
        data = {"client_id": CLIENT_ID}
        response = requests.post(url, data=data, headers=headers)
        return self.parse_response(response)

    @reset_abort
    def request_token(self, device_code):
        LOG.info(
            f"Requesting authorization token... | Client ID: {CLIENT_ID} | Device Code: {device_code} |"
        )
        url = "https://github.com/login/oauth/access_token"
        headers = {"Accept": "application/json"}
        data = {
            "client_id": CLIENT_ID,
            "device_code": device_code,
            "grant_type": "urn:ietf:params:oauth:grant-type:device_code",
        }
        response = requests.post(url, data=data, headers=headers)
        return self.parse_response(response)

    @reset_abort
    def poll_for_token(self, device_code, interval):
        while not self._abort:
            response = self.request_token(device_code)
            if response is None:
                LOG.error("Task failed: No response.")
                self.update_status("Login failed!")
                sleep(2)
                return None

            error = response.get("error")
            access_token = response.get("access_token")
            refresh_token = response.get("refresh_token")

            if error:
                if error == "authorization_pending":
                    sleep(interval)
                    continue
                elif error == "slow_down":
                    LOG.debug(
                        f"Too many requests. Increasing wait time to {interval + 5}"
                    )
                    sleep(interval + 5)
                    continue
                elif error == "expired_token":
                    LOG.error("Device code expired.")
                    self.update_status(
                        "The device code has expired! Please login again."
                    )
                    return None
                elif error == "access_denied":
                    LOG.warning("Login canceled.")
                    self.update_status("Login canceled.")
                    return None
                else:
                    LOG.error(f"An error occurred: {response}")
                    self.update_status("Login failed!")
                    return response

            self.store_tokens(access_token, refresh_token)
            LOG.info("Authorization granted.")
            return access_token

    @reset_abort
    def refresh_token(self):
        _, refresh_token, _ = self.get_stored_tokens()
        if not refresh_token:
            LOG.warning("No refresh token found! User must reauthenticate.")
            save_cfg_item(CONFIG_PATH, "git_logged_in", False)
            save_cfg_item(CONFIG_PATH, "git_username", "")
            self.update_status("Failed to get a refresh token! Please login again.")
            sleep(1)
            return None

        url = "https://github.com/login/oauth/access_token"
        headers = {"Accept": "application/json"}
        data = {
            "client_id": CLIENT_ID,
            "grant_type": "refresh_token",
            "refresh_token": refresh_token,
        }

        LOG.info("Refreshing GitHub token...")
        response = requests.post(url, data=data, headers=headers)
        token_data = self.parse_response(response)

        if token_data and "access_token" in token_data:
            LOG.info("Token refreshed successfully!")
            self.store_tokens(token_data["access_token"], token_data["refresh_token"])
            return token_data["access_token"]

        LOG.error("Failed to refresh token! User must reauthenticate.")
        self.delete_tokens()
        save_cfg_item(CONFIG_PATH, "git_logged_in", False)
        save_cfg_item(CONFIG_PATH, "git_username", "")
        self.update_status("Failed to get a refresh token! Please login again.")
        sleep(1)
        return None

    @reset_abort
    def login(self):
        global AUTH_TOKEN
        access_token, _, expiry = self.get_stored_tokens()

        if access_token and time() < expiry:
            LOG.info("Existing token is still valid.")
            AUTH_TOKEN = access_token
            return

        if access_token and time() >= expiry:
            LOG.info("Access token expired. Attempting to refresh...")
            new_token = self.refresh_token()
            if new_token:
                AUTH_TOKEN = new_token
                return

        LOG.info("No valid tokens found. Starting login process...")
        self.delete_tokens()
        device_code_data = self.request_device_code()
        if not device_code_data:
            save_cfg_item(CONFIG_PATH, "git_logged_in", False)
            save_cfg_item(CONFIG_PATH, "git_username", "")
            self.update_status(
                "Failed to refresh GitHub access Token!. Please login again."
            )
            sleep(3)
            return

        verification_uri = device_code_data["verification_uri"]
        user_code = device_code_data["user_code"]
        device_code = device_code_data["device_code"]
        interval = device_code_data["interval"]

        self.update_status(
            f"Please visit: {verification_uri} and enter this code: {user_code}"
        )
        access_token = self.poll_for_token(device_code, interval)

        if access_token:
            LOG.info("Authenticating user...")
            git = Github(access_token)
            save_cfg_item(CONFIG_PATH, "git_logged_in", True)
            save_cfg_item(CONFIG_PATH, "git_username", git.get_user().login)
            self.save_avatar(git.get_user().login)
            AUTH_TOKEN = access_token
            LOG.info(
                f"Successfully authenticated as {git.get_user().login} ({git.get_user().name})"
            )
            self.update_status("Authentication successful")
            sleep(3)

    def logout(self):
        self.delete_tokens()
        self.delete_avatar()
        save_cfg_item(CONFIG_PATH, "git_logged_in", False)
        save_cfg_item(CONFIG_PATH, "git_username", "")
        LOG.info("Logged out of GitHub.")
        self.update_status(
            "Logged out. To fully revoke access, visit https://github.com/settings/apps/authorizations."
        )


class MemoryScanner:
    def __init__(
        self,
        process_name=None,
    ):
        self.process_name = process_name
        self.pid = self.get_process_id()
        self.process_handle = self.get_process_handle()

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
                or p.info["exe"]
                and os.path.basename(p.info["exe"]) == self.process_name
                or p.info["cmdline"]
                and p.info["cmdline"][0] == self.process_name
            ):
                return p.pid
        return None

    def get_process_modules(self):
        loaded_modules = {}
        try:
            process = psutil.Process(self.pid)
            for module in process.memory_maps():
                if module.path and module.path.endswith(".dll"):
                    loaded_modules[os.path.basename(module.path).lower()] = module.path
        except (psutil.NoSuchProcess, psutil.AccessDenied) as e:
            LOG.error(f"Error accessing process {self.pid}: {e}")
            return {}

        return loaded_modules

    def is_module_loaded(self, module_name: str):
        if self.pid and module_name.endswith(".dll"):
            return module_name.lower() in self.get_process_modules()
        return False

    def get_base_address(self):
        if self.pid:
            return Pymem(self.pid).base_address

    def get_process_handle(self):
        if self.pid:
            return kernel32.OpenProcess(
                PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, self.pid
            )

    def is_address_valid(self, address):
        if self.process_handle:
            try:
                mem_info = virtual_query(self.process_handle, address)
                if mem_info.Protect & PAGE_READWRITE:
                    return True
                else:
                    LOG.warning(
                        f"[SCANNER] Memory at {hex(address)} is protected: {mem_info.Protect}"
                    )
                    return False
            except:
                return False
        return False

    def is_memory_readable(self, address):
        if self.process_handle and self.is_address_valid(address):
            mem_info = MEMORY_BASIC_INFORMATION()
            result = VirtualQueryEx(
                self.process_handle, address, byref(mem_info), sizeof(mem_info)
            )

            if result == 0:
                LOG.error(f"[SCANNER] VirtualQueryEx failed for address {hex(address)}")
                return False

            LOG.debug(
                f"[SCANNER] Memory Region: Base={hex(mem_info.BaseAddress)}, State={mem_info.State}, Protect={mem_info.Protect}"
            )
            return mem_info.State == MEM_COMMIT and mem_info.Protect not in (
                PAGE_NOACCESS,
                PAGE_GUARD,
            )
        return False

    def read_process_memory(self, address, size):
        if self.is_address_valid(address) and self.is_memory_readable(address):
            buffer = create_string_buffer(size)
            bytes_read = DWORD(0)
            kernel32.ReadProcessMemory(
                self.process_handle, c_void_p(address), buffer, size, byref(bytes_read)
            )
            return buffer.raw

    def memory_compare(self, memory, pattern: str, mask: str):
        for i in range(len(pattern)):
            if mask[i] == "x" and memory[i] != pattern[i]:
                return False
        return True

    def find_pattern(self, pattern: str, mask: str, chunk_size=1024):
        if not self.process_handle:
            return 0

        found_address = 0
        base_address = self.get_base_address()
        while True:
            memory = self.read_process_memory(
                self.process_handle, base_address, chunk_size
            )
            for i in range(len(memory) - len(pattern) + 1):
                if self.memory_compare(memory[i:], pattern, mask):
                    found_address = base_address + i
                    print("")
                    print(f"Found pattern at address {hex(found_address)}")
                    return found_address
            base_address += chunk_size
            print(f"Scanning {hex(base_address + chunk_size)}", end="\r", flush=True)
            if base_address > 0x7FFFFFFFFFFF:
                LOG.warning("[SCANNER] Memory limit reached!")
                print("")
                print("Memory limit reached!")
                return 0


def stringFind(string: str, sub: str):
    return string.lower().find(sub.lower()) != -1


def stringContains(string: str, subs: dict):
    return any(s.lower() in string.lower() for s in subs)


def get_launchpad_version():
    try:
        r = requests.get(f"{LAUNCHPAD_URL}/tags")
        soup = BeautifulSoup(r.content, "html.parser")
        result = soup.find(class_="Link--primary Link")
        s = str(result)
        result = s.replace("</a>", "")
        charLength = len(result)
        latest_version = result[charLength - 7 :]
        return latest_version
    except Exception as e:
        LOG.info(f"Failed to get the latest GitHub version! Traceback: {e}")
        return None


def calc_file_checksum(file):
    if os.path.exists(file):
        file_name = os.path.basename(file)
        LOG.info(f"Calculating SHA256 checksum of {file_name}")
        sha256_hash = hashlib.sha256()
        with open(file, "rb") as f:
            for byte_block in iter(lambda: f.read(4096), b""):
                sha256_hash.update(byte_block)
            LOG.info(f"{file_name}: {sha256_hash.hexdigest()}")
        return sha256_hash.hexdigest()
    else:
        LOG.error("The file specified does not exist.")
        return None


def append_short_sha(file: str):
    checksum = str(calc_file_checksum(file))[:6]
    if len(checksum) == 6:
        return f"{os.path.basename(file)} [{checksum}]"
    return str(os.path.basename(file))


def get_remote_checksum():
    try:
        LOG.info(
            "Checking the latest YimMenu release on https://github.com/Mr-X-GTA/YimMenu/releases/tag/nightly"
        )
        r = requests.get("https://github.com/Mr-X-GTA/YimMenu/releases/tag/nightly")
        soup = BeautifulSoup(r.content, "html.parser")
        list = soup.find(class_="notranslate")
        l = list("code")
        s = str(l)
        tag = s.replace("[<code>", "")
        sep = " "
        head, sep, _ = tag.partition(sep)
        sha256_string = head
        char_count = len(sha256_string)
        if char_count == 64:
            LOG.info(f"Latest YimMenu release checksum: {sha256_string}")
            return sha256_string
    except Exception as e:
        LOG.error(f"An error occured! Traceback: function get_remote_checksum() -> {e}")
        return None


def is_file_saved(name, list):
    if len(list) > 0:
        for file in list:
            if file["name"] == name:
                return True
    return False


def get_rgl_path() -> str:
    regkey = winreg.OpenKey(
        winreg.HKEY_LOCAL_MACHINE,
        r"SOFTWARE\\WOW6432Node\\Rockstar Games\\",
        0,
        winreg.KEY_READ,
    )

    try:
        subkey = winreg.OpenKey(regkey, r"Grand Theft Auto V")
        subkeyValue = winreg.QueryValueEx(subkey, r"InstallFolder")
        LOG.debug(f"Rockstar Games Launcher version path: {subkeyValue[0]}")
        return subkeyValue[0]
    except OSError as err:
        LOG.error(
            f"An error has occured while trying to read Rockstar Games Launcher's path! Traceback: {err}"
        )


def open_folder(path: str):
    if not os.path.exists(path):
        os.makedirs(path)
    subprocess.Popen(f"explorer {os.path.abspath(path)}")


def visit_url(url: str):
    webbrowser.open_new_tab(url)


def is_repo_starred(repo_name: str, starred_list: list):
    if len(starred_list) > 0:
        return repo_name in starred_list
    return False


def is_repo_empty(repo: Repository, path=""):
    try:
        contents = repo.get_contents(path)
        for file in contents:
            if file.type == "dir":
                is_repo_empty(repo, file.path)
            elif file.type == "file" and file.name.endswith(".lua"):
                return False
        return True
    except RateLimitExceededException:
        return True


def lua_script_needs_update(repo: Repository, installed: dict) -> bool:
    if len(installed) > 0 and repo.name in installed:
        local_date = installed[repo.name]["date_modified"]
        last_commit = repo.get_commits()[0]
        remote_date = last_commit.commit.committer.date.replace(tzinfo=timezone.utc)
        return local_date < remote_date

    return False


def get_lua_repos():
    LOG.info("Fetching repositories from https://github.com/YimMenu-Lua")
    user = None
    repos = []
    update_available = []
    starred_repos = []
    installed = get_installed_scripts()
    exclude_repos = {
        "example",
        "submission",
        "samurai-scenarios",
        "samurai-animations",
        "yimlls",
    }

    if not AUTH_TOKEN:
        git = Github()
        LOG.info(
            "User is not logged in to GitHub. Sending non-authenticated requests..."
        )

    try:
        GitHubOAuth().login()
        git = Github(AUTH_TOKEN)
        user = git.get_user()
        LOG.info(f"User is logged in to GitHub as {user.login} ({user.name})")
    except Exception:
        git = Github()
        AUTH_TOKEN = None

    rate_limit, _ = git.rate_limiting
    LOG.info(f"Current API rate limit is {rate_limit} requests/hour.")
    try:

        if rate_limit == 0:
            LOG.error("Failed to fetch GitHub repositories! Rate limit exceeded.")
            return [], [], [], True, 0

        YimMenu_Lua = git.get_organization("YimMenu-Lua")

        for repo in YimMenu_Lua.get_repos(sort="pushed", direction="desc"):
            if str(repo.name).lower() not in exclude_repos:
                repos.append(repo)
                if repo.name in installed:
                    if lua_script_needs_update(repo, installed):
                        update_available.append(repo.name)
                if user:
                    if user.has_in_starred(repo):
                        starred_repos.append(repo.name)
            else:
                LOG.info(f"Skipping repository '{repo.name}'")
        LOG.info(f"Loaded {len(repos)} repositories.")
        return repos, update_available, starred_repos, False, rate_limit

    except RateLimitExceededException:
        LOG.error("Failed to fetch GitHub repositories! Rate limit exceeded.")
        return [], [], [], True, 0


def refresh_repository(repo: Repository):
    git = Github(AUTH_TOKEN)
    rate_limit, _ = git.rate_limiting
    try:
        if rate_limit == 0:
            return repo, 0

        YimMenu_Lua = git.get_organization("YimMenu-Lua")
        return YimMenu_Lua.get_repo(repo.name), rate_limit
    except RateLimitExceededException:
        return repo, 0


def get_installed_scripts():
    Luas = {}

    if os.path.exists(YIM_SCRIPTS_PATH):

        enabled_scripts = next(os.walk(YIM_SCRIPTS_PATH))[1]
        # disabled_path = os.path.join(YIM_SCRIPTS_PATH, "disabled")
        # disabled_scripts = next(os.walk(disabled_path))[1]

        for dir_1 in enabled_scripts:
            if dir_1 not in ("includes", "disabled") and not str(dir_1).startswith("."):
                mtime_1 = max(
                    os.stat(root).st_mtime
                    for root, _, _ in os.walk(os.path.join(YIM_SCRIPTS_PATH, dir_1))
                )
                date_1 = datetime.fromtimestamp(mtime_1, tz=timezone.utc)
                Luas.update({dir_1: {"disabled": False, "date_modified": date_1}})

        # for dir_2 in disabled_scripts:
        #     if dir_2 not in ("includes", "disabled") and not str(dir_2).startswith("."):
        #         mtime_2 = max(
        #             os.stat(root).st_mtime
        #             for root, _, _ in os.walk(os.path.join(disabled_path, dir_2))
        #         )
        #         date_2 = datetime.fromtimestamp(mtime_2, tz=timezone.utc)
        #         Luas.update({dir_2: {"disabled": True, "date_modified": date_2}})

    return Luas


def is_script_installed(repo: Repository):
    return os.path.exists(os.path.join(YIM_SCRIPTS_PATH, repo.name))


def is_script_disabled(repo: Repository):
    return os.path.exists(
        os.path.join(os.path.join(YIM_SCRIPTS_PATH, "disabled"), repo.name)
    )


def run_updater():

    main_file = os.path.join(executable_dir(), "YimLaunchpad.exe")
    vbs_file = os.path.join(UPDATE_PATH, "run_update.vbs")
    batch_file = os.path.join(UPDATE_PATH, "update.bat")
    temp_file = os.path.join(UPDATE_PATH, "YimLaunchpad.exe")

    batch_script = f"""@echo off
    :waitloop
    timeout /t 1 /nobreak >nul
    tasklist | find /i "YimLaunchpad.exe" >nul
    if not errorlevel 1 goto waitloop

    del "{main_file}"
    move "{temp_file}" "{main_file}"
    cd /d "{executable_dir()}"
    start "" ./YimLaunchpad.exe
    rmdir /s /q "{UPDATE_PATH}"
    """

    vb_script = (
        """CreateObject("Wscript.Shell").Run "" & WScript.Arguments(0) & "", 0, False"""
    )

    if not os.path.exists(UPDATE_PATH):
        os.mkdir(UPDATE_PATH)

    with open(batch_file, "w") as f:
        f.write(batch_script)

    with open(vbs_file, "w") as f:
        f.write(vb_script)

    subprocess.Popen(
        f'wscript.exe "{vbs_file}" "{batch_file}"', shell=True, creationflags=8
    )
    sys.exit(0)


def add_exclusion(paths: list):
    LOG.info("Starting Windows Powershell...")
    exclusion_commands = "\n".join(
        f"    Write-Host 'Adding '{path}'' -ForegroundColor Cyan;\nAdd-MpPreference -ExclusionPath '{path}';"
        for path in paths
    )
    try:
        exit_code = subprocess.call(
            [
                "powershell.exe",
                "-noprofile",
                "-c",
                f"""
            Start-Process -Verb RunAs -PassThru -Wait powershell.exe -Args "
                -noprofile -NoExit -Command &{{
                    {exclusion_commands}
                    Write-Host 'Done.' -ForegroundColor Green;
                    Read-Host 'Press any key to exit'
                    exit
                }}
            "
            """,
            ],
            shell=False,
        )
        LOG.info(f"Process exited with code: {exit_code}")
    except Exception:
        LOG.error(f"Failed to add exclusions.")
        pass
