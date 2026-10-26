#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <clap/clap.h>
#include <array>

struct HostedDescriptor
{
    juce::String name, format, path, id;
    juce::PluginDescription vst;
    bool instrument = false;
    juce::var toJson() const;
    static HostedDescriptor fromJson(const juce::var&);
};

class ClapProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    static std::vector<HostedDescriptor> discover(const juce::File&);
    explicit ClapProcessor(const HostedDescriptor&);
    ~ClapProcessor() override;
    const juce::String getName() const override { return descriptor.name; }
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }
    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::String parameterID(int) const;
    const clap_plugin_t* nativePlugin() const { return plugin; }
    const clap_plugin_gui_t* nativeGui() const { return gui; }
    std::atomic<int> requestedWidth {0}, requestedHeight {0};
private:
    struct Module;
    class Parameter;
    struct Event { alignas(8) std::array<std::byte, 128> bytes {}; };
    void timerCallback() override;
    void activate();
    void deactivate();
    void configurePorts();
    void flushParameters();
    void addEvent(const void*, size_t);
    static const void* CLAP_ABI extension(const clap_host_t*, const char*);
    static void CLAP_ABI restart(const clap_host_t*);
    static void CLAP_ABI callback(const clap_host_t*);
    static bool CLAP_ABI outputEvent(const clap_output_events_t*, const clap_event_header_t*);
    std::shared_ptr<Module> module;
    HostedDescriptor descriptor;
    clap_host_t host {};
    const clap_plugin_t* plugin = nullptr;
    const clap_plugin_params_t* params = nullptr;
    const clap_plugin_gui_t* gui = nullptr;
    std::vector<Parameter*> parameters;
    std::vector<juce::AudioBuffer<float>> inputs, outputs;
    std::vector<clap_audio_buffer_t> inputViews, outputViews;
    std::vector<std::array<float*, 64>> inputPointers, outputPointers;
    std::array<Event, 4096> events;
    uint32_t eventCount = 0;
    clap_input_events_t inputEvents {};
    clap_output_events_t outputEvents {};
    juce::CriticalSection lifecycle;
    std::atomic<bool> restartPending {false}, callbackPending {false};
    bool active = false, processing = false, clapNotes = false;
    double rate = 48000.0;
    int maxBlock = 512;
    int64_t steadyTime = 0;
};
