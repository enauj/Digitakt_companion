#include "ClapHost.h"
#include <map>
#include <cstring>
#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace { thread_local bool clapAudioThread = false;
struct AudioScope { bool before = clapAudioThread; AudioScope() { clapAudioThread = true; } ~AudioScope() { clapAudioThread = before; } };
}

juce::var HostedDescriptor::toJson() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty("name", name); o->setProperty("format", format); o->setProperty("path", path);
    o->setProperty("id", id); o->setProperty("instrument", instrument);
    if (format == "VST3") if (auto xml = vst.createXml()) o->setProperty("vst", xml->toString());
    return juce::var(o);
}
HostedDescriptor HostedDescriptor::fromJson(const juce::var& v)
{
    HostedDescriptor d;
    d.name = v.getProperty("name", {}).toString(); d.format = v.getProperty("format", {}).toString();
    d.path = v.getProperty("path", {}).toString(); d.id = v.getProperty("id", {}).toString();
    d.instrument = (bool)v.getProperty("instrument", false);
    if (auto xml = juce::parseXML(v.getProperty("vst", {}).toString())) d.vst.loadFromXml(*xml);
    return d;
}

struct ClapProcessor::Module
{
    juce::DynamicLibrary library;
    const clap_plugin_entry_t* entry = nullptr;
    const clap_plugin_factory_t* factory = nullptr;
    explicit Module(const juce::File& file)
    {
        if (!library.open(file.getFullPathName())) throw std::runtime_error("Cannot open CLAP library (Windows x64 required)");
        entry = reinterpret_cast<const clap_plugin_entry_t*>(library.getFunction("clap_entry"));
        if (entry == nullptr || !clap_version_is_compatible(entry->clap_version)) throw std::runtime_error("Invalid or incompatible CLAP entry");
        if (!entry->init(file.getFullPathName().toRawUTF8())) { entry = nullptr; throw std::runtime_error("CLAP initialization failed"); }
        factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        if (factory == nullptr) { entry->deinit(); entry = nullptr; throw std::runtime_error("CLAP plugin factory missing"); }
    }
    ~Module() { if (entry) entry->deinit(); }
    static std::shared_ptr<Module> open(const juce::File& f)
    {
        static std::map<juce::String, std::weak_ptr<Module>> cache;
        auto& item = cache[f.getFullPathName()];
        if (auto result = item.lock()) return result;
        auto result = std::make_shared<Module>(f); item = result; return result;
    }
};

std::vector<HostedDescriptor> ClapProcessor::discover(const juce::File& f)
{
    auto lib = Module::open(f);
    std::vector<HostedDescriptor> result;
    auto count = juce::jmin(4096u, lib->factory->get_plugin_count(lib->factory));
    for (uint32_t i = 0; i < count; ++i)
    {
        auto* info = lib->factory->get_plugin_descriptor(lib->factory, i);
        if (info == nullptr || info->id == nullptr || info->name == nullptr) continue;
        HostedDescriptor d; d.name = info->name; d.format = "CLAP"; d.path = f.getFullPathName(); d.id = info->id;
        if (info->features) for (auto p = info->features; *p; ++p) if (std::strcmp(*p, CLAP_PLUGIN_FEATURE_INSTRUMENT) == 0) d.instrument = true;
        result.push_back(d);
    }
    return result;
}

