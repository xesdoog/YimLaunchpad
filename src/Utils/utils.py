import hashlib
import json
import keyring
import os
import requests
import shutil
import struct
import subprocess
import sys
import webbrowser
import winreg

from bs4 import BeautifulSoup
from ctypes import WinDLL
from datetime import datetime, timezone
from github import Github, RateLimitExceededException
from github.Repository import Repository
from psutil import win_service_get
from requests_cache import DO_NOT_CACHE, install_cache
from .logger import LOGGER
from time import sleep, time


LAUNCHPAD_PATH = os.path.join(os.getenv("APPDATA"), "YimLaunchpad")
WORKDIR = os.path.abspath(os.getcwd())
UPDATE_PATH = os.path.join(LAUNCHPAD_PATH, "update")
CONFIG_PATH = os.path.join(LAUNCHPAD_PATH, "settings.json")
YIM_MENU_PATH = os.path.join(os.getenv("APPDATA"), "YimMenu")
YIM_SCRIPTS_PATH = os.path.join(YIM_MENU_PATH, "scripts")
LAUNCHPAD_URL = "https://github.com/xesdoog/YimLaunchpad"
EGS_LAUNCH_CMD = "com.epicgames.launcher://apps/9d2d0eb64d5c44529cece33fe2a46482?action=launch&silent=true"
STEAM_LAUNCH_CMD = "steam://run/271590"
CLIENT_ID = "Iv23lij2DMB5JgpS7rK7"
AUTH_TOKEN = None
LOG = LOGGER()

kernel32 = WinDLL("kernel32", use_last_error=True)

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


def executable_dir():
    return os.path.dirname(os.path.abspath(sys.argv[0]))


def read_cfg(file):
    if os.path.exists(file):
        with open(file, "r") as f:
            try:
                data = json.load(f)
                f.close()
                return data
            except Exception:
                return None
    return None


def read_cfg_item(file, item_name):
    if os.path.exists(file):
        with open(file, "r") as f:
            try:
                data = json.load(f)
                if item_name in data:
                    f.close()
                    return data[item_name]
            except Exception:
                return None
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
        LOG.debug("Parsing response...")
        if response.status_code in {200, 201}:
            LOG.debug(f"Received response: {response.status_code} OK.")
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

    def save_tokens(self, access_token, refresh_token, expires_in=28800):
        keyring.set_password("YLPGIT_ACC", "access_token", access_token)
        keyring.set_password("YLPGIT_RFR", "refresh_token", refresh_token)
        keyring.set_password("YLPGIT_EXP", "token_expiry", str(time() + expires_in))

    def get_tokens(self):
        try:
            access_token = keyring.get_password("YLPGIT_ACC", "access_token")
            refresh_token = keyring.get_password("YLPGIT_RFR", "refresh_token")
            expiry = keyring.get_password("YLPGIT_EXP", "token_expiry")
            return access_token, refresh_token, float(expiry) if expiry else None
        except Exception:
            LOG.error("An error occured!")
            return None, None, None

    def delete_tokens(self):
        try:
            keyring.delete_password("YLPGIT_ACC", "access_token")
            keyring.delete_password("YLPGIT_RFR", "refresh_token")
            keyring.delete_password("YLPGIT_EXP", "token_expiry")
        except Exception:
            LOG.error("An error occured!")

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

            self.save_tokens(access_token, refresh_token)
            LOG.info("Authorization granted.")
            return access_token

    @reset_abort
    def refresh_token(self):
        _, refresh_token, _ = self.get_tokens()
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
            self.save_tokens(token_data["access_token"], token_data["refresh_token"])
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
        access_token, _, expiry = self.get_tokens()

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


AUTH_TOKEN, _, _ = GitHubOAuth().get_tokens()


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
    except Exception:
        LOG.error(f"Failed to get the latest GitHub version!")
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


def append_short_sha(file_path: str):
    checksum = str(calc_file_checksum(file_path))[:6]
    if checksum:
        return f"{os.path.basename(file_path)} [{checksum}]"
    return str(os.path.basename(file_path))


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
    except Exception:
        LOG.error(f"An error occured!")
        return None


