#include "AppSettings.h"

static constexpr const char* kRootTag      = "PolyHostSettings";
static constexpr const char* kMidiDevice   = "midiDevice";
static constexpr const char* kAudioDevice  = "audioDevice";
static constexpr const char* kAudioDeviceState = "audioDeviceState";
static constexpr const char* kAutoSaveAfterPluginRepair = "autoSaveAfterPluginRepair";
static constexpr const char* kPluginScanFolders         = "PluginScanFolders";
static constexpr const char* kFolderTag                 = "Folder";
static constexpr const char* kPathAttribute             = "path";
static constexpr const char* kEnabledMidiDevices        = "EnabledMidiDevices";
static constexpr const char* kDeviceTag                 = "Device";
static constexpr const char* kIdentifierAttribute       = "identifier";
static constexpr const char* kNameAttribute             = "name";
static constexpr auto kLastPresetPath   = "lastPresetPath";
static constexpr auto kRecentPresets    = "recentPresets";
static constexpr auto kWindowX = "windowX";
static constexpr auto kWindowY = "windowY";

juce::String AppSettings::getLastPresetPath() const
{
    return xml->getStringAttribute(kLastPresetPath);
}

void AppSettings::setLastPresetPath(const juce::String& path)
{
    xml->setAttribute(kLastPresetPath, path);
    save();
}

juce::File AppSettings::getSettingsFile()
{
    auto settingsDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                       .getParentDirectory().getChildFile("Settings");
    settingsDir.createDirectory();
    return settingsDir.getChildFile("polyhost.xml");
}

juce::File AppSettings::getPresetsDirectory()
{
    auto presetsDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                      .getParentDirectory().getChildFile("Presets");
    presetsDir.createDirectory();
    return presetsDir;
}

AppSettings::AppSettings() { xml = std::make_unique<juce::XmlElement>(kRootTag); load(); }

void AppSettings::load()
{
    auto file = getSettingsFile();
    if (file.existsAsFile())
        if (auto loaded = juce::XmlDocument::parse(file))
            if (loaded->hasTagName(kRootTag))
                xml = std::move(loaded);
}

void AppSettings::save() { xml->writeTo(getSettingsFile(), {}); }
juce::String AppSettings::getMidiDeviceName() const { return xml->getStringAttribute(kMidiDevice, ""); }
void AppSettings::setMidiDeviceName(const juce::String& name) { xml->setAttribute(kMidiDevice, name); save(); }
juce::String AppSettings::getAudioDeviceName() const { return xml->getStringAttribute(kAudioDevice, ""); }
void AppSettings::setAudioDeviceName(const juce::String& name) { xml->setAttribute(kAudioDevice, name); save(); }

juce::String AppSettings::getAudioDeviceState() const
{
    return xml->getStringAttribute(kAudioDeviceState);
}

void AppSettings::setAudioDeviceState(const juce::String& xmlText)
{
    xml->setAttribute(kAudioDeviceState, xmlText);
    save();
}

juce::File AppSettings::getAppDirectory()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory();
}

juce::String AppSettings::makePathPortable(const juce::File& file)
{
    auto appDir = getAppDirectory();
    auto relative = file.getRelativePathFrom(appDir);

    if (!relative.isEmpty() && !juce::File::isAbsolutePath(relative))
        return relative;

    return {};
}

juce::File AppSettings::resolvePortablePath(const juce::String& path)
{
    if (path.isEmpty())
        return {};

    if (juce::File::isAbsolutePath(path))
        return juce::File(path);

    return getAppDirectory().getChildFile(path);
}

juce::String AppSettings::getDriveFlexiblePath(const juce::File& file)
{
   #if JUCE_WINDOWS
    auto fullPath = file.getFullPathName();

    if (fullPath.length() > 2 && fullPath[1] == ':')
        return fullPath.substring(2); // strips "D:"

    return fullPath;
   #else
    juce::ignoreUnused(file);
    return {};
   #endif
}

juce::File AppSettings::resolvePluginPath(const juce::String& absolutePath,
                                          const juce::String& relativePath,
                                          const juce::String& driveFlexiblePath)
{
    if (relativePath.isNotEmpty())
    {
        auto relativeFile = resolvePortablePath(relativePath);
        if (relativeFile.existsAsFile())
            return relativeFile;
    }

    if (absolutePath.isNotEmpty())
    {
        juce::File absoluteFile(absolutePath);
        if (absoluteFile.existsAsFile())
            return absoluteFile;
    }

   #if JUCE_WINDOWS
    if (driveFlexiblePath.isNotEmpty())
    {
        for (char drive = 'C'; drive <= 'Z'; ++drive)
        {
            juce::String testPath;
            testPath << juce::String::charToString(drive) << ":" << driveFlexiblePath;

            juce::File candidate(testPath);
            if (candidate.existsAsFile())
                return candidate;
        }
    }
   #else
    juce::ignoreUnused(driveFlexiblePath);
   #endif

    return {};
}

bool AppSettings::getAutoSaveAfterPluginRepair() const
{
    return xml->getBoolAttribute(kAutoSaveAfterPluginRepair, false);
}

void AppSettings::setAutoSaveAfterPluginRepair(bool shouldAutoSave)
{
    xml->setAttribute(kAutoSaveAfterPluginRepair, shouldAutoSave);
    save();
}

