#pragma once
#include "Processor.h"

class PixelEditor final : public juce::AudioProcessorEditor,private juce::Timer,public juce::FileDragAndDropTarget
{
public:
    explicit PixelEditor(CompanionProcessor&);
    ~PixelEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    std::function<void()> onUtility;
    std::function<void()> onRecorderSettings;
    void showHome(){home=true;repaint();}
    void showPerformance(){home=false;repaint();}
    void showHardwarePage(int p){processor.selectTrack(6);processor.setPage(p);home=false;repaint();}
    static juce::Rectangle<int> trackBounds(int t) {return {12+(t%4)*117,48+(t/4)*104,105,94};}
    void closeHostedEditors();
private:
    juce::Point<int> logical(juce::Point<float>) const;
    void drawScreen(juce::Graphics&);
    void drawCurve(juce::Graphics&);
    void drawAddress(juce::Graphics&,int slot,int x,int y);
    void timerCallback() override;
    void choosePatch(bool save);
    void utility();
    void transportSettings();
    void recorderSettings();
    void chooseRecordingFolder();
    void trackMenu(int);
    void modAssignMenu(int);
    void lfoAssignMenu(int);
    void slotMenu(int);
    void choosePlugin(int,juce::File folder = {});
    void loadFile(int,const juce::File&,int targetTrack=-1);
    void assignMenu(int);
    void openEditor(int);
    int tabCount() const;
    int knobAt(juce::Point<int>) const;
    int pointAt(juce::Point<int>) const;
    juce::PopupMenu::Options menuOptions();
    CompanionProcessor& processor;
    juce::Image screen {juce::Image::RGB,480,300,true};
    juce::Rectangle<float> viewport;
    bool home=true,noteHeld=false;
    int dragSlot=-1,dragPoint=-1,noteChannel=1;
    float dragValue=0;
    juce::Point<int> dragOrigin;
    juce::String toast;
    juce::File lastPluginFolder;
    juce::File recordingFolder {juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("DTX Recordings")};
    int toastTicks=0;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::unique_ptr<juce::DocumentWindow>> pluginWindows;
    juce::Component::SafePointer<juce::DialogWindow> utilityWindow;
    juce::Component::SafePointer<juce::DialogWindow> transportWindow;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PixelEditor)
};