def is_file_saved(file_name, saved_list):
    if len(saved_list) > 0:
        for file in saved_list:
            if file["name"] == file_name:
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
    except Exception:
        LOG.error(
            f"An error has occured while trying to read Rockstar Games Launcher's path"
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
    global AUTH_TOKEN

    LOG.info("Fetching repositories from https://github.com/YimMenu-Lua")
    user = None
    repos = []
    update_available = []
    starred_repos = []
    installed = get_installed_scripts()
    exclude_repos = {
        "Bunker-Research-Unlocker",
        "example",
        "GTA-Prostitute-Spawner",
        "hi",
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

    rl, _ = git.rate_limiting
    LOG.info(f"Current API rate limit is {rl} requests/hour.")
    try:
        if rl == 0:
            LOG.error("Failed to fetch GitHub repositories! Rate limit exceeded.")
            return [], [], [], 0

        YimMenu_Lua = git.get_organization("YimMenu-Lua")

        for repo in YimMenu_Lua.get_repos(sort="pushed", direction="desc"):
            rl, _ = git.rate_limiting
            if rl == 0:
                LOG.warning(
                    f"API rate limit reached while fetching repositories! Managed to load {len(repos)} repositories."
                )
                return (
                    repos,
                    update_available,
                    starred_repos,
                    0,
                )

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
        rl, _ = git.rate_limiting
        return repos, update_available, starred_repos, rl

    except Exception:
        LOG.error("Failed to fetch GitHub repositories! Rate limit exceeded.")
        return [], [], [], 0


def refresh_repository(repo: Repository):
    git = Github(AUTH_TOKEN)
    requests_remaining, _ = git.rate_limiting
    try:
        if requests_remaining == 0:
            return repo, 0

        YimMenu_Lua = git.get_organization("YimMenu-Lua")
        return YimMenu_Lua.get_repo(repo.name), requests_remaining
    except Exception:
        return repo, 0


def get_installed_scripts():
    Luas = {}
    if os.path.exists(YIM_SCRIPTS_PATH):
        enabled_scripts = next(os.walk(YIM_SCRIPTS_PATH))[1]
        for dir_1 in enabled_scripts:
            if dir_1 not in ("includes", "disabled") and not str(dir_1).startswith("."):
                mtime_1 = max(
                    os.stat(root).st_mtime
                    for root, _, _ in os.walk(os.path.join(YIM_SCRIPTS_PATH, dir_1))
                )
                date_1 = datetime.fromtimestamp(mtime_1, tz=timezone.utc)
                Luas.update({dir_1: {"disabled": False, "date_modified": date_1}})
    return Luas


def is_script_installed(repo: Repository):
    return os.path.exists(os.path.join(YIM_SCRIPTS_PATH, repo.name))


def is_script_disabled(repo: Repository):
    return os.path.exists(
        os.path.join(os.path.join(YIM_SCRIPTS_PATH, "disabled"), repo.name)
    )


def does_script_have_updates(repo_name: str, updatable_scripts: list) -> bool:
    return len(updatable_scripts) > 0 and repo_name in updatable_scripts


def run_updater(mei_path=None):
    main_file = os.path.join(executable_dir(), "YimLaunchpad.exe")
    vbs_file = os.path.join(UPDATE_PATH, "run_update.vbs")
    batch_file = os.path.join(UPDATE_PATH, "update.bat")
    temp_file = os.path.join(UPDATE_PATH, "YimLaunchpad.exe")

    batch_script = f"""@echo off
    setlocal enableextensions
    set RETRIES=3
    set WAIT=2

    :waitloop
    timeout /t 1 /nobreak >nul
    tasklist | find /i "YimLaunchpad.exe" >nul
    if not errorlevel 1 goto waitloop

    {"if exist \"" + mei_path + "\" (" if mei_path else ""}
    echo Waiting for PyInstaller temp folder to be released...
    :waitmeipass
    timeout /t 2 /nobreak >nul
    (rd "{mei_path}" 2>nul) || goto waitmeipass
    {")" if mei_path else ""}

    move /Y "{temp_file}" "{main_file}"

    :retrylaunch
    timeout /t %WAIT% /nobreak >nul
    start "" /wait "{main_file}"
    if errorlevel 1 (
        set /a RETRIES-=1
        if %RETRIES% GTR 0 goto retrylaunch
    ) else (
        echo App started successfully.
        rmdir /s /q "{UPDATE_PATH}"
    )
    """

    vb_script = (
        """CreateObject("Wscript.Shell").Run "" & WScript.Arguments(0) & "", 0, False"""
    )

    if not os.path.exists(UPDATE_PATH):
        os.mkdir(UPDATE_PATH)

    with open(batch_file, "w") as f:
        f.write(batch_script)
        f.flush()
        f.close()

    with open(vbs_file, "w") as f:
        f.write(vb_script)
        f.flush()
        f.close()

    subprocess.Popen(
        f'wscript.exe "{vbs_file}" "{batch_file}"', shell=True, creationflags=0x00000008
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


def run_detached_process(arg: str):
    try:
        return subprocess.run(
            args=f"cmd /c start {arg}",
            creationflags=0x00000008 | 0x00000200,
            close_fds=True,
        ).returncode
    except Exception:
        return -1


def to_hex(value: int):
    return f"0x{value & 0xFFFFFFFF if value < 0 else value:X}"


def splash_cleanup(mei_path):
    if not mei_path:
        return
    
    zlib_path = os.path.join(mei_path, "__splash", "zlib.dll")
    if os.path.exists(zlib_path):
        try:
            kernel32.FreeLibrary(WinDLL(zlib_path)._handle)
        except Exception:
            LOG.error("Failed to unload zlib.dll!")


def is_service_running(service_name: str) -> bool:
    try:
        instance = win_service_get(service_name)
        service = instance.as_dict()
        return service is not None and service["status"] == "running"
    except Exception:
        return False


def is_valid_dll(file_path: str):
    if not os.path.isfile(file_path):
        return False
    
    if not file_path.endswith(".dll"):
        return False

    try:
        with open(file_path, "rb") as f:
            if f.read(2) != b"MZ":
                return False

            f.seek(0x3C)
            pe_offset = struct.unpack("<I", f.read(4))[0]
            f.seek(pe_offset)
            pe_sig = f.read(4)
            if pe_sig != b"PE\x00\x00":
                return False

            machine_type = struct.unpack("<H", f.read(2))[0]
            return machine_type in (0x014C, 0x8664)  # x86, x64
    except Exception:
        return False