juce::StringArray AppSettings::getPluginScanFolders() const
{
    juce::StringArray folders;

    if (auto* foldersXml = xml->getChildByName(kPluginScanFolders))
    {
        for (auto* child : foldersXml->getChildIterator())
        {
            if (child->hasTagName(kFolderTag))
            {
                auto path = child->getStringAttribute(kPathAttribute).trim();
                if (path.isNotEmpty())
                    folders.addIfNotAlreadyThere(path);
            }
        }
    }

    return folders;
}

void AppSettings::setPluginScanFolders(const juce::StringArray& folders)
{
    xml->removeChildElement(xml->getChildByName(kPluginScanFolders), true);

    auto foldersXml = std::make_unique<juce::XmlElement>(kPluginScanFolders);

    for (auto& folder : folders)
    {
        auto trimmed = folder.trim();
        if (trimmed.isEmpty())
            continue;

        auto* child = foldersXml->createNewChildElement(kFolderTag);
        child->setAttribute(kPathAttribute, trimmed);
    }

    xml->addChildElement(foldersXml.release());
    save();
}

void AppSettings::addPluginScanFolder(const juce::String& folderPath)
{
    auto folders = getPluginScanFolders();
    auto trimmed = folderPath.trim();

    if (trimmed.isEmpty())
        return;

    folders.addIfNotAlreadyThere(trimmed);
    setPluginScanFolders(folders);
}

void AppSettings::removePluginScanFolder(const juce::String& folderPath)
{
    auto folders = getPluginScanFolders();
    folders.removeString(folderPath.trim(), true);
    setPluginScanFolders(folders);
}

juce::StringArray AppSettings::getEnabledMidiDeviceIdentifiers() const
{
    juce::StringArray identifiers;

    if (auto* devicesXml = xml->getChildByName(kEnabledMidiDevices))
    {
        for (auto* child : devicesXml->getChildIterator())
        {
            if (child->hasTagName(kDeviceTag))
            {
                auto identifier = child->getStringAttribute(kIdentifierAttribute).trim();
                if (identifier.isNotEmpty())
                    identifiers.addIfNotAlreadyThere(identifier);
            }
        }
    }

    return identifiers;
}

void AppSettings::setEnabledMidiDeviceIdentifiers(const juce::StringArray& identifiers)
{
    xml->removeChildElement(xml->getChildByName(kEnabledMidiDevices), true);

    auto devicesXml = std::make_unique<juce::XmlElement>(kEnabledMidiDevices);

    for (auto& identifier : identifiers)
    {
        auto trimmed = identifier.trim();
        if (trimmed.isEmpty())
            continue;

        auto* child = devicesXml->createNewChildElement(kDeviceTag);
        child->setAttribute(kIdentifierAttribute, trimmed);
    }

    xml->addChildElement(devicesXml.release());
    save();
}

juce::StringArray AppSettings::getRecentPresetPaths() const
{
    juce::StringArray paths;

    if (auto* recentXml = xml->getChildByName(kRecentPresets))
    {
        for (auto* presetXml : recentXml->getChildIterator())
        {
            if (presetXml->hasTagName("Preset"))
            {
                auto path = presetXml->getStringAttribute("path").trim();
                if (path.isNotEmpty())
                    paths.add(path);
            }
        }
    }

    return paths;
}

void AppSettings::addRecentPresetPath(const juce::String& path)
{
    auto trimmedPath = path.trim();

    if (trimmedPath.isEmpty())
        return;

    auto previous = getRecentPresetPaths();
    previous.removeString(trimmedPath);
    previous.insert(0, trimmedPath);

    while (previous.size() > 10)
        previous.remove(previous.size() - 1);

    if (auto* existing = xml->getChildByName(kRecentPresets))
        xml->removeChildElement(existing, true);

    auto* recentXml = xml->createNewChildElement(kRecentPresets);

    for (auto& recentPath : previous)
    {
        auto* presetXml = recentXml->createNewChildElement("Preset");
        presetXml->setAttribute("path", recentPath);
    }

    save();
}

void AppSettings::clearRecentPresetPaths()
{
    if (auto* existing = xml->getChildByName(kRecentPresets))
        xml->removeChildElement(existing, true);

    save();
}

void AppSettings::removeMissingRecentPresetPaths()
{
    auto recentPaths = getRecentPresetPaths();
    juce::StringArray validPaths;

    for (auto& path : recentPaths)
    {
        juce::File file(path);
        if (file.existsAsFile())
            validPaths.add(path);
    }

    if (auto* existing = xml->getChildByName(kRecentPresets))
        xml->removeChildElement(existing, true);

    if (!validPaths.isEmpty())
    {
        auto* recentXml = xml->createNewChildElement(kRecentPresets);

        for (auto& validPath : validPaths)
        {
            auto* presetXml = recentXml->createNewChildElement("Preset");
            presetXml->setAttribute("path", validPath);
        }
    }

    save();
}

int AppSettings::getWindowX() const
{
    return xml->getIntAttribute(kWindowX, -1);
}

int AppSettings::getWindowY() const
{
    return xml->getIntAttribute(kWindowY, -1);
}

void AppSettings::setWindowPosition(int x, int y)
{
    xml->setAttribute(kWindowX, x);
    xml->setAttribute(kWindowY, y);
    save();
}

