
#include "Render.h"

namespace vcp {

Render::Render (AudioFormatManager& f) : formats (f), thread("vcprt") { }
Render::~Render() { thread.stopThread (2 * 1000); }

void Render::reset()
{
    frame       = 0;
    layer       = 0;
}

void Render::prepare (double sr, int block)
{
    if (prepared)
        release();
    jassert (! prepared);
    reset();
    sampleRate  = sr;
    blockSize   = block;
    thread.startThread();
    prepared = true;
}

void Render::process (int nframes)
{
    if (renderingRequest.get() != rendering.get())
    {
        rendering.set (renderingRequest.get());
        if (isRendering())
        {
            DBG("[VCP] rendering started");
            reset();
        }
        else
        {
            DBG("[VCP] rendering stopped");
            triggerAsyncUpdate();
        }
    }

    if (! isRendering())
        return;

    if (! context.layerEnabled [layer])
    {
        frame = 0;
        event = 0;
        ++layer;
        return;
    }

    if (layer >= 4)
    {
        renderingRequest.compareAndSetBool (0, 1);
        return;
    }

    auto* const detail  = details.getUnchecked (layer);
    const auto& midi    = detail->sequence;
    const int numEvents = midi.getNumEvents();
    const double start  = static_cast<double> (frame);
    const int64 endFrame  = frame + nframes;
    const double end    = static_cast<double> (endFrame);
    bool layerChanged   = false;
    int i;
    for (i = midi.getNextIndexAtTime (start); i < numEvents;)
    {
        const auto* const ev = midi.getEventPointer (i);
        const auto& msg = ev->message;
        const double timestamp = msg.getTimeStamp();
        const int localFrame = roundToInt (timestamp - start);
        if (timestamp >= end)
            break;
        
        if (msg.isNoteOn())
        {
            DBG("note on: " << MidiMessage::getMidiNoteName (msg.getNoteNumber(), true, true, 4) << " - "
                << static_cast<int64> (msg.getTimeStamp()));
            DBG("local frame: " << localFrame);
        }
        else if (msg.isNoteOff())
        {
            DBG("note off: " << MidiMessage::getMidiNoteName (msg.getNoteNumber(), true, true, 4) << " - "
                << static_cast<int64> (msg.getTimeStamp()));
            DBG("local frame: " << localFrame);
        }

        ++i;
    }

    const auto lastStop = detail->getHighestEndFrame();
    const int numDetails = detail->getNumRenderLayers();
    AudioSampleBuffer dummy (2, nframes);
    dummy.clear();
    for (int i = detail->getNextRenderLayerIndex (frame); i < numDetails;)
    {
        auto* const render = detail->getRenderLayer (i);
        if (render->start >= endFrame)
            break;
   
        if (render->start >= frame && render->start < endFrame)
        {
            // started recording
            DBG("======= last stop: " << lastStop << " ========");
            DBG("======= start writing =======");
            int localFrame = render->start - frame;
            int numSamples = endFrame - render->start;
            render->writer->write (dummy.getArrayOfReadPointers(), numSamples);
        }
        else if (render->stop >= frame && render->stop < endFrame)
        {
            // stop writing
            DBG("======= stop writing " << render->stop << " =======");
            int localFrame = 0;
            int numSamples = render->stop - frame;
            render->writer->write (dummy.getArrayOfReadPointers(), numSamples);
        }
        else if (frame >= render->start && frame < render->stop)
        {
            // in the middle
            // DBG("======= middle =======");
            render->writer->write (dummy.getArrayOfReadPointers(), nframes);
        }

        ++i;
    }
    
    if (lastStop >= frame && lastStop < endFrame)
    {
        layerChanged = true;
        ++layer;
        frame = 0;
        event = 0;
    }

    if (layerChanged)
    {
        DBG("[VCP] layer will change: " << layer);
        return; // bail and don't inc. the frame counter
    }

    frame += nframes;
}

void Render::release()
{
    details.clearQuick (true);
    prepared = false;
}

void Render::handleAsyncUpdate()
{
    if (isRendering())
    {
        jassertfalse;
        return;
    }

    OwnedArray<LayerRenderDetails> old;
    {
        ScopedLock sl (getCallbackLock());
        details.swapWith (old);
    }

    for (auto* detail : old)
        for (auto* frame : detail->frames)
            frame->writer.reset();
}

void Render::start (const RenderContext& newContext)
{
    if (isRendering())
        return;
    
    OwnedArray<LayerRenderDetails> newSeq;
    for (int i = 0; i < 4; ++i)
    {
        if (! newContext.layerEnabled [i])
        {
            newSeq.add (new LayerRenderDetails());
        }
        else
        {
            newSeq.add (context.createLayerRenderDetails (i, sampleRate, formats, thread));
        }
    }
    
    {
        ScopedLock sl (getCallbackLock());
        details.swapWith (newSeq);
        context = newContext;
    }

    if (renderingRequest.compareAndSetBool (1, 0))
    {
        DBG("[VCP] render start requested");
    }

    newSeq.clear();
}

void Render::stop()
{
    if (renderingRequest.compareAndSetBool (0, 1))
    {
        DBG("[VCP] render stop requested");
    }
}

}
