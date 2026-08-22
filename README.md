# Startup Monitor 64

[![Release](https://img.shields.io/github/v/release/sl2365/Startup-Monitor-64?style=for-the-badge-square&color=orange)](https://github.com/sl2365/Startup-Monitor-64/releases/latest/download/Startup-Monitor-64.rar)
[![Release Date](https://img.shields.io/github/release-date/sl2365/Startup-Monitor-64?style=for-the-badge-square&color=yellow)](https://github.com/sl2365/Startup-Monitor-64/releases)

[![Latest Asset Downloads](https://img.shields.io/github/downloads/sl2365/Startup-Monitor-64/latest/Startup-Monitor-64.rar?style=for-the-badge-square&label=downloads-latest&displayAssetName=false&color=blue)](https://github.com/sl2365/Startup-Monitor-64/releases/latest)
[![Total Downloads](https://img.shields.io/github/downloads/sl2365/Startup-Monitor-64/total?style=for-the-badge-square&label=downloads-total&color=blue)](https://github.com/sl2365/Startup-Monitor-64/releases)

## Overview

This Windows tray application monitors system startup locations, scheduled tasks, and registry entries for changes or new items. It alerts the user to new startup entries, lets the user approve or deny them, and provides options for logging and deletion. All configuration and log files are stored in a dedicated `App` folder within the application's directory, making it fully portable.

The app can be used to monitor any custom location added by the user, so this could serve as an app monitor, checking for realtime changes in folders or registry locations.

For a quicker response, please post bug reports and FR's on the [PortableFreeware](https://www.portablefreeware.com/forums/viewtopic.php?t=26600) forum.

---

## Features

- **Monitors**:
  - User-specified startup folders (e.g. Startup menus).
  - Scheduled tasks (Windows Task Scheduler).
  - Registry locations (stubbed, expandable).
  - Locations can be enabled/disabled on a per-item basis.
- **Allows/Denies**:
  - New entries alert the user with a review GUI.
  - User can allow (approve) or deny (block/delete) new items.
  - Denied items are deleted after confirmation.
  - Manual rule additions. [See warning and instructions.](Allowed_Denied_Rules.md)
- **Persistence**:
  - Baseline snapshot on first run (never updated except on settings reset).
  - Tracks Allowed and Denied items separately for user decisions.
- **Logging**:
  - All actions are logged to `Log.ini`.
- **Tray Menu**:
  - Settings
  - Open App Folder
  - Pause Monitoring
  - Exit

---

## Folder and File Structure

All files are created in:  
`[AppFolder]\App`

| File Name           | Purpose                                                                   |
|---------------------|---------------------------------------------------------------------------|
| Allowed.ini         | User-allowed startup/scheduled task items                                 |
| Denied.ini          | User-denied startup/scheduled task items                                  |
| Log.ini             | Log of all actions (Last 30 days)                                         |
| Settings.ini        | App settings/configuration                                                |
| BaseStartup.ini     | Baseline snapshot of startup items at first run                           |
| BaseTasks.ini       | Baseline snapshot of scheduled tasks at first run                         |
| Locations.ini       | List of monitored locations (folders and registry keys)                   |

---

## Usage

1. **First Run**  
   - Creates the `App` folder and all required files with default values.
   - Takes baseline snapshots of startup folders and scheduled tasks.

2. **Monitoring**  
   - The app polls monitored locations at the interval set in Settings GUI: MonitorTime, default: 3000ms. The longer the time, the less resources required.
   - Finds new items, compares them to baseline and Allowed/Denied lists, reports to user.

3. **Review GUI**  
   - New or changed items open a review dialog with checkboxes:
     - **CHECKED**: Item will be added to Allowed.ini (approved)
     - **UNCHECKED**: Item will be added to Denied.ini (blocked/deleted)
   - Denied items are deleted on clicking Apply.
   - Export List: This exports a full list of items shown in the Review GUI.
   - NOTE: This window is only displayed depending on the setting: Review denied items again. see below.

4. **Tray Menu Functions**
   - Left-click opens settings window
   - Right-click shows context menu:
     - Settings
     - Open App Folder
     - Pause Monitoring
     - Exit

---

## Customising Monitored Locations

---

# Settings GUI – Options & Functionality

The Settings GUI features several tabs that allow for detailed configuration and management of monitored startup items. Use the Settings window which covers all functionality. Just left-click the tray icon to open Settings. Here, you can configure monitoring behaviour, baseline creation, logging preferences, review window size, and manage advanced lists such as Locations, Allowed, and Denied.

---

### **1. Options Tab**

| Option | Description | Values / Range |
|---|---|---|
| **Monitor Interval**           | Set scan interval of Startup locations. | 1000–60000 ms (1–60 sec) |
| **Tasks Scan Interval**        | Set scan interval of Scheduled Tasks. | 10000–3600000 ms (10sec–1hr) |
| **Review Window Width**        | Sets the width of the review window. | 400–1600 pixels |
| **Review Window Height**       | Sets the height of the review window. | 200–900 pixels |
| **Clear Log on Start**         | Create a new log file on each startup. | (On/Off) |
| **Create Persistent Baseline** | Creates a baseline of existing startup items on first run to reduce false alerts. | (On/Off) |
| **Monitor Scheduled Tasks**    | When enabled, scheduled tasks are included in the monitoring process. | (On/Off) |
| **Monitor Registry Locations** | Monitors registry keys for startup programs. Disable for folder monitoring only. | (On/Off) |
| **Review items selected by default** | Detected items checkboxes are auto checked, or not. | (On/Off) |
| **Review denied items again**  | CHECKED: Displays whenever new items detected. UNCHECKED: Only displays if an item isn't in Allowed or Denied list.	| (On/Off) |
| **Theme**                      | Sets the skin theme. | Light, Dark, System |
| **Reset to Defaults**          | Restores all settings to their default values. | Button |
| **Settings Folder**            | Opens application settings folder for manual review or backup. | Button |
| **Pause Monitoring**           | Works in tandem with Tray Menu to pause the motioring of all locations/tasks. | (On/Off) |

**NOTE:** Please be aware that all settings are saved immediately. The only things that aren't are changes to the fields on the Options tab and Reset to Defaults. These require clicking Apply, this will then save and exit. Click Cancel to ignore these changes.

---

### **2. Locations Tab**
**Use Locations Tab** to tailor what areas of the system are monitored. Add, remove, or edit to suit your needs. You can use this to monitor any folder or registry key for changes, other than startup.
 
Manage the startup locations that are monitored for changes. These locations typically include registry keys, file paths, and scheduled tasks.

**How to Use:**

- **List View:** See all locations currently being monitored. Use checkboxes to enable/disable that item. Changes saved immediately.

- **Context Menu:** Right click the list view to access options.
  - **Edit:** Modify an existing location's settings. Right Click a list view item, select Edit, then the edit field appears below. Make your changes and click the Edit button to save.
  - **Remove:** Remove a location if you no longer wish to monitor it. A confirmation message appears at the bottom of the window with OK/Cancel buttons.
  - **Refresh:** Likely not necessary, added to facilitate refreshing the view in case auto-refresh fails.
  - **Add Folder:** Browse to a folder and add it to your monitoring list.
  - **Add Registry Path:** Add a new registry startup location to monitor. Use the "Open RegEdit" from the context menu, then use RegEdit to browse to your location. Copy the desired path into the edit field, then click "Add Path" button to add to the list.
  - **Open RegEdit:** Use this to locate registry locations you want to add as a monitored location.
---

- **Default Locations:**
```
[Folders]
C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Startup\
C:\Users\<Username>\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup

[Registry]
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\RunOnce
HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Run
HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\RunOnce
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders
HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\RunServicesOnce
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\RunServicesOnce
HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\RunServices
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\RunServices
HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer\Run
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer\Run
HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\Winlogon\Userinit
HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\Winlogon\Shell
HKEY_CURRENT_USER\Software\Microsoft\Windows NT\CurrentVersion\Windows
HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\Session Manager
```
---

### **3. Allowed Tab**

**Purpose:**

Manage the list of startup items that are explicitly allowed. Items in this list will not trigger alerts or warnings.

**How to Use:**
- **Context Menu**
  - **Remove:** Remove items that are no longer trusted. Select item in the list view, click "Remove". Review the confirmation at the bottom of the window and select OK/Cancel as desired.
  - **Refresh:** Refreshes list. Unlikely to be required as changes are applied immediately, but, just in case!
  - **CopyPath:** Copies the full path of selected item.
- **Use Case:**  
  For trusted programs you know and want to keep running at startup.
- **Manual rule additions:**
  You can now add your own rules to both the allowed/Denied tabs, but please read this first: [Warning and instructions.](Allowed_Denied_Rules.md)

---

### **4. Denied Tab**

**Purpose:**

Manage the list of startup items that are explicitly denied. Any item matching entries in this list will trigger alerts and may be blocked.

**How to Use:**  
- **Context Menu**
  - **Remove:** Remove entries if a program is no longer considered a threat. Select item in the list view, click "Remove". Review the confirmation at the bottom of the window and select OK/Cancel as desired.
  - **Refresh:** Refreshes list. Unlikely to be required as changes are applied immediately, again, just in case!
  - **CopyPath:** Copies the full path of selected item.
- **Use Case:**  
  For known threats, unwanted software, or items you want to prevent from running at startup.
- **Manual rule additions:**
  You can now add your own rules to both the allowed/Denied tabs, but please read this first: [Warning and instructions.](Allowed_Denied_Rules.md)

---

### **5. Other Tabs**

Additional tabs for advanced settings, such as:
- **Baseline:**  View and manage the persistent baseline of startup items.  
  - **Use:** See which items were present during initial setup and adjust baseline as needed. Click to view full path and at the same time copy the full path to the clipboard.
- **Log:**  View logging details, such as log file path and retention. Delete existing log to start afresh. You can select and right click to copy or use keyboard shortcuts.

---

## **Managing Tabs Effectively**

- **Use Allowed and Denied Tabs** to fine-tune which startup items are permitted or blocked. For example if a review was accidentally accepted or denied, you can amend here.
- **Review Baseline** periodically to ensure only legitimate items are included.
- **Consult Log Tab** for troubleshooting or audit purposes.

---

## **Typical Usage Flow**

1. **Configure monitoring frequency and targets** in Main Settings.
2. **Review Locations Tab** — enable/disable or add/remove monitored areas.
3. **Populate Allowed Tab** with trusted items to reduce unnecessary alerts.
4. **Populate Denied Tab** with unwanted or suspicious items for proactive blocking.
5. **Check Baseline and Log Tabs** for additional control and troubleshooting.
6. **Save and exit** — settings take effect immediately.

---

## **Notes**

- Invalid values prompt warnings and revert to previous/default settings.
- All changes are saved automatically unless otherwise specified.
- Tip: To reduce repeat notification of items already denied but that continue to reoccur, uncheck the box for `Review denied items again`.

---

For further support or questions, contact me, preferably on the StartupMonitor forum at [PortableFreeware.com](https://www.portablefreeware.com/forums/viewtopic.php?t=26600&start=15).

---

## Building & Running

1. Requires [python-3.10.9-embed-amd64](https://www.python.org/downloads/windows/).
2. Extract Source files and open `build_SM64.bat`.
3. Change this line to show the relative or absolute path to the folder containg python.exe:
   `set "PYTHON_DIR=%TOOLS_DIR%\python\_3.10.9"`
4. Run `build_SM64.bat` to create the exe.

---

## License

MIT

---
