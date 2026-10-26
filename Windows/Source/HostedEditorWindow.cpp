#include "HostedEditorWindow.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

HostedEditorWindow::HostedEditorWindow(std::shared_ptr<juce::AudioProcessor> p, juce::Component* owner)
    : DocumentWindow(p->getName(), juce::Colours::black, closeButton), plugin(std::move(p)), ownerComponent(owner)
{
    setUsingNativeTitleBar(true);
    setResizable(false, false);
    editor = plugin->createEditorIfNeeded();
    if (!editor) editor = new juce::GenericAudioProcessorEditor(*plugin);
    view.setViewedComponent(editor, true);
    view.setScrollBarsShown(false, false);
    setContentNonOwned(&view, false);
    editor->addComponentListener(this);
    fitEditor();
    if (owner) centreAroundComponent(owner, getWidth(), getHeight());
    else centreWithSize(getWidth(), getHeight());
    addToDesktop();
    if (owner && owner->getPeer() && getPeer())
        SetWindowLongPtrW((HWND)getPeer()->getNativeHandle(), GWLP_HWNDPARENT,
                         (LONG_PTR)owner->getPeer()->getNativeHandle());
    setVisible(true);
    fitEditor();
    toFront(true);
}
HostedEditorWindow::~HostedEditorWindow()
{
    editor->removeComponentListener(this);
    plugin->editorBeingDeleted(editor);
    clearContentComponent();
    view.setViewedComponent(nullptr); // Destroy the editor before releasing its processor.
}
void HostedEditorWindow::fitEditor()
{
    if (fitting) return;
    juce::ScopedValueSetter<bool> guard(fitting, true);
    auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(
        ownerComponent ? ownerComponent->getScreenBounds() : getScreenBounds());
    auto area = display ? display->userArea.reduced(24) : juce::Rectangle<int>(0, 0, 1200, 800);
    int w = juce::jmin(juce::jmax(64, editor->getWidth()), juce::jmax(128, area.getWidth()-20));
    int h = juce::jmin(juce::jmax(64, editor->getHeight()), juce::jmax(128, area.getHeight()-60));
    bool vertical = editor->getHeight() > h, horizontal = editor->getWidth() > w;
    view.setScrollBarsShown(vertical, horizontal);
    setContentComponentSize(w + (vertical ? view.getScrollBarThickness() : 0),
                            h + (horizontal ? view.getScrollBarThickness() : 0));
    if(isVisible())setTopLeftPosition(juce::jlimit(area.getX(),juce::jmax(area.getX(),area.getRight()-getWidth()),getX()),
                                     juce::jlimit(area.getY(),juce::jmax(area.getY(),area.getBottom()-getHeight()),getY()));
}
void HostedEditorWindow::componentMovedOrResized(juce::Component&, bool, bool resized)
{
    if (resized) fitEditor();
}
