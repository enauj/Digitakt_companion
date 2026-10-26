#include "PluginRack.h"

PluginRack::PluginRack() { formats.addFormat(std::make_unique<juce::VST3PluginFormat>()); }
PluginRack::~PluginRack() { release(); }
std::vector<HostedDescriptor> PluginRack::discover(const juce::File& file)
{
    if (file.hasFileExtension("clap")) return ClapProcessor::discover(file);
    std::vector<HostedDescriptor> result;
    for (auto* format : formats.getFormats())
    {
        juce::OwnedArray<juce::PluginDescription> found;
        format->findAllTypesForFile(found, file.getFullPathName());
        for (auto* d : found)
        {
            HostedDescriptor item; item.name = d->name; item.format = "VST3"; item.path = file.getFullPathName();
            item.id = d->createIdentifierString(); item.vst = *d; item.instrument = d->isInstrument; result.push_back(item);
        }
    }
    return result;
}
void PluginRack::Slot::prepare(double rate, int frames)
{
    if (!plugin) return;
    plugin->setRateAndBufferSizeDetails(rate, frames);
    plugin->prepareToPlay(rate, frames);
    dry.setSize(2, frames);
    delaySamples = juce::jlimit(0, 192000, plugin->getLatencySamples());
    delay.setSize(2, juce::jmax(1, delaySamples)); delay.clear(); delayPosition = 0;
}
juce::String PluginRack::load(int slot, const HostedDescriptor& description, const juce::MemoryBlock* saved)
{
    if (!juce::isPositiveAndBelow(slot, 3)) return "Invalid slot";
    try
    {
        std::shared_ptr<juce::AudioProcessor> plugin;
        if (description.format == "CLAP") plugin = std::make_shared<ClapProcessor>(description);
        else if (description.format == "VST3")
        {
            juce::String error;
            plugin = formats.createPluginInstance(description.vst, sampleRate, blockSize, error);
            if (!plugin) return error.isEmpty() ? "VST3 could not be loaded" : error;
        }
        else return "Choose a VST3 or CLAP plugin";
        auto layout = plugin->getBusesLayout();
        for (int i = 1; i < layout.inputBuses.size(); ++i) layout.inputBuses.getReference(i) = juce::AudioChannelSet::disabled();
        for (int i = 1; i < layout.outputBuses.size(); ++i) layout.outputBuses.getReference(i) = juce::AudioChannelSet::disabled();
        if (!layout.outputBuses.isEmpty()) layout.outputBuses.getReference(0) = juce::AudioChannelSet::stereo();
        if (!layout.inputBuses.isEmpty()) layout.inputBuses.getReference(0) = description.instrument ? juce::AudioChannelSet::disabled() : juce::AudioChannelSet::stereo();
        plugin->setBusesLayout(layout);
        if (plugin->getTotalNumOutputChannels() > 2 || plugin->getTotalNumInputChannels() > 2 || plugin->getTotalNumOutputChannels() < 1)
            return "This preview hosts mono/stereo instruments and effects";
        if (slot > 0 && (description.instrument || plugin->getTotalNumInputChannels() == 0)) return "Choose an audio effect for FX slots";
        if (slot == 0 && !description.instrument && plugin->getTotalNumInputChannels() > 0) return "Choose an instrument for the source slot";
        if (saved && saved->getSize() > 0) plugin->setStateInformation(saved->getData(), (int)saved->getSize());
        Slot next; next.plugin = std::move(plugin); next.descriptor = description; next.prepare(sampleRate, blockSize);
        const juce::ScopedLock lock(mutex);
        if (slots[(size_t)slot].plugin) slots[(size_t)slot].plugin->releaseResources();
        slots[(size_t)slot] = std::move(next);
        return {};
    }
    catch (const std::exception& e) { return juce::String(e.what()); }
}
void PluginRack::remove(int slot)
{
    const juce::ScopedLock lock(mutex);
    if (!juce::isPositiveAndBelow(slot, 3)) return;
    auto& s = slots[(size_t)slot]; if (s.plugin) s.plugin->releaseResources(); s = Slot();
}
void PluginRack::prepare(double rate, int frames)
{
    const juce::ScopedLock lock(mutex);
    sampleRate = rate; blockSize = juce::jmax(32, frames);
    for (auto& s : slots) s.prepare(rate, blockSize);
}
void PluginRack::release()
{
    const juce::ScopedLock lock(mutex);
    for (auto& s : slots) if (s.plugin) s.plugin->releaseResources();
}
void PluginRack::panic()
{
    const juce::CriticalSection::ScopedTryLockType lock(mutex);
    if (lock.isLocked()) for (auto& s : slots) if (s.plugin) s.plugin->reset();
}
void PluginRack::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool hostedSource, const std::array<float, 8>& mix, juce::AudioPlayHead* playHead, const std::array<float,8>& fxOffsets)
{
    const juce::CriticalSection::ScopedTryLockType lock(mutex);
    if (!lock.isLocked()) { buffer.clear(); return; }
    if (hostedSource)
    {
        buffer.clear();
        if (auto& p = slots[0].plugin) { p->setPlayHead(playHead); p->processBlock(buffer, midi); if(p->getTotalNumOutputChannels()==1)buffer.copyFrom(1,0,buffer,0,0,buffer.getNumSamples()); }
    }
    buffer.applyGain(mix[0] * 1.5f);
    for (int i = 1; i < 3; ++i)
    {
        auto& s = slots[(size_t)i];
        if (!s.plugin || mix[(size_t)(i + 4)] > .5f || buffer.getNumSamples() > s.dry.getNumSamples()) continue;
        for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
        {
            for (int c = 0; c < 2; ++c)
            {
                auto value = buffer.getSample(c, frame);
                if (s.delaySamples > 0)
                {
                    auto delayed = s.delay.getSample(c, s.delayPosition);
                    s.delay.setSample(c, s.delayPosition, value); value = delayed;
                }
                s.dry.setSample(c, frame, value);
            }
            if (s.delaySamples > 0) s.delayPosition = (s.delayPosition + 1) % s.delaySamples;
        }
        // Temporarily apply MSEG offsets to the same four parameters shown on each
        // FX row. Restore the base without emitting automation/gesture notifications.
        std::array<float,4> base {};std::array<juce::AudioProcessorParameter*,4> changed {};
        for(int k=0;k<juce::jmin(4,s.plugin->getParameters().size());++k)
        {
            auto* p=s.plugin->getParameters()[k];float offset=fxOffsets[(size_t)(i-1)*4+(size_t)k];
            if(offset!=0&&p->isAutomatable()){base[(size_t)k]=p->getValue();changed[(size_t)k]=p;p->setValue(juce::jlimit(0.f,1.f,base[(size_t)k]+offset));}
        }
        s.plugin->setPlayHead(playHead); s.plugin->processBlock(buffer, midi);
        for(int k=0;k<4;++k)if(changed[(size_t)k])changed[(size_t)k]->setValue(base[(size_t)k]);
        if(s.plugin->getTotalNumOutputChannels()==1)buffer.copyFrom(1,0,buffer,0,0,buffer.getNumSamples());
        float wet = mix[(size_t)i];
        buffer.applyGain(wet);
        for (int c = 0; c < 2; ++c) buffer.addFrom(c, 0, s.dry, c, 0, buffer.getNumSamples(), 1.f - wet);
    }
    float pan = juce::jlimit(0.f, 1.f, mix[4]);
    buffer.applyGain(0, 0, buffer.getNumSamples(), mix[3] * juce::jmin(1.f, 2.f * (1.f - pan)));
    buffer.applyGain(1, 0, buffer.getNumSamples(), mix[3] * juce::jmin(1.f, 2.f * pan));
}
std::shared_ptr<juce::AudioProcessor> PluginRack::instance(int slot) const
{
    const juce::ScopedLock lock(mutex);
    return juce::isPositiveAndBelow(slot, 3) ? slots[(size_t)slot].plugin : nullptr;
}
juce::String PluginRack::name(int slot) const { const juce::ScopedLock lock(mutex); if(!juce::isPositiveAndBelow(slot,3))return "EMPTY"; auto& s=slots[(size_t)slot]; return s.plugin?s.plugin->getName():s.descriptor.path.isNotEmpty()?"MISSING "+s.descriptor.name:"EMPTY"; }
juce::String PluginRack::parameterID(int slot, int index) const
{
    auto p = instance(slot); if (!p || !juce::isPositiveAndBelow(index, p->getParameters().size())) return {};
    auto* param = p->getParameters()[index];
    if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*>(param)) return hosted->getParameterID();
    if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) return withId->paramID;
    return juce::String(index);
}
void PluginRack::control(int slot, int index, float value, bool realtime)
{
    if (realtime && !mutex.tryEnter()) return;
    if (!realtime) mutex.enter();
    if (juce::isPositiveAndBelow(slot, 3))
        if (auto& p = slots[(size_t)slot].plugin)
            if (juce::isPositiveAndBelow(index, p->getParameters().size())) p->getParameters()[index]->setValueNotifyingHost(value);
    mutex.exit();
}
int PluginRack::parameterIndex(int slot, const juce::String& id) const
{
    auto p = instance(slot); if (!p) return -1;
    for (int i = 0; i < p->getParameters().size(); ++i) if (parameterID(slot, i) == id) return i;
    return -1;
}
int PluginRack::latency(bool hostedSource, const std::array<float, 8>& mix) const
{
    const juce::ScopedLock lock(mutex); int result = 0;
    for (int i = hostedSource ? 0 : 1; i < 3; ++i) if (slots[(size_t)i].plugin && (i == 0 || mix[(size_t)(i + 4)] <= .5f)) result += slots[(size_t)i].plugin->getLatencySamples();
    return result;
}
juce::var PluginRack::save() const
{
    const juce::ScopedLock lock(mutex); juce::Array<juce::var> result;
    for (const auto& s : slots)
    {
        if (!s.plugin && s.descriptor.path.isEmpty()) { result.add(juce::var()); continue; }
        auto state = s.descriptor.toJson(); juce::MemoryBlock data = s.missingState; if(s.plugin)s.plugin->getStateInformation(data);
        state.getDynamicObject()->setProperty("state", data.toBase64Encoding()); result.add(state);
    }
    return result;
}
juce::String PluginRack::restore(const juce::var& state)
{
    juce::String errors;
    if (auto* array = state.getArray()) if (array->size() == 3)
        for (int i = 0; i < 3; ++i)
        {
            remove(i);
            if (!(*array)[i].isObject()) continue;
            auto description = HostedDescriptor::fromJson((*array)[i]);
            juce::MemoryBlock data; data.fromBase64Encoding((*array)[i].getProperty("state", {}).toString());
            auto error = load(i, description, &data);
            if (error.isNotEmpty()) { const juce::ScopedLock lock(mutex); slots[(size_t)i].descriptor=description; slots[(size_t)i].missingState=data; errors += description.name + ": " + error + "\n"; }
        }
    return errors;
}
