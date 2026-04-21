#include "AppSettings.h"

static constexpr const char* kRootTag      = "PolyHostSettings";
static constexpr const char* kMidiDevice   = "midiDevice";
static constexpr const char* kAudioDevice  = "audioDevice";
static constexpr const char* kWindowWidth  = "windowWidth";
static constexpr const char* kWindowHeight = "windowHeight";
static constexpr const char* kAudioDeviceState = "audioDeviceState";

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
int AppSettings::getWindowWidth()  const { return xml->getIntAttribute(kWindowWidth,  1100); }
int AppSettings::getWindowHeight() const { return xml->getIntAttribute(kWindowHeight,  740); }
void AppSettings::setWindowSize(int w, int h) { xml->setAttribute(kWindowWidth, w); xml->setAttribute(kWindowHeight, h); save(); }

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