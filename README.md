# YimLaunchpad
A launchpad for YimMenu.

![ylp_splash](https://github.com/user-attachments/assets/0acf2233-078a-4cce-a0a7-d7b84d91682b)

###  


> [!WARNING]
> This program was written for educational purposes only. Use at your own risk.

###  

## Features

### YimMemu & Other DLLs

- Download YimMenu.
- Keep it updated: Automatically checks for updates on launch and sends toast notifications. *(you can also manually check for updates at any time)*.
- Enable/Disable YimMenu's debug console.
- Start the game.
- Inject YimMenu into the game's process.
- Auto-Inject YimMenu without having to worry about the perfect injection delay: YimLaunchpad will only auto-inject when the game is in the landing page.
- Add custom DLLs.
- Inject custom DLLs into the game's process.
- Auto-Exit after injecting any DLL.
- Scans modules loaded by the game to determine if YimMenu and FSL are loaded.

### Lua Scripts

- Download Lua scripts from [YimMenu-Lua](https://github.com/YimMenu-Lua).
- Enable/Disable scripts.
- Delete scripts.
- Enable/Disable auto-reloading of Lua scripts.
- Keep your Lua scripts updated: Automatically checks for script updates when fetching repositories and sends toast notifications. *(You can also manually check individual repositories that you have installed)*.

> [!IMPORTANT]
> To benefit from the Lua Scripts features, you have to download the scripts using the launcher. The reason for that is simply because YimLaunchpad organizes your scripts into folders named after the script's GitHub repository. This allows it to track your enabled/disabled scripts, check the last time a script was updated and compare it to the repository, etc...
>
> Loose files and folders named anything other than the script's repository name will not be recognized as installed scripts.

> [!IMPORTANT]
> If you're not authenticated, avoid spamming the Lua Scripts tab *(refreshing multiple times, repeatedly closing and relaunching the app, etc...)*. The GitHub API has a rate limit of 60 request per hour for unauthenticated requests and will put you on a timeout if you exceed that limit. More info on API rate limits can be found [here](https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api?apiVersion=2022-11-28).
>
> If you want to increase the limit to 5000 requests per hour, you have the option to login to GitHub through YimLaunchpad using a device code. No email or password is required.

### Settings

- Update the launchpad.
- Open the launchpad's folder. *(located under `%AppData%\YimLaunchpad`)*.
- Add `YimLaunchpad.exe` and its parent folder to Windows Defender exclusions using Windows Powershell™.
- Enable/Disable `Auto-Exit`.
- Enable/Disable `Auto-Inject`.
- Enable/Disable YimLaunchpad's `Debug Console`.
- Login/logout to/from GitHub.

## TODO

- [ ] Add themes. *meh*
- [x] Add automatic injection. ✅
- [x] Add a button to refresh installed Lua scripts individually. ✅

## Showcase

### Dashboard

![dashboard_x3](https://github.com/user-attachments/assets/f4ed5ecf-3d10-40ad-b86d-85146d071ef3)

### Lua Scripts (Without GitHub Auth)

![lua](https://github.com/user-attachments/assets/866f2dde-b743-43e7-a265-df3dd62ad08d)

### Settings (Without GitHub Auth)

![settings](https://github.com/user-attachments/assets/2c53073f-8424-4bcc-b273-9d17da324d1e)

### Lua Scripts (With GitHub Auth)

![lua_1](https://github.com/user-attachments/assets/32a292e9-54f0-4bff-aeeb-e86b11e70ec5)

### Settings (With GitHub Auth)

![settings_1](https://github.com/user-attachments/assets/95038fea-a22d-4ee4-b955-41b9ebcb6899)

## Licenses

- This program uses Google's Rokkit Regular font, licensed under [SIL Open Font License V1.1](https://openfontlicense.org/open-font-license-official-text/)
- This program uses the free version of FontAwesome v4.7 also licensed under [SIL Open Font License V1.1](https://openfontlicense.org/open-font-license-official-text/)

## Donations

- I don't want your money. If you like the project, give it a star ⭐

## Build It Yourself

If you don't want to use the release executable built by GitHub Actions, you can build YimLaunchpad yourself if you have `Python >= 3.12.x` installed. It takes less than 2 minutes:
- Fork the repo.
- Open a terminal and cd to the repo's directory.
- Install Pyinstaller:

      pip install pyinstaller

- Install Requirements:

      pip install -r requirements.txt 

- Download [UPX](https://github.com/upx/upx/releases) and note down where you saved it.
- Once ready, run this command to build a portable executable:

      pyinstaller "YimLaunchpad.py" --noconfirm --onefile --windowed --name "YimLaunchpad" --clean --icon "./src/assets/img/ylp_icon.ico" --splash "./src/assets/img/ylp_splash.png" --add-data "./src;src/" --add-binary "./src/assets/dll/glfw3.dll;." --add-binary "./src/assets/dll/msvcr110.dll;." --upx-dir "PATH_TO_WHERE_YOU_SAVED_UPX" --upx-exclude "vcruntime140.dll" --hidden-import=winsdk --hidden-import=pymem

- Once the process completes, there will be a new folder in the project's root directory named `dist`. The executable can be found there.
