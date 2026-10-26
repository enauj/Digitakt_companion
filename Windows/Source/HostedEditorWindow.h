#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// A separate, owned native window. Oversized fixed plugin editors can be scrolled.
class HostedEditorWindow final : public juce::DocumentWindow, private juce::ComponentListener
{
public:
    HostedEditorWindow(std::shared_ptr<juce::AudioProcessor>, juce::Component* owner);
    ~HostedEditorWindow() override;
    void closeButtonPressed() override { setVisible(false); }
    juce::AudioProcessorEditor* pluginEditor() const { return editor; }
private:
    void fitEditor();
    void componentMovedOrResized(juce::Component&, bool, bool resized) override;
    std::shared_ptr<juce::AudioProcessor> plugin;
    juce::Viewport view;
    juce::AudioProcessorEditor* editor = nullptr;
    juce::Component::SafePointer<juce::Component> ownerComponent;
    bool fitting = false;
};
