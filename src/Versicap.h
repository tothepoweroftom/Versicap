#pragma once

#include "exporters/Exporter.h"
#include "Project.h"
#include "Settings.h"
#include "Tags.h"

namespace vcp {

class MainWindow;
class AudioEngine;
class PluginManager;
class Render;
class RenderContext;

struct AppMessage : public Message
{

};

struct DisplayObjectMessage : public AppMessage
{
    DisplayObjectMessage (const ValueTree& o) : object (o) { }
    const ValueTree object;
};

class Versicap final
{
public:
    Versicap();
    ~Versicap();

    struct Listener
    {
        Listener() = default;
        virtual ~Listener() = default;

        virtual void displayedObjectChanged() {}

        virtual void projectChanged() {}

        virtual void renderWillStart() { }
        virtual void renderStarted() { }
        virtual void renderWillStop() { }
        virtual void renderStopped() { }
        virtual void renderProgress (double, const String&) { }

        virtual void exportStarted() {}
        virtual void exportFinished() {}
        virtual void exportProgress (double, const String&) {}
    };
    
    //=========================================================================
    Result startExporting();
    void stopExporting();
    
    //=========================================================================
    static File getApplicationDataPath();
    static File getUserDataPath();
    static File getSamplesPath();
    static File getProjectsPath();

    //=========================================================================
    void initialize();
    void shutdown();

    //=========================================================================
    Settings& getSettings();
    void saveSettings();
    
    //=========================================================================
    Project getProject() const;
    bool setProject (const Project& project);
    File getProjectFile() const;
    void setProjectFile (const File&);
    bool saveProject (const File& file);
    bool loadProject (const File& file);    
    bool hasProjectChanged() const;
    
    //=========================================================================
    const ExporterTypeArray& getExporterTypes() const;
    ExporterTypePtr getExporterType (const String& slug) const;
    
    //=========================================================================
    AudioEngine& getAudioEngine();
    AudioThumbnailCache& getAudioThumbnailCache();
    ApplicationCommandManager& getCommandManager();
    AudioDeviceManager& getDeviceManager();
    PluginManager& getPluginManager();
    AudioFormatManager& getAudioFormats();
    MidiKeyboardState& getMidiKeyboardState();
    UndoManager& getUndoManager();

    //=========================================================================
    static MainWindow* getMainWindow();

    //=========================================================================
    AudioThumbnail* createAudioThumbnail (const File& file);

    //=========================================================================
    void loadPlugin (const PluginDescription&, bool clearProjectPlugin = true);
    void closePlugin (bool clearProjectPlugin = true);
    void closePluginWindow();
    void showPluginWindow();

    //=========================================================================
    void saveRenderContext();
    Result startRendering();
    void stopRendering();

    //=========================================================================
    template<class ControllerType> ControllerType* findController() const;

    //=========================================================================
    void addListener (Listener* listener)       { listeners.add (listener); }
    void removeListener (Listener* listener)    { listeners.remove (listener); }

    //=========================================================================
    void post (Message*);

private:
    friend class Application;
    friend struct Impl;

    struct Impl; std::unique_ptr<Impl> impl;
    ListenerList<Listener> listeners;

    //=========================================================================
    void initializeDataPath();
    void initializeExporters();
    void initializeAudioDevice();
    void initializePlugins();

    void launched();
};

}
