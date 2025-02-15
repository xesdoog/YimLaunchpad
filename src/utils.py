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

from bs4 import BeautifulSoup
from datetime import datetime, timezone
from github import Github, RateLimitExceededException, GithubRetry
from requests_cache import DO_NOT_CACHE, get_cache, install_cache

LAUNCHPAD_PATH = os.path.join(os.getenv("APPDATA"), "YimLaunchpad")
if not os.path.exists(LAUNCHPAD_PATH):
    os.mkdir(LAUNCHPAD_PATH)

WORKDIR = os.path.abspath(os.getcwd())
UPDATE_PATH = os.path.join(LAUNCHPAD_PATH, "update")
YIM_MENU_PATH = os.path.join(os.getenv("APPDATA"), "YimMenu")
YIM_SCRIPTS_PATH = os.path.join(YIM_MENU_PATH, "scripts")
LOG_FILE = os.path.join(LAUNCHPAD_PATH, "yimlaunchpad.log")
LOG_BACKUP = os.path.join(LAUNCHPAD_PATH, "Log Backup")


install_cache(
    cache_control=True,
    urls_expire_after={
        "*.github.com": 360,
        "*": DO_NOT_CACHE,
        "github.com/xesdoog/*": DO_NOT_CACHE,
        "github.com/Mr-X-GTA/*": DO_NOT_CACHE,
    },
)


def executable_path():
    return os.path.dirname(os.path.abspath(sys.argv[0]))


class CustomLogHandler(logging.FileHandler):

    def __init__(
        self,
        filename,
        archive_path=LOG_BACKUP,
        archive_name="backup_%Y%m%d_%H%M%S.log",
        max_bytes=524288,
        **kwargs,
    ):
        self.archive_path = archive_path
        self.archive_name = archive_name
        self.max_bytes = max_bytes

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


class LOGGER:

    def __init__(self):
        self.logger = logging.getLogger("YLP")
        log_handler = CustomLogHandler(LOG_FILE)
        logging.basicConfig(
            encoding="utf-8",
            level=logging.INFO,
            format="[%(asctime)s] [%(levelname)s] [%(name)s] %(message)s",
            datefmt="%H:%M:%S",
            handlers=[log_handler],
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

    def OnStart(self, parent_path, version):
        userOS = platform.system()
        userOSarch = platform.architecture()
        userOSrel = platform.release()
        userOSver = platform.version()
        workDir = parent_path
        exeDir = executable_path()
        logfile = open(LOG_FILE, "a")
        logfile.write("\n--- YimLaunchpad ---\n\n")
        logfile.write(f"    ¤ Version: {version}\n")
        logfile.write(
            f"    ¤ Operating System: {userOS} {userOSrel} x{userOSarch[0][:2]} v{userOSver}\n"
        )
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


def append_short_sha(file) -> str:
    checksum = str(calc_file_checksum(file))[:6]
    file_name = f"{os.path.basename(file)} [{checksum}]"
    return file_name


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
        json.dump(list, f, indent=4)


def save_cfg_item(file, item_name, value):
    config = read_cfg(file)
    with open(file, "w") as f:
        config[item_name] = value
        json.dump(config, f, indent=4)


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


def open_folder(path):
    if not os.path.exists(path):
        os.makedirs(path)
    subprocess.Popen(f"explorer {os.path.abspath(path)}")


def visit_url(url):
    webbrowser.open_new_tab(url)


def is_repo_empty(repo, path=""):
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


def lua_script_needs_update(repo, installed) -> bool:
    if len(installed) > 0 and repo.name in installed:
        local_date = installed[repo.name]["date_modified"]
        last_commit = repo.get_commits()[0]
        remote_date = last_commit.commit.committer.date.replace(tzinfo=timezone.utc)
        return local_date < remote_date

    return False


def get_lua_repos():

    repos = []
    update_available = []
    installed = get_installed_scripts()
    exclude_repos = {
        "example",
        "submission",
        "samurai-scenarios",
        "samurai-animations",
        "yimlls",
    }

    git = Github()
    rate_limit, _ = git.rate_limiting
    try:
        if rate_limit <= 0:
            return [], [], True

        YimMenu_Lua = git.get_organization("YimMenu-Lua")
        for repo in YimMenu_Lua.get_repos(sort="pushed", direction="desc"):
            if str(repo.name).lower() not in exclude_repos:
                repos.append(repo)
                if repo.name in installed and lua_script_needs_update(repo, installed):
                    update_available.append(repo.name)
        return repos, update_available, False

    except RateLimitExceededException:
        return [], [], True


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


def is_script_installed(repo):
    return os.path.exists(os.path.join(YIM_SCRIPTS_PATH, repo.name))


def is_script_disabled(repo):
    return os.path.exists(
        os.path.join(os.path.join(YIM_SCRIPTS_PATH, "disabled"), repo.name)
    )


def run_updater():

    main_file = os.path.join(os.getcwd(), "YimLaunchpad.exe")
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
    cd /d "{os.path.abspath(os.getcwd())}"
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
