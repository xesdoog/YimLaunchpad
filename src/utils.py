import hashlib
import json
import logging
import logging.handlers
import os
import platform
import requests
import subprocess
import sys
import webbrowser
import winreg

from bs4       import BeautifulSoup
from datetime  import datetime
from functools import cache
from github    import Github, RateLimitExceededException

LAUNCHPAD_PATH = os.path.join(os.getenv("APPDATA"), "YimLaunchpad")
if not os.path.exists(LAUNCHPAD_PATH):
    os.mkdir(LAUNCHPAD_PATH)

LOG_FILE   = os.path.join(LAUNCHPAD_PATH, "yimlaunchpad.log")
LOG_BACKUP = os.path.join(LAUNCHPAD_PATH, "Log Backup")

def executable_path():
    return os.path.dirname(os.path.abspath(sys.argv[0]))


class CustomLogHandler(logging.FileHandler):

    def __init__(self, filename, archive_path = LOG_BACKUP, archive_name = "backup_%Y%m%d_%H%M%S.log", max_bytes = 524288,  **kwargs):
        self.archive_path = archive_path
        self.archive_name = archive_name
        self.max_bytes    = max_bytes

        self._archive_log(filename)
        super().__init__(filename, **kwargs)

    def _archive_log(self, filepath):
        if os.path.exists(filepath) and os.path.getsize(filepath) > self.max_bytes:
            os.makedirs(self.archive_path, exist_ok = True)
            timestamped_name = datetime.now().strftime(self.archive_name)
            archive_file = os.path.join(self.archive_path, timestamped_name)
            os.rename(filepath, archive_file)

    def close(self):
        super().close()
        self._archive_log(self.baseFilename)


class LOGGER:

    def __init__(self):
        self.logger = logging.getLogger("YLP")
        log_handler = CustomLogHandler(LOG_FILE)
        logging.basicConfig(
            encoding = 'utf-8',
            level    = logging.INFO,
            format   = '[%(asctime)s] [%(levelname)s] [%(name)s] %(message)s',
            datefmt  = '%H:%M:%S',
            handlers = [log_handler]
        )

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

    def OnStart(self, parent_path):
        LOCAL_VERSION = "v1.0.0"
        userOS        = platform.system()
        userOSarch    = platform.architecture()
        userOSrel     = platform.release()
        userOSver     = platform.version()
        workDir       = parent_path
        exeDir        = executable_path() + '\\'
        logfile = open(LOG_FILE, "a")
        logfile.write("\n--- YimLaunchpad ---\n\n")
        logfile.write(f"    ¤ Version: {LOCAL_VERSION}\n")
        logfile.write(f"    ¤ Operating System: {userOS} {userOSrel} x{userOSarch[0][:2]} v{userOSver}\n")
        logfile.write(f"    ¤ Working Directory: {workDir}\n")
        logfile.write(f"    ¤ Executable Directory: {exeDir}\n\n\n")
        logfile.flush()
        logfile.close()

LOG = LOGGER()


def get_launchpad_version():
    try:
        r = requests.get("https://github.com/xesdoog/YimLaunchpad/tags")
        soup = BeautifulSoup(r.content, "html.parser")
        result = soup.find(class_="Link--primary Link")
        s = str(result)
        result = s.replace("</a>", "")
        charLength = len(result)
        latest_version = result[charLength - 7:]
        return latest_version
    except Exception as e:
        LOG.info(f'Failed to get the latest GitHub version! Traceback: {e}')
        return None


def calc_file_checksum(file):
    if os.path.exists(file):
        LOG.info(f"Found local DLL under {file}")
        sha256_hash = hashlib.sha256()
        with open(file, "rb") as f:
            for byte_block in iter(lambda: f.read(4096), b""):
                sha256_hash.update(byte_block)
        LOG.info(f"Local DLL checksum {sha256_hash.hexdigest()}")
        return sha256_hash.hexdigest()
    else:
        LOG.error("Local DLL not found!")
        return None


def append_short_sha(file) -> str:
    checksum  = str(calc_file_checksum(file))[:6]
    file_name = f"{os.path.basename(file)} [{checksum}]"
    return file_name


def get_remote_checksum():
    try:
        LOG.info("Checking the latest YimMenu release on https://github.com/Mr-X-GTA/YimMenu/releases/tag/nightly")
        r = requests.get("https://github.com/Mr-X-GTA/YimMenu/releases/tag/nightly")
        soup = BeautifulSoup(r.content, "html.parser")
        list = soup.find(class_ = "notranslate")
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
        LOG.error(f'An error occured! Traceback: {e}')
        return None


def is_file_saved(name, list):
    if len(list) > 0:
        for file in list:
            if file["name"] == name:
                return True
    return False


def read_cfg(file):
    if os.path.exists(file):
        with open(file, "r") as f:
            return json.load(f)
    else:
        return None


def read_cfg_item(file, item_name):
    if os.path.exists(file):
        with open(file, "r") as f:
            return json.load(f)[item_name]
    else:
        return None


def save_cfg(file, list):
    open_mode = os.path.exists(file) and "w" or "x"
    with open(file, open_mode) as f:
        json.dump(list, f, indent = 4)


def save_cfg_item(file, item_name, value):
    config = read_cfg(file)
    with open(file, "w") as f:
        config[item_name] = value
        json.dump(config, f, indent = 4)

    
def get_rgl_path() -> str:
    regkey = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\\WOW6432Node\\Rockstar Games\\', 0, winreg.KEY_READ)
    try:
        subkey = winreg.OpenKey(regkey, r'Grand Theft Auto V')
        subkeyValue = winreg.QueryValueEx(subkey, r'InstallFolder')
        LOG.debug(f"Rockstar Games Launcher version path: {subkeyValue[0]}")
        return (subkeyValue[0])
    except OSError as err:
        LOG.error(f"An error has occured while trying to read Rockstar Games Launcher's path! Traceback: {err}")


def open_folder(path):
    if not os.path.exists(path):
        os.makedirs(path)
    subprocess.Popen(f"explorer {os.path.abspath(path)}")


def visit_url(url):
    webbrowser.open_new_tab(url)


def is_repo_empty(repo, path = ""):
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


@cache
def get_lua_repos():
    repos        = []
    exclude_repos = {
        "example",
        "submission",
        "samurai-scenarios",
        "samurai-animations",
        "yimlls",
    }
    git          = Github()
    core_limit   = git.get_rate_limit().core.remaining
    search_limit = git.get_rate_limit().search.remaining
    reset_time   = {"hours": git.get_rate_limit().core.reset.hour, "minutes": git.get_rate_limit().core.reset.minute, "seconds": git.get_rate_limit().core.reset.second}
    if core_limit > 0 and search_limit > 0:
        try:
            YimMenu_Lua = git.get_organization("YimMenu-Lua")
            for repo in YimMenu_Lua.get_repos(sort="pushed", direction="desc"):
                if str(repo.name).lower() not in exclude_repos:
                    repos.append(repo)
            return repos, core_limit, search_limit, reset_time
        except RateLimitExceededException:
            return [], core_limit, search_limit, reset_time
    else:
        return [], core_limit, search_limit, reset_time


def get_installed_scripts(scripts_path):
    Luas = []
    if os.path.exists(scripts_path):
        dirs = next(os.walk(scripts_path))[1]
        for dir in dirs:
            if dir not in ("includes", "disabled") and not str(dir).startswith("."):
                Luas.append(dir)
    return Luas