class ClapProcessor::Parameter final : public juce::AudioProcessorParameterWithID
{
public:
    explicit Parameter(ClapProcessor& hostProcessor,clap_param_info_t p, double initial)
        : AudioProcessorParameterWithID(juce::ParameterID(juce::String(p.id), 1), p.name), owner(hostProcessor), info(p), value(initial) {}
    float getValue() const override { return (float)juce::jlimit(0.0, 1.0, (value.load() - info.min_value) / juce::jmax(.000000001, info.max_value - info.min_value)); }
    void setValue(float v) override
    {
        if ((info.flags & CLAP_PARAM_IS_READONLY) != 0) return;
        double plain = info.min_value + juce::jlimit(0.f, 1.f, v) * (info.max_value - info.min_value);
        if ((info.flags & CLAP_PARAM_IS_STEPPED) != 0) plain = std::round(plain);
        value.store(plain); dirty.store(true);
    }
    float getDefaultValue() const override { return (float)((info.default_value - info.min_value) / juce::jmax(.000000001, info.max_value - info.min_value)); }
    juce::String getText(float v, int n) const override
    {
        double plain=info.min_value+v*(info.max_value-info.min_value);if(isDiscrete())plain=std::round(plain);
        char text[256] {};const juce::ScopedLock lock(owner.lifecycle);
        auto result=owner.params&&owner.params->value_to_text&&owner.params->value_to_text(owner.plugin,info.id,plain,text,sizeof(text))?juce::String::fromUTF8(text):juce::String(plain,isDiscrete()?0:3);
        return n>0?result.substring(0,n):result;
    }
    int getNumSteps() const override {return isDiscrete()?(int)juce::jmin(2147483646.,info.max_value-info.min_value+1):juce::AudioProcessorParameter::getNumSteps();}
    bool isDiscrete() const override {return (info.flags&CLAP_PARAM_IS_STEPPED)!=0;}
    float getValueForText(const juce::String& t) const override { return (float)juce::jlimit(0.0, 1.0, (t.getDoubleValue() - info.min_value) / juce::jmax(.000000001, info.max_value - info.min_value)); }
    bool isAutomatable() const override { return (info.flags & CLAP_PARAM_IS_READONLY) == 0; }
    ClapProcessor& owner;
    clap_param_info_t info;
    std::atomic<double> value;
    std::atomic<bool> dirty {false};
};

ClapProcessor::ClapProcessor(const HostedDescriptor& d)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), !d.instrument)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)), descriptor(d)
{
    module = Module::open(juce::File(d.path));
    host = {CLAP_VERSION, this, "DTX Companion", "DTX Companion", "", "0.9.0", extension, restart,
        [](const clap_host_t*) {}, callback};
    plugin = module->factory->create_plugin(module->factory, &host, d.id.toRawUTF8());
    if (plugin == nullptr) throw std::runtime_error("CLAP plugin could not be created");
    if (!plugin->init(plugin)) { plugin->destroy(plugin); plugin = nullptr; throw std::runtime_error("CLAP plugin init failed"); }
    params = static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    gui = static_cast<const clap_plugin_gui_t*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
    if (params)
    {
        auto count = juce::jmin(8192u, params->count(plugin));
        for (uint32_t i = 0; i < count; ++i)
        {
            clap_param_info_t info {}; double v = 0;
            if (!params->get_info(plugin, i, &info)) continue;
            if (!params->get_value(plugin, info.id, &v)) v = info.default_value;
            auto* p = new Parameter(*this,info, v); parameters.push_back(p); addParameter(p);
        }
    }
    if (auto* ports = static_cast<const clap_plugin_note_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS)))
    {
        clap_note_port_info_t p {};
        if (ports->count(plugin, true) && ports->get(plugin, 0, true, &p)) clapNotes = (p.preferred_dialect & CLAP_NOTE_DIALECT_CLAP) != 0 || (p.supported_dialects & CLAP_NOTE_DIALECT_MIDI) == 0;
    }
    inputEvents = {this, [](const clap_input_events_t* e) -> uint32_t { return static_cast<ClapProcessor*>(e->ctx)->eventCount; },
        [](const clap_input_events_t* e, uint32_t i) -> const clap_event_header_t* { auto* self = static_cast<ClapProcessor*>(e->ctx); return i < self->eventCount ? reinterpret_cast<const clap_event_header_t*>(self->events[i].bytes.data()) : nullptr; }};
    outputEvents = {this, outputEvent};
    startTimerHz(30);
}

