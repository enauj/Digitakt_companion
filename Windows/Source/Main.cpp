#include "MainComponent.h"

class CompanionApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Digitakt Companion"; }
    const juce::String getApplicationVersion() override { return "0.9.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String&) override { window = std::make_unique<Window>(getApplicationName()); }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override {}
private:
    class Window final : public juce::DocumentWindow
    {
    public:
        explicit Window(const juce::String& name)
            : DocumentWindow(name, juce::Colour(0xff101110), allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, false);
            setResizeLimits(720, 450, 1920, 1200);
            getConstrainer()->setFixedAspectRatio(1.6);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION(CompanionApplication)