ClapProcessor::~ClapProcessor()
{
    stopTimer();
    const juce::ScopedLock lock(lifecycle);
    deactivate();
    if (plugin) plugin->destroy(plugin);
}
void ClapProcessor::configurePorts()
{
    inputs.clear(); outputs.clear(); inputViews.clear(); outputViews.clear();
    auto* ports = static_cast<const clap_plugin_audio_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
    if (!ports) return;
    for (bool isInput : {true, false})
    {
        auto count = ports->count(plugin, isInput);
        if (count > 16) throw std::runtime_error("CLAP plugin has more than 16 audio ports");
        auto& buffers = isInput ? inputs : outputs;
        auto& views = isInput ? inputViews : outputViews;
        auto& pointers = isInput ? inputPointers : outputPointers;
        buffers.resize(count); views.resize(count); pointers.resize(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            clap_audio_port_info_t info {};
            if (!ports->get(plugin, i, isInput, &info) || info.channel_count == 0 || info.channel_count > 64) throw std::runtime_error("Unsupported CLAP audio port");
            buffers[i].setSize((int)info.channel_count, maxBlock);
            for (uint32_t ch = 0; ch < info.channel_count; ++ch) pointers[i][ch] = buffers[i].getWritePointer((int)ch);
            views[i] = {}; views[i].data32 = pointers[i].data(); views[i].channel_count = info.channel_count;
        }
    }
}
void ClapProcessor::activate()
{
    deactivate(); configurePorts();
    active = plugin->activate(plugin, rate, 1, (uint32_t)maxBlock);
    if (!active) throw std::runtime_error("CLAP activation failed");
    if (auto* latency = static_cast<const clap_plugin_latency_t*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY))) setLatencySamples((int)latency->get(plugin));
}
void ClapProcessor::deactivate()
{
    AudioScope audio;
    if (processing) { plugin->stop_processing(plugin); processing = false; }
    if (active) { plugin->deactivate(plugin); active = false; }
}
void ClapProcessor::prepareToPlay(double r, int frames)
{
    const juce::ScopedLock lock(lifecycle);
    rate = r; maxBlock = juce::jmax(32, frames);
    if (juce::MessageManager::getInstance()->isThisTheMessageThread()) activate();
    else restartPending.store(true);
}
void ClapProcessor::releaseResources()
{
    const juce::ScopedLock lock(lifecycle);
    if (juce::MessageManager::getInstance()->isThisTheMessageThread()) deactivate();
}
void ClapProcessor::addEvent(const void* event, size_t size)
{
    if (eventCount < events.size() && size <= events[0].bytes.size()) std::memcpy(events[eventCount++].bytes.data(), event, size);
}
void ClapProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const juce::CriticalSection::ScopedTryLockType lock(lifecycle);
    AudioScope audio;
    if (!lock.isLocked() || !active || buffer.getNumSamples() > maxBlock) { buffer.clear(); return; }
    if (!processing) processing = plugin->start_processing(plugin);
    if (!processing) { buffer.clear(); return; }
    eventCount = 0;
    for (auto* p : parameters)
    {
        if (!p->dirty.exchange(false)) continue;
        clap_event_param_value_t event {}; event.header = {sizeof(event), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
        event.param_id = p->info.id; event.cookie = p->info.cookie; event.note_id = -1; event.port_index = -1; event.channel = -1; event.key = -1; event.value = p->value.load();
        addEvent(&event, sizeof(event));
    }
    for (const auto meta : midi)
    {
        auto m = meta.getMessage(); uint32_t t = (uint32_t)juce::jlimit(0, buffer.getNumSamples() - 1, meta.samplePosition);
        if (clapNotes && (m.isNoteOn() || m.isNoteOff() || m.isAllNotesOff() || m.isAllSoundOff()))
        {
            clap_event_note_t event {}; event.header = {sizeof(event), t, CLAP_CORE_EVENT_SPACE_ID, (uint16_t)(m.isNoteOn() ? CLAP_EVENT_NOTE_ON : m.isNoteOff() ? CLAP_EVENT_NOTE_OFF : CLAP_EVENT_NOTE_CHOKE), 0};
            event.note_id = -1; event.port_index = 0; event.channel = (int16_t)(m.getChannel() - 1); event.key = (int16_t)((m.isNoteOn() || m.isNoteOff()) ? m.getNoteNumber() : -1); event.velocity = (m.isNoteOn() || m.isNoteOff()) ? m.getFloatVelocity() : 0;
            addEvent(&event, sizeof(event));
        }
        else if (meta.numBytes <= 3)
        {
            clap_event_midi_t event {}; event.header = {sizeof(event), t, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0};
            std::memcpy(event.data, meta.data, (size_t)meta.numBytes); addEvent(&event, sizeof(event));
        }
    }
    for (size_t port = 0; port < inputs.size(); ++port)
    {
        auto& input = inputs[port]; input.clear();
        if (port == 0) for (int c = 0; c < input.getNumChannels(); ++c)
            input.copyFrom(c, 0, buffer, juce::jmin(c, buffer.getNumChannels() - 1), 0, buffer.getNumSamples());
    }
    // Native plugins write through cached channel pointers, so invalidate JUCE's
    // cleared-buffer hint before copying their results back to the host buffer.
    for (auto& output : outputs) { output.clear(); output.setNotClear(); }
    clap_event_transport_t transport {}; transport.header = {sizeof(transport), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0};
    if (auto* playHead = getPlayHead()) if (auto position = playHead->getPosition())
    {
        if (auto bpm = position->getBpm()) { transport.flags |= CLAP_TRANSPORT_HAS_TEMPO; transport.tempo = *bpm; }
        if (auto beat = position->getPpqPosition()) { transport.flags |= CLAP_TRANSPORT_HAS_BEATS_TIMELINE; transport.song_pos_beats = (clap_beattime)std::llround(*beat * CLAP_BEATTIME_FACTOR); }
        if (position->getIsPlaying()) transport.flags |= CLAP_TRANSPORT_IS_PLAYING;
        if (auto signature = position->getTimeSignature()) { transport.flags |= CLAP_TRANSPORT_HAS_TIME_SIGNATURE; transport.tsig_num = (uint16_t)signature->numerator; transport.tsig_denom = (uint16_t)signature->denominator; }
    }
    clap_process_t process {steadyTime, (uint32_t)buffer.getNumSamples(), &transport,
        inputViews.data(), outputViews.data(), (uint32_t)inputs.size(), (uint32_t)outputs.size(), &inputEvents, &outputEvents};
    auto result = plugin->process(plugin, &process);
    steadyTime += buffer.getNumSamples(); buffer.clear();
    if (result != CLAP_PROCESS_ERROR && !outputs.empty()) for (int c = 0; c < buffer.getNumChannels(); ++c)
        buffer.copyFrom(c, 0, outputs[0], juce::jmin(c, outputs[0].getNumChannels() - 1), 0, buffer.getNumSamples());
}
bool CLAP_ABI ClapProcessor::outputEvent(const clap_output_events_t* e, const clap_event_header_t* event)
{
    auto& self = *static_cast<ClapProcessor*>(e->ctx);
    if (event->space_id == CLAP_CORE_EVENT_SPACE_ID && event->type == CLAP_EVENT_PARAM_VALUE && event->size >= sizeof(clap_event_param_value_t))
    {
        auto& value = *reinterpret_cast<const clap_event_param_value_t*>(event);
        for (auto* p : self.parameters) if (p->info.id == value.param_id && !p->dirty.load()) p->value.store(value.value);
    }
    return true;
}
const void* CLAP_ABI ClapProcessor::extension(const clap_host_t*, const char* id)
{
    static const clap_host_thread_check_t threads {
        [](const clap_host_t*) { return juce::MessageManager::getInstance()->isThisTheMessageThread(); },
        [](const clap_host_t*) { return clapAudioThread; }};
    static const clap_host_params_t parameters {
        [](const clap_host_t* h, clap_param_rescan_flags flags) { if ((flags & CLAP_PARAM_RESCAN_ALL) != 0) restart(h); },
        [](const clap_host_t*, clap_id, clap_param_clear_flags) {}, [](const clap_host_t* h) { callback(h); }};
    static const clap_host_gui_t guiHost {
        [](const clap_host_t*) {},
        [](const clap_host_t* h, uint32_t w, uint32_t height) { auto* self = static_cast<ClapProcessor*>(h->host_data); if (w < 64 || height < 64 || w > 4096 || height > 4096) return false; self->requestedWidth.store((int)w); self->requestedHeight.store((int)height); return true; },
        [](const clap_host_t*) { return false; }, [](const clap_host_t*) { return false; }, [](const clap_host_t*, bool) {}};
    if (std::strcmp(id, CLAP_EXT_THREAD_CHECK) == 0) return &threads;
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &parameters;
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) return &guiHost;
    return nullptr;
}
void CLAP_ABI ClapProcessor::restart(const clap_host_t* h) { static_cast<ClapProcessor*>(h->host_data)->restartPending.store(true); }
void CLAP_ABI ClapProcessor::callback(const clap_host_t* h) { static_cast<ClapProcessor*>(h->host_data)->callbackPending.store(true); }
void ClapProcessor::timerCallback()
{
    const juce::ScopedLock lock(lifecycle);
    if (restartPending.exchange(false)) { try { activate(); } catch (...) { active = false; } }
    if (callbackPending.exchange(false)) plugin->on_main_thread(plugin);
    flushParameters();
    if (params) for (auto* p : parameters)
    {
        double v = 0; if (!p->dirty.load() && params->get_value(plugin, p->info.id, &v)) p->value.store(v);
    }
}
void ClapProcessor::flushParameters()
{
    if (!params) return;
    eventCount = 0;
    for (auto* p : parameters) if (p->dirty.exchange(false))
    {
        clap_event_param_value_t event {}; event.header = {sizeof(event), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
        event.param_id=p->info.id; event.cookie=p->info.cookie; event.note_id=-1; event.port_index=-1; event.channel=-1; event.key=-1; event.value=p->value.load(); addEvent(&event,sizeof(event));
    }
    if (eventCount > 0) { AudioScope audio; params->flush(plugin,&inputEvents,&outputEvents); }
}
juce::String ClapProcessor::parameterID(int i) const { return juce::isPositiveAndBelow(i, (int)parameters.size()) ? parameters[(size_t)i]->paramID : juce::String(); }
void ClapProcessor::getStateInformation(juce::MemoryBlock& data)
{
    const juce::ScopedLock lock(lifecycle); data.reset();
    flushParameters();
    if (auto* state = static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin, CLAP_EXT_STATE)))
    {
        clap_ostream_t stream {&data, [](const clap_ostream_t* s, const void* bytes, uint64_t count) -> int64_t {
            if (count > 64 * 1024 * 1024 || static_cast<juce::MemoryBlock*>(s->ctx)->getSize() + count > 64 * 1024 * 1024) return -1;
            static_cast<juce::MemoryBlock*>(s->ctx)->append(bytes, (size_t)count); return (int64_t)count; }};
        if (!state->save(plugin, &stream)) data.reset();
    }
}
void ClapProcessor::setStateInformation(const void* bytes, int size)
{
    const juce::ScopedLock lock(lifecycle);
    if (auto* state = static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin, CLAP_EXT_STATE)))
    {
        juce::MemoryInputStream source(bytes, (size_t)juce::jmax(0, size), false);
        clap_istream_t stream {&source, [](const clap_istream_t* s, void* target, uint64_t count) -> int64_t { return static_cast<juce::MemoryInputStream*>(s->ctx)->read(target, (int)juce::jmin(count, (uint64_t)INT_MAX)); }};
        state->load(plugin, &stream);
        if (params) for (auto* p : parameters) { double value = 0; if (params->get_value(plugin,p->info.id,&value)) { p->value.store(value); p->dirty.store(false); } }
    }
}

namespace
{
class ClapEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ClapEditor(ClapProcessor& p) : AudioProcessorEditor(p), owner(p)
    {
        auto* gui = owner.nativeGui();
        created = gui->create(owner.nativePlugin(), CLAP_WINDOW_API_WIN32, false);
        if (!created)
        {
            fallback = std::make_unique<juce::GenericAudioProcessorEditor>(owner);
            addAndMakeVisible(*fallback);setSize(fallback->getWidth(),fallback->getHeight());return;
        }
        addAndMakeVisible(container);
        uint32_t w = 640, h = 480;
        if (created) gui->get_size(owner.nativePlugin(), &w, &h);
        setSize(juce::jlimit(64, 4096, (int)w), juce::jlimit(64, 4096, (int)h));
        startTimerHz(30);
    }
    ~ClapEditor() override
    {
        stopTimer();
        if (created) { owner.nativeGui()->hide(owner.nativePlugin()); owner.nativeGui()->destroy(owner.nativePlugin()); }
        container.setHWND(nullptr);
    }
    void resized() override
    {
        container.setBounds(getLocalBounds());
        if(fallback)fallback->setBounds(getLocalBounds());
    }
    void parentHierarchyChanged() override { attach(); }
private:
    void attach()
    {
        if (created && !attached && getPeer())
        {
            // CLAP win32 sizes are physical pixels; JUCE component sizes are logical.
            scale=getPeer()->getPlatformScaleFactor();
            auto* gui=owner.nativeGui();gui->set_scale(owner.nativePlugin(),scale);
            uint32_t width=0,height=0;
            if(gui->get_size(owner.nativePlugin(),&width,&height)&&width>=64&&height>=64&&width<=4096&&height<=4096)
                setSize(juce::roundToInt(width/scale),juce::roundToInt(height/scale));
            if (!container.getHWND())
            {
                auto hwnd=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
                    0,0,1,1,(HWND)getPeer()->getNativeHandle(),nullptr,GetModuleHandleW(nullptr),nullptr);
                if(!hwnd)return;
                container.setHWND(hwnd);container.setBounds(getLocalBounds());container.updateHWNDBounds();
            }
            clap_window_t window {}; window.api = CLAP_WINDOW_API_WIN32; window.win32 = container.getHWND();
            attached = owner.nativeGui()->set_parent(owner.nativePlugin(), &window);
            if (attached) owner.nativeGui()->show(owner.nativePlugin());
        }
    }
    void timerCallback() override
    {
        attach();
        int w = owner.requestedWidth.exchange(0), h = owner.requestedHeight.exchange(0);
        if (w > 0 && h > 0)
        {
            // The plugin has already applied an accepted request_resize. Calling
            // set_size back here can trigger a resize loop in native editors.
            setSize(juce::roundToInt(w/scale),juce::roundToInt(h/scale));
        }
    }
    ClapProcessor& owner;
    juce::HWNDComponent container;
    std::unique_ptr<juce::GenericAudioProcessorEditor> fallback;
    bool created = false, attached = false;
    double scale=1.0;
};
}
bool ClapProcessor::hasEditor() const { return gui && gui->is_api_supported(plugin, CLAP_WINDOW_API_WIN32, false); }
juce::AudioProcessorEditor* ClapProcessor::createEditor() { return hasEditor() ? static_cast<juce::AudioProcessorEditor*>(new ClapEditor(*this)) : nullptr; }
